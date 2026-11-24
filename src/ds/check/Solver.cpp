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

const std::string* Solver::value(const std::string& owner, const model::Term& term, const Variables& variables,
                                 const Bindings& bindings) const {
  if (term.value) return text(term.text);
  if (term.variable) {
    const int slot = variables.slotOf(term.text);
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
bool Solver::ready(const model::Literal& literal, const Variables& variables, const Bindings& bindings) const {
  const auto known = [&](const model::Term& t) {
    if (!t.variable) return true;
    const int slot = variables.slotOf(t.text);
    return slot >= 0 && bindings[slot] != nullptr;
  };
  if (literal.negated || compares(literal.form)) return std::all_of(literal.terms.begin(), literal.terms.end(), known);
  // name(x) reads what x is bound to: it waits for x, and never binds it (DS-403)
  for (const auto& t : literal.terms)
    if (!t.property.empty() && !known(t)) return false;
  if (literal.form == Form::In || literal.form == Form::Equal) return std::any_of(literal.terms.begin(), literal.terms.end(), known);
  return true;
}

std::vector<Bindings> Solver::solve(const std::string& owner, const std::vector<model::Literal>& body, const Variables& variables,
                                    const Bindings& bindings, bool first) const {
  std::vector<Bindings> out;
  facts_.reading();  // a binding points into the facts: they do not grow while it is made
  solveFrom(owner, body, variables, std::vector<bool>(body.size(), false), bindings, first, out);
  facts_.done();
  return out;
}

void Solver::solveFrom(const std::string& owner, const std::vector<model::Literal>& body, const Variables& variables,
                       std::vector<bool> done, const Bindings& bindings, bool first, std::vector<Bindings>& out_answers) const {
  if (first && !out_answers.empty()) return;
  // the most bound ready literal first: tests prune before generators multiply
  int best = -1;
  std::size_t bestKnown = 0;
  for (std::size_t i = 0; i < body.size(); ++i) {
    if (done[i] || !ready(body[i], variables, bindings)) continue;
    std::size_t known = 0;
    for (const auto& t : body[i].terms) {
      const int slot = t.variable ? variables.slotOf(t.text) : -1;
      known += (!t.variable || (slot >= 0 && bindings[slot])) ? 1 : 0;
    }
    if (body[i].negated) known += body[i].terms.size() + 1;
    if (best < 0 || known > bestKnown) {
      best = static_cast<int>(i);
      bestKnown = known;
    }
  }
  if (best < 0) {
    if (std::find(done.begin(), done.end(), false) == done.end()) out_answers.push_back(bindings);
    return;
  }
  done[best] = true;
  const model::Literal& l = body[best];
  if (l.negated) {
    model::Literal positive = l;
    positive.negated = false;
    if (answers(owner, positive, variables, bindings).empty()) solveFrom(owner, body, variables, done, bindings, first, out_answers);
    return;
  }
  for (const auto& b : answers(owner, l, variables, bindings)) {
    solveFrom(owner, body, variables, done, b, first, out_answers);
    if (first && !out_answers.empty()) return;
  }
}

bool Solver::unify(const std::string& owner, const model::Term& term, const Variables& variables, const std::string* entity,
                   Bindings& out_bindings) const {
  if (const std::string* have = value(owner, term, variables, out_bindings)) return have == entity || *have == *entity;
  out_bindings[variables.slotOf(term.text)] = entity;
  return true;
}

std::vector<Bindings> Solver::answers(const std::string& owner, const model::Literal& l, const Variables& variables,
                                      const Bindings& bindings) const {
  std::vector<Bindings> out;
  // bind the terms to these texts on top of `bindings`, keeping the result when they fit
  const auto offer = [&](std::initializer_list<std::pair<std::size_t, const std::string*>> values) {
    Bindings b = bindings;
    for (const auto& v : values)
      if (!unify(owner, l.terms[v.first], variables, v.second, b)) return;
    out.push_back(std::move(b));
  };
  const std::string* empty = text("");
  const std::string* a = l.terms.empty() ? nullptr : value(owner, l.terms[0], variables, bindings);
  const std::string* z = l.terms.size() > 1 ? value(owner, l.terms[1], variables, bindings) : nullptr;
  const bool hasA = a != nullptr;
  const bool hasZ = z != nullptr;
  switch (l.form) {
    case Form::Relation: {
      const std::multimap<std::string, std::string>* fw = facts_.forward(l.name);
      if (!fw) break;
      if (hasA) {
        const auto range = fw->equal_range(*a);
        for (auto it = range.first; it != range.second; ++it) offer({{1, &it->second}});
      } else if (hasZ) {
        const auto range = facts_.backward(l.name).equal_range(*z);
        for (auto it = range.first; it != range.second; ++it) offer({{0, &it->second}});
      } else {
        for (const auto& pair : *fw) offer({{0, &pair.first}, {1, &pair.second}});
      }
      break;
    }
    case Form::Kind: {
      if (hasA) {
        if (facts_.isOfKind(*a, l.name)) out.push_back(bindings);
        break;
      }
      if (const std::vector<std::string>* ids = facts_.ofKind(l.name))
        for (const auto& id : *ids) offer({{0, &id}});
      break;
    }
    case Form::In: {
      if (hasA) {  // what holds a, outward
        for (const std::string* up = facts_.parentOf(*a); up; up = facts_.parentOf(*up)) offer({{1, up}});
        break;
      }
      std::vector<const std::string*> stack{hasZ ? z : empty};  // what z holds, inward
      while (!stack.empty()) {
        const std::string* at = stack.back();
        stack.pop_back();
        if (const std::vector<std::string>* kids = facts_.childrenOf(*at))
          for (const auto& kid : *kids) {
            offer({{0, &kid}});
            stack.push_back(&kid);
          }
      }
      break;
    }
    case Form::Equal:
      if (hasA && hasZ) {
        if (compare(Form::Equal, *a, *z)) out.push_back(bindings);
      } else if (hasA) {
        offer({{1, a}});
      } else {
        offer({{0, hasZ ? z : empty}});
      }
      break;
    case Form::NotEqual:
    case Form::Less:
    case Form::LessEqual:
    case Form::Greater:
    case Form::GreaterEqual:
      if (compare(l.form, hasA ? *a : *empty, hasZ ? *z : *empty)) out.push_back(bindings);
      break;
    case Form::Matches: {  // the value, as it reads, against the pattern the validator compiled (DS-403)
      const std::regex* pattern = facts_.patternOf(owner, l.terms[1].text);
      if (!pattern || !hasA) break;
      const bool quoted = a->size() >= 2 && a->front() == '"' && a->back() == '"';
      const std::string subject = quoted ? a->substr(1, a->size() - 2) : *a;
      if (std::regex_search(subject, *pattern)) out.push_back(bindings);
      break;
    }
    case Form::Flag:  // `m is abstract`: a flag of what m is bound to (DS-403)
      if (hasA && facts_.flagged(*a, l.name)) out.push_back(bindings);
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
          if (passed.first == l.name && literal(passed.second)) offer({{0, call}, {1, text(passed.second)}});
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
        if (const std::string* v = value(owner, l.terms[i], variables, bindings)) fixed.push_back({i, v});
      const bool byFirst = !fixed.empty() && fixed.front().first == 0;
      auto tuple = byFirst ? ext->lower_bound({*fixed.front().second}) : ext->begin();
      for (; tuple != ext->end(); ++tuple) {
        if (byFirst && ((*tuple).empty() || (*tuple)[0] != *fixed.front().second)) break;
        if (tuple->size() != l.terms.size()) continue;
        bool fits = true;
        for (std::size_t k = 0; k < fixed.size() && fits; ++k) fits = (*tuple)[fixed[k].first] == *fixed[k].second;
        if (!fits) continue;
        Bindings b = bindings;
        for (std::size_t i = 0; i < tuple->size() && fits; ++i) fits = unify(owner, l.terms[i], variables, &(*tuple)[i], b);
        if (fits) out.push_back(std::move(b));
      }
      break;
    }
  }
  return out;
}

}  // namespace check
}  // namespace ds
