#include "ds/check/Solver.hpp"

#include <algorithm>
#include <cstdlib>

namespace ds {
namespace check {

using Form = model::LiteralForm;
using Bindings = std::map<std::string, std::string>;

bool Solver::compares(Form form) {
  return form == Form::NotEqual || form == Form::Less || form == Form::LessEqual || form == Form::Greater || form == Form::GreaterEqual;
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

bool Solver::value(const std::string& owner, const model::Term& term, const Bindings& bindings, std::string& out_value) const {
  if (term.value) {
    out_value = term.text;
    return true;
  }
  if (term.variable) {
    const auto it = bindings.find(term.text);
    if (it == bindings.end()) return false;
    out_value = it->second;
    if (!term.field.empty()) {  // a field of what it is bound to (DS-268)
      out_value = facts_.fieldOf(it->second, term.field);
      double ignored = 0;
      if (!number(out_value, ignored)) out_value = "\"" + out_value + "\"";  // a text compares with a string as written
    }
    return true;
  }
  if (!facts_.constant(owner, term.text, out_value)) out_value.clear();
  return true;
}

// a literal can be answered once what it tests is bound: a negation or != needs every term, in and =
// one side; a relation, a kind or a definition generates what it does not have
bool Solver::ready(const model::Literal& literal, const Bindings& bindings) const {
  const auto known = [&](const model::Term& t) { return !t.variable || bindings.count(t.text); };
  if (literal.negated || compares(literal.form)) return std::all_of(literal.terms.begin(), literal.terms.end(), known);
  if (literal.form == Form::In || literal.form == Form::Equal) return std::any_of(literal.terms.begin(), literal.terms.end(), known);
  return true;
}

std::vector<Bindings> Solver::solve(const std::string& owner, const std::vector<model::Literal>& body, const Bindings& bindings,
                                    bool first) const {
  std::vector<Bindings> out;
  solveFrom(owner, body, std::vector<bool>(body.size(), false), bindings, first, out);
  return out;
}

void Solver::solveFrom(const std::string& owner, const std::vector<model::Literal>& body, std::vector<bool> done, const Bindings& bindings,
                       bool first, std::vector<Bindings>& out_answers) const {
  if (first && !out_answers.empty()) return;
  // the most bound ready literal first: tests prune before generators multiply
  int best = -1;
  std::size_t bestKnown = 0;
  for (std::size_t i = 0; i < body.size(); ++i) {
    if (done[i] || !ready(body[i], bindings)) continue;
    std::size_t known = 0;
    for (const auto& t : body[i].terms) known += (!t.variable || bindings.count(t.text)) ? 1 : 0;
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
    if (answers(owner, positive, bindings).empty()) solveFrom(owner, body, done, bindings, first, out_answers);
    return;
  }
  for (const auto& b : answers(owner, l, bindings)) {
    solveFrom(owner, body, done, b, first, out_answers);
    if (first && !out_answers.empty()) return;
  }
}

bool Solver::unify(const std::string& owner, const model::Term& term, const std::string& entity, Bindings& out_bindings) const {
  std::string have;
  if (value(owner, term, out_bindings, have)) return have == entity;
  out_bindings[term.text] = entity;
  return true;
}

std::vector<Bindings> Solver::answers(const std::string& owner, const model::Literal& l, const Bindings& bindings) const {
  std::vector<Bindings> out;
  // bind the terms to these entities on top of `bindings`, keeping the result when they fit
  const auto offer = [&](std::initializer_list<std::pair<std::size_t, std::string>> values) {
    Bindings b = bindings;
    for (const auto& v : values)
      if (!unify(owner, l.terms[v.first], v.second, b)) return;
    out.push_back(b);
  };
  std::string a, z;
  const bool hasA = !l.terms.empty() && value(owner, l.terms[0], bindings, a);
  const bool hasZ = l.terms.size() > 1 && value(owner, l.terms[1], bindings, z);
  switch (l.form) {
    case Form::Relation: {
      const std::multimap<std::string, std::string>* fw = facts_.forward(l.name);
      if (!fw) break;
      if (hasA) {
        const auto range = fw->equal_range(a);
        for (auto it = range.first; it != range.second; ++it) offer({{1, it->second}});
      } else if (hasZ) {
        const auto range = facts_.backward(l.name).equal_range(z);
        for (auto it = range.first; it != range.second; ++it) offer({{0, it->second}});
      } else {
        for (const auto& pair : *fw) offer({{0, pair.first}, {1, pair.second}});
      }
      break;
    }
    case Form::Kind: {
      if (hasA) {
        if (facts_.isOfKind(a, l.name)) out.push_back(bindings);
        break;
      }
      if (const std::vector<std::string>* ids = facts_.ofKind(l.name))
        for (const auto& id : *ids) offer({{0, id}});
      break;
    }
    case Form::In: {
      if (hasA) {  // what holds a, outward
        for (std::string up = facts_.parentOf(a); !up.empty(); up = facts_.parentOf(up)) offer({{1, up}});
        break;
      }
      std::vector<std::string> stack{z};  // what z holds, inward
      while (!stack.empty()) {
        const std::string at = stack.back();
        stack.pop_back();
        for (const auto& kid : facts_.childrenOf(at)) {
          offer({{0, kid}});
          stack.push_back(kid);
        }
      }
      break;
    }
    case Form::Equal:
      if (hasA && hasZ) {
        if (compare(Form::Equal, a, z)) out.push_back(bindings);
      } else if (hasA) {
        offer({{1, a}});
      } else {
        offer({{0, z}});
      }
      break;
    case Form::NotEqual:
    case Form::Less:
    case Form::LessEqual:
    case Form::Greater:
    case Form::GreaterEqual:
      if (compare(l.form, a, z)) out.push_back(bindings);
      break;
    case Form::Arg: {  // the literal a call passes to the parameter named, as the sequence writes it
      std::vector<std::string> calls;
      if (hasA) {
        calls.push_back(a);
      } else if (const std::vector<std::string>* messages = facts_.ofKind("message")) {
        calls = *messages;
      }
      for (const auto& call : calls)
        for (const auto& passed : facts_.argsTo(call))
          if (passed.first == l.name && literal(passed.second)) offer({{0, call}, {1, passed.second}});
      break;
    }
    case Form::Call: {
      const std::set<std::vector<std::string>>* ext = facts_.derived(facts_.definitionOf(owner, l.name));
      if (!ext) break;
      // what the bindings already fix is tested before anything is copied, and a fixed first argument visits only
      // the tuples that begin with it: they lie together in the ordered extension, in the order a full pass meets
      // them (DS-338)
      std::vector<std::pair<std::size_t, std::string>> fixed;
      for (std::size_t i = 0; i < l.terms.size(); ++i) {
        std::string v;
        if (value(owner, l.terms[i], bindings, v)) fixed.push_back({i, v});
      }
      const bool byFirst = !fixed.empty() && fixed.front().first == 0;
      auto tuple = byFirst ? ext->lower_bound({fixed.front().second}) : ext->begin();
      for (; tuple != ext->end(); ++tuple) {
        if (byFirst && ((*tuple).empty() || (*tuple)[0] != fixed.front().second)) break;
        if (tuple->size() != l.terms.size()) continue;
        bool fits = true;
        for (std::size_t k = 0; k < fixed.size() && fits; ++k) fits = (*tuple)[fixed[k].first] == fixed[k].second;
        if (!fits) continue;
        Bindings b = bindings;
        for (std::size_t i = 0; i < tuple->size() && fits; ++i) fits = unify(owner, l.terms[i], (*tuple)[i], b);
        if (fits) out.push_back(b);
      }
      break;
    }
  }
  return out;
}

}  // namespace check
}  // namespace ds
