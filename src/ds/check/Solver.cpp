#include "ds/check/Solver.hpp"

#include <algorithm>
#include <cstdlib>
#include <regex>

namespace ds {
namespace check {

using Form = model::LiteralForm;
using Bindings = std::vector<const std::string*>;

bool Solver::compares(Form form) {
  return form == Form::NotEqual || form == Form::Less || form == Form::LessEqual || form == Form::Greater || form == Form::GreaterEqual ||
         form == Form::Matches || form == Form::Flag;
}

bool Solver::number(const std::string& text, double& out_number) {
  if (text.empty()) return false;
  char* end = nullptr;
  out_number = std::strtod(text.c_str(), &end);
  return end == text.c_str() + text.size();
}

bool Solver::literal(const std::string& text) {
  double ignored = 0;
  return number(text, ignored) || (text.size() >= 2 && text.front() == '"' && text.back() == '"');
}

bool Solver::compare(Form form, const std::string& a, const std::string& z) {
  double x = 0, y = 0;
  const bool numbers = number(a, x) && number(z, y);
  switch (form) {
    case Form::Equal:
      return numbers ? x == y : a == z;
    case Form::NotEqual:
      return numbers ? x != y : a != z;
    case Form::Less:
      return numbers && x < y;
    case Form::LessEqual:
      return numbers && x <= y;
    case Form::Greater:
      return numbers && x > y;
    case Form::GreaterEqual:
      return numbers && x >= y;
    default:
      return false;
  }
}

const std::string* Solver::value(const std::string& owner, const model::Term& term, int slot, const Bindings& bindings) const {
  if (term.value) return text(term.text);
  if (term.variable) {
    if (slot < 0 || !bindings[slot]) return nullptr;
    if (term.field.empty() && term.property.empty()) return bindings[slot];
    std::string field = term.property.empty() ? facts_.fieldOf(*bindings[slot], term.field)  // a field of what it is bound to (DS-268)
                                              : facts_.propertyOf(*bindings[slot], term.property);  // what it is (DS-403)
    double ignored = 0;
    if (!number(field, ignored)) field = "\"" + field + "\"";  // a text compares with a string as written
    return text(field);
  }
  const std::string* constant = facts_.constantOf(owner, term.text);
  return constant ? constant : text("");
}

// a literal can be answered once what it tests is bound: a negation or != needs every term, in and =
// one side; a relation, a kind or a definition generates what it does not have
bool Solver::ready(const model::Literal& literal, const std::vector<int>& slots, const Bindings& bindings) {
  const std::size_t n = literal.terms.size();
  const auto known = [&](std::size_t i) {
    if (!literal.terms[i].variable) return true;
    return slots[i] >= 0 && bindings[slots[i]] != nullptr;
  };
  if (literal.negated || compares(literal.form)) {
    for (std::size_t i = 0; i < n; ++i)
      if (!known(i)) return false;
    return true;
  }
  // name(x) reads what x is bound to: it waits for x, and never binds it (DS-403)
  for (std::size_t i = 0; i < n; ++i)
    if (!literal.terms[i].property.empty() && !known(i)) return false;
  if (literal.form == Form::In || literal.form == Form::Equal) {
    for (std::size_t i = 0; i < n; ++i)
      if (known(i)) return true;
    return false;
  }
  return true;
}

std::vector<Bindings> Solver::solve(const std::string& owner, const std::vector<model::Literal>& body, const Variables& variables,
                                    const Bindings& bindings, bool first) const {
  std::vector<Bindings> out;
  facts_.reading();  // a binding points into the facts: they do not grow while it is made
  std::vector<bool> done(body.size(), false);
  Bindings work = bindings;
  solveFrom(owner, body, variables.slotsOf(body), done, work, first, out);  // the body's slots, compiled once (DS-507)
  facts_.done();
  return out;
}

bool Solver::solveFrom(const std::string& owner, const std::vector<model::Literal>& body, const Variables::Slots& slots,
                       std::vector<bool>& done, Bindings& bindings, bool first, std::vector<Bindings>& out_answers) const {
  if (first && !out_answers.empty()) return false;
  // the most bound ready literal first: tests prune before generators multiply
  int best = -1;
  std::size_t bestKnown = 0;
  for (std::size_t i = 0; i < body.size(); ++i) {
    if (done[i] || !ready(body[i], slots[i], bindings)) continue;
    std::size_t known = 0;
    const auto& terms = body[i].terms;
    for (std::size_t k = 0; k < terms.size(); ++k) {
      const int slot = slots[i][k];
      known += (!terms[k].variable || (slot >= 0 && bindings[slot])) ? 1 : 0;
    }
    if (body[i].negated) known += body[i].terms.size() + 1;
    if (best < 0 || known > bestKnown) {
      best = static_cast<int>(i);
      bestKnown = known;
    }
  }
  if (best < 0) {
    if (std::find(done.begin(), done.end(), false) == done.end()) out_answers.push_back(bindings);
    return !(first && !out_answers.empty());
  }
  done[best] = true;
  const model::Literal& l = body[best];
  bool go = true;
  if (l.negated) {
    bool any = false;
    answers(owner, l, slots[best], bindings, [&any]() {
      any = true;
      return false;  // one answer is enough to know it is not empty
    });
    if (!any) go = solveFrom(owner, body, slots, done, bindings, first, out_answers);
  } else {
    go = answers(owner, l, slots[best], bindings, [&]() { return solveFrom(owner, body, slots, done, bindings, first, out_answers); });
  }
  done[best] = false;
  return go;
}

bool Solver::unify(const std::string& owner, const model::Term& term, int slot, const std::string* entity, Bindings& out_bindings) const {
  if (const std::string* have = value(owner, term, slot, out_bindings)) return have == entity || *have == *entity;
  out_bindings[slot] = entity;
  return true;
}

template <class Next>
bool Solver::answers(const std::string& owner, const model::Literal& l, const std::vector<int>& slots, Bindings& bindings,
                     Next&& next) const {
  // bind the terms to these texts in place, continue, and undo what was bound: the slots it filled, no more (DS-401)
  int touched[4];
  const auto offer = [&](std::initializer_list<std::pair<std::size_t, const std::string*>> values) {
    int n = 0;
    bool fits = true;
    for (const auto& v : values) {
      const model::Term& t = l.terms[v.first];
      const int slot = t.variable && t.field.empty() && t.property.empty() ? slots[v.first] : -1;
      const bool open = slot >= 0 && !bindings[slot];
      if (!unify(owner, t, slots[v.first], v.second, bindings)) {
        fits = false;
        break;
      }
      if (open && bindings[slot]) touched[n++] = slot;
    }
    const bool go = fits ? next() : true;
    for (int i = 0; i < n; ++i) bindings[touched[i]] = nullptr;
    return go;
  };
  const std::string* empty = text("");
  const std::string* a = l.terms.empty() ? nullptr : value(owner, l.terms[0], slots[0], bindings);
  const std::string* z = l.terms.size() > 1 ? value(owner, l.terms[1], slots[1], bindings) : nullptr;
  const bool hasA = a != nullptr;
  const bool hasZ = z != nullptr;
  switch (l.form) {
    case Form::Relation: {
      const std::multimap<std::string, std::string>* fw = facts_.forward(l.name);
      if (!fw) break;
      if (hasA) {
        const auto range = fw->equal_range(*a);
        for (auto it = range.first; it != range.second; ++it)
          if (!offer({{1, &it->second}})) return false;
      } else if (hasZ) {
        const auto range = facts_.backward(l.name).equal_range(*z);
        for (auto it = range.first; it != range.second; ++it)
          if (!offer({{0, &it->second}})) return false;
      } else {
        for (const auto& pair : *fw)
          if (!offer({{0, &pair.first}, {1, &pair.second}})) return false;
      }
      break;
    }
    case Form::Kind: {
      if (hasA) {
        if (facts_.isOfKind(*a, l.name))
          if (!next()) return false;
        break;
      }
      if (const std::vector<std::string>* ids = facts_.ofKind(l.name))
        for (const auto& id : *ids)
          if (!offer({{0, &id}})) return false;
      break;
    }
    case Form::In: {
      if (hasA) {  // what holds a, outward
        for (const std::string* up = facts_.parentOf(*a); up; up = facts_.parentOf(*up))
          if (!offer({{1, up}})) return false;
        break;
      }
      std::vector<const std::string*> stack{hasZ ? z : empty};  // what z holds, inward
      while (!stack.empty()) {
        const std::string* at = stack.back();
        stack.pop_back();
        if (const std::vector<std::string>* kids = facts_.childrenOf(*at))
          for (const auto& kid : *kids) {
            if (!offer({{0, &kid}})) return false;
            stack.push_back(&kid);
          }
      }
      break;
    }
    case Form::Equal:
      if (hasA && hasZ) {
        if (compare(Form::Equal, *a, *z))
          if (!next()) return false;
      } else if (hasA) {
        if (!offer({{1, a}})) return false;
      } else {
        if (!offer({{0, hasZ ? z : empty}})) return false;
      }
      break;
    case Form::NotEqual:
    case Form::Less:
    case Form::LessEqual:
    case Form::Greater:
    case Form::GreaterEqual:
      if (compare(l.form, hasA ? *a : *empty, hasZ ? *z : *empty))
        if (!next()) return false;
      break;
    case Form::Matches: {  // the value, as it reads, against the pattern the validator compiled (DS-403)
      const std::regex* pattern = facts_.patternOf(owner, l.terms[1].text);
      if (!pattern || !hasA) break;
      const bool quoted = a->size() >= 2 && a->front() == '"' && a->back() == '"';
      const std::string subject = quoted ? a->substr(1, a->size() - 2) : *a;
      if (std::regex_search(subject, *pattern))
        if (!next()) return false;
      break;
    }
    case Form::Flag:  // `m is abstract`: a flag of what m is bound to (DS-403)
      if (hasA && facts_.flagged(*a, l.name))
        if (!next()) return false;
      break;
    case Form::Arg: {  // the literal a call passes to the parameter named, as the sequence writes it
      std::vector<const std::string*> calls;
      if (hasA) {
        calls.push_back(a);
      } else if (const std::vector<std::string>* messages = facts_.ofKind("message")) {
        for (const auto& m : *messages) calls.push_back(&m);
      }
      for (const std::string* call : calls)
        for (const auto& passed : facts_.argsTo(*call))
          if (passed.first == l.name && literal(passed.second))
            if (!offer({{0, call}, {1, text(passed.second)}})) return false;
      break;
    }
    case Form::Call: {
      const std::string* definition = facts_.constantOf(owner, "call:" + l.name);
      const std::set<std::vector<std::string>>* ext = facts_.derived(definition ? *definition : std::string());
      if (!ext) break;
      // what the bindings already fix is tested before anything is copied, and a fixed first argument visits only
      // the tuples that begin with it: they lie together in the ordered extension, in the order a full pass meets
      // them (DS-338)
      std::vector<std::pair<std::size_t, const std::string*>> fixed;
      for (std::size_t i = 0; i < l.terms.size(); ++i)
        if (const std::string* v = value(owner, l.terms[i], slots[i], bindings)) fixed.push_back({i, v});
      const bool byFirst = !fixed.empty() && fixed.front().first == 0;
      auto tuple = byFirst ? ext->lower_bound({*fixed.front().second}) : ext->begin();
      for (; tuple != ext->end(); ++tuple) {
        if (byFirst && ((*tuple).empty() || (*tuple)[0] != *fixed.front().second)) break;
        if (tuple->size() != l.terms.size()) continue;
        bool fits = true;
        for (std::size_t k = 0; k < fixed.size() && fits; ++k) fits = (*tuple)[fixed[k].first] == *fixed[k].second;
        if (!fits) continue;
        std::vector<int> set;
        for (std::size_t i = 0; i < tuple->size() && fits; ++i) {
          const model::Term& t = l.terms[i];
          const int slot = t.variable && t.field.empty() && t.property.empty() ? slots[i] : -1;
          const bool open = slot >= 0 && !bindings[slot];
          fits = unify(owner, t, slots[i], &(*tuple)[i], bindings);
          if (fits && open && bindings[slot]) set.push_back(slot);
        }
        const bool go = fits ? next() : true;
        for (const int slot : set) bindings[slot] = nullptr;
        if (!go) return false;
      }
      break;
    }
  }
  return true;
}

}  // namespace check
}  // namespace ds
