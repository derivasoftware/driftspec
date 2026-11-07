#include "ds/check/Evaluator.hpp"

#include <algorithm>
#include <functional>

#include "ds/check/Checker.hpp"
#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

namespace {

using Form = model::LiteralForm;
using Bindings = std::map<std::string, std::string>;

std::string key(const std::string& owner, const std::string& written) { return owner + "\n" + written; }

bool isKindName(const std::string& name) {
  for (const auto& spec : model::kinds())
    if (name == model::kindName(spec.kind)) return true;
  return false;
}

std::string describe(const std::map<std::string, std::string>& bindings) {
  std::string out;
  for (const auto& b : bindings) out += (out.empty() ? "" : ", ") + b.first + " = " + b.second;
  return out;
}

const std::size_t kReportsPerRule = 10;

}  // namespace

Evaluator::Evaluator(model::Model& model, const Checker& checker) : model_(model), checker_(checker) {}

void Evaluator::run() {
  if (model_.formulas().empty()) return;
  index();
  for (const auto& f : model_.formulas())
    if (!validate(f.first, f.second)) invalid_.insert(f.first);
  stratify();
  computeDefinitions();
  for (const auto& f : model_.formulas()) {
    const model::Entity* e = model_.find(f.first);
    if (e && e->kind == model::Kind::Rule && !invalid_.count(f.first)) evaluateRule(f.first, f.second);
  }
}

// What holds what (contains, else the declared scope), the entities of each kind and every relation
// both ways: the facts a rule is evaluated over.
void Evaluator::index() {
  for (const auto& r : model_.relations()) {
    forward_[r.kind].insert({r.from, r.to});
    backward_[r.kind].insert({r.to, r.from});
    if (r.kind == "contains" && !parent_.count(r.to)) {
      parent_[r.to] = r.from;
      children_[r.from].push_back(r.to);
    }
  }
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    byKind_[model::kindName(e.kind)].push_back(e.id);
    if (!parent_.count(e.id) && !e.scope.empty() && e.scope != e.id && model_.find(e.scope)) {
      parent_[e.id] = e.scope;
      children_[e.scope].push_back(e.id);
    }
  }
}

std::string Evaluator::scopeOf(const std::string& owner) const {
  const model::Entity* e = model_.find(owner);
  return e ? e->scope : "";
}

void Evaluator::report(const std::string& code, const std::string& owner, const std::string& message, const model::Location& at) {
  model_.report({code, "error", owner + " " + message, at});
}

// Everything a formula names exists in the metamodel or the model, every definition it applies takes
// that many arguments, and every variable is bound by a positive literal before anything tests it.
bool Evaluator::validate(const std::string& id, const model::Formula& formula) {
  const model::Entity* e = model_.find(id);
  if (!e) return false;
  bool ok = true;
  std::set<std::string> bound;
  for (const auto& range : formula.ranges) {
    if (!isKindName(range.kind)) {
      report("E-RULE-UNKNOWN", id, "ranges over '" + range.kind + "', which is no kind of the language", range.location);
      ok = false;
    }
    if (!range.scope.empty()) {
      const std::string scope = checker_.resolve(range.scope, e->scope);
      if (scope.empty()) {
        report("E-RULE-UNKNOWN", id, "ranges inside '" + range.scope + "', which nothing declares", range.location);
        ok = false;
      }
      constants_[key(id, range.scope)] = scope;
    }
    bound.insert(range.variable);
  }
  if (e->kind == model::Kind::Rule) {
    ok = validateBody(id, formula.where, bound) && ok;
    for (const auto& claim : formula.claims) {
      std::set<std::string> inClaim = bound;
      ok = validateBody(id, claim, inClaim) && ok;
    }
  } else {
    for (const auto& c : formula.cases) {
      std::set<std::string> inCase;
      ok = validateBody(id, c, inCase) && ok;
      for (const auto& p : formula.parameters) {
        if (inCase.count(p) || c.empty()) continue;
        report("E-RULE-UNSAFE", id, "leaves its parameter '" + p + "' unbound in one of its cases: name it in a positive literal",
               c.front().location);
        ok = false;
      }
    }
  }
  return ok;
}

bool Evaluator::validateBody(const std::string& id, const std::vector<model::Literal>& body, std::set<std::string>& out_bound) {
  bool ok = true;
  const std::string scope = scopeOf(id);
  for (const auto& l : body) {
    if (l.form == Form::Relation && !model::relationOf(l.name)) {
      report("E-RULE-UNKNOWN", id, "names the relation '" + l.name + "', which the metamodel does not declare", l.location);
      ok = false;
    } else if (l.form == Form::Kind && !isKindName(l.name)) {
      report("E-RULE-UNKNOWN", id, "names the kind '" + l.name + "', which is no kind of the language", l.location);
      ok = false;
    } else if (l.form == Form::Call) {
      const std::string d = checker_.resolve(l.name, scope);
      const model::Entity* def = d.empty() ? nullptr : model_.find(d);
      if (!def || def->kind != model::Kind::Definition) {
        report("E-RULE-UNKNOWN", id, "applies '" + l.name + "', which no definition in scope declares", l.location);
        ok = false;
      } else {
        constants_[key(id, "call:" + l.name)] = d;
        const auto it = model_.formulas().find(d);
        const std::size_t arity = it == model_.formulas().end() ? 0 : it->second.parameters.size();
        if (arity != l.terms.size()) {
          report("E-RULE-ARITY", id,
                 "applies " + d + " to " + std::to_string(l.terms.size()) + " arguments; it takes " + std::to_string(arity), l.location);
          ok = false;
        }
      }
    }
    for (const auto& t : l.terms) {
      if (t.variable) continue;
      const std::string entity = checker_.resolve(t.text, scope);
      if (entity.empty()) {
        report("E-RULE-UNKNOWN", id, "names '" + t.text + "', which nothing declares", l.location);
        ok = false;
      }
      constants_[key(id, t.text)] = entity;
    }
  }
  // safety: what positive literals bind, to a fixpoint; any variable left is never out_bound
  const auto known = [&](const model::Term& t) { return !t.variable || out_bound.count(t.text); };
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& l : body) {
      if (l.negated || l.form == Form::NotEqual) continue;
      const bool binds = l.form == Form::Relation || l.form == Form::Kind || l.form == Form::Call ||
                         std::any_of(l.terms.begin(), l.terms.end(), known);  // in and = bind one side from the other
      if (!binds) continue;
      for (const auto& t : l.terms)
        if (t.variable && out_bound.insert(t.text).second) changed = true;
    }
  }
  std::set<std::string> told;
  for (const auto& l : body)
    for (const auto& t : l.terms)
      if (!known(t) && told.insert(t.text).second) {
        report("E-RULE-UNSAFE", id,
               "leaves the variable '" + t.text + "' unbound: a negation or a comparison tests only what a positive literal binds",
               l.location);
        ok = false;
      }
  return ok;
}

// A definition sits one stratum above every definition it negates; a negation through recursion has
// no stratum and is an error (the evaluation would not be decided).
bool Evaluator::stratify() {
  std::vector<std::string> definitions;
  for (const auto& f : model_.formulas()) {
    const model::Entity* e = model_.find(f.first);
    if (e && e->kind == model::Kind::Definition) {
      definitions.push_back(f.first);
      stratum_[f.first] = 0;
    }
  }
  const int ceiling = static_cast<int>(definitions.size());
  bool ok = true;
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& d : definitions) {
      for (const auto& c : model_.formulas().at(d).cases)
        for (const auto& l : c) {
          if (l.form != Form::Call) continue;
          const std::string callee = definitionOf(d, l.name);
          if (callee.empty()) continue;
          if (invalid_.count(callee) && !invalid_.count(d)) {
            invalid_.insert(d);  // what rests on a broken definition is not evaluated either
            changed = true;
          }
          const int need = stratum_[callee] + (l.negated ? 1 : 0);
          if (stratum_[d] < need && stratum_[d] <= ceiling) {
            stratum_[d] = need;
            changed = true;
          }
        }
    }
  }
  for (const auto& d : definitions)
    if (stratum_[d] > ceiling) {
      const model::Entity* e = model_.find(d);
      report("E-RULE-UNSTRATIFIED", d, "negates itself through recursion: no order of evaluation decides it", e->location);
      invalid_.insert(d);
      ok = false;
    }
  // a rule applying an invalid definition is not evaluated (the definition carries the finding)
  for (const auto& f : model_.formulas()) {
    const auto rests = [&](const std::vector<model::Literal>& body) {
      for (const auto& l : body)
        if (l.form == Form::Call && invalid_.count(definitionOf(f.first, l.name))) return true;
      return false;
    };
    bool broken = rests(f.second.where);
    for (const auto& c : f.second.claims) broken = broken || rests(c);
    if (broken) invalid_.insert(f.first);
  }
  return ok;
}

void Evaluator::computeDefinitions() {
  std::map<int, std::vector<std::string>> strata;
  for (const auto& s : stratum_)
    if (!invalid_.count(s.first)) strata[s.second].push_back(s.first);
  for (const auto& level : strata) {
    for (bool changed = true; changed;) {
      changed = false;
      for (const auto& d : level.second) {
        const model::Formula& f = model_.formulas().at(d);
        for (const auto& c : f.cases) {
          for (const auto& b : solve(d, c, {}, false)) {
            std::vector<std::string> t;
            for (const auto& p : f.parameters) t.push_back(b.at(p));
            if (extension_[d].insert(t).second) changed = true;
          }
        }
      }
    }
  }
}

void Evaluator::evaluateRule(const std::string& id, const model::Formula& formula) {
  if (formula.claims.size() != 1) return;  // the checker says so (W-RULE-NO-CLAIM)
  const model::Entity* rule = model_.find(id);
  const auto& claim = formula.claims.front();
  std::set<Bindings> subjects;
  // every binding of the ranges, then of where
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (const auto& w : solve(id, formula.where, b, false)) subjects.insert(w);
      return;
    }
    const model::Range& r = formula.ranges[i];
    const std::string scope = r.scope.empty() ? "" : constants_[key(id, r.scope)];
    const auto it = byKind_.find(r.kind);
    if (it == byKind_.end()) return;
    for (const auto& entity : it->second) {
      if (!scope.empty() && !contains(scope, entity)) continue;
      const auto already = b.find(r.variable);
      if (already != b.end() && already->second != entity) continue;
      Bindings next = b;
      next[r.variable] = entity;
      range(i + 1, next);
    }
  };
  range(0, {});
  std::size_t broken = 0;
  for (const auto& subject : subjects) {
    Bindings witness;
    const auto found = solve(id, claim, subject, true);
    const bool answered = !found.empty();
    if (answered)
      for (const auto& v : found.front())
        if (!subject.count(v.first)) witness.insert(v);
    if (answered != formula.never) continue;
    if (++broken > kReportsPerRule) continue;
    std::string message = "is broken";
    if (!subject.empty()) message += " for " + describe(subject);
    if (formula.never && !witness.empty()) message += ": " + describe(witness);
    report("E-RULE-BROKEN", id, message, rule->location);
  }
  if (broken > kReportsPerRule)
    report("E-RULE-BROKEN", id, "is broken " + std::to_string(broken - kReportsPerRule) + " more times", rule->location);
}

bool Evaluator::value(const std::string& owner, const model::Term& term, const Bindings& bindings, std::string& out_value) const {
  if (term.variable) {
    const auto it = bindings.find(term.text);
    if (it == bindings.end()) return false;
    out_value = it->second;
    return true;
  }
  const auto it = constants_.find(key(owner, term.text));
  out_value = it == constants_.end() ? "" : it->second;
  return true;
}

std::string Evaluator::definitionOf(const std::string& owner, const std::string& name) const {
  const auto it = constants_.find(key(owner, "call:" + name));
  return it == constants_.end() ? "" : it->second;
}

bool Evaluator::contains(const std::string& outer, const std::string& inner) const {
  for (auto it = parent_.find(inner); it != parent_.end(); it = parent_.find(it->second))
    if (it->second == outer) return true;
  return false;
}

// a literal can be answered once what it tests is bound: a negation or != needs every term, in and =
// one side; a relation, a kind or a definition generates what it does not have
bool Evaluator::ready(const model::Literal& literal, const Bindings& bindings) const {
  const auto known = [&](const model::Term& t) { return !t.variable || bindings.count(t.text); };
  if (literal.negated || literal.form == Form::NotEqual) return std::all_of(literal.terms.begin(), literal.terms.end(), known);
  if (literal.form == Form::In || literal.form == Form::Equal) return std::any_of(literal.terms.begin(), literal.terms.end(), known);
  return true;
}

std::vector<Bindings> Evaluator::solve(const std::string& owner, const std::vector<model::Literal>& body, const Bindings& bindings,
                                       bool first) const {
  std::vector<Bindings> out;
  solveFrom(owner, body, std::vector<bool>(body.size(), false), bindings, first, out);
  return out;
}

void Evaluator::solveFrom(const std::string& owner, const std::vector<model::Literal>& body, std::vector<bool> done,
                          const Bindings& bindings, bool first, std::vector<Bindings>& out_answers) const {
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

bool Evaluator::unify(const std::string& owner, const model::Term& term, const std::string& entity, Bindings& out_bindings) const {
  std::string have;
  if (value(owner, term, out_bindings, have)) return have == entity;
  out_bindings[term.text] = entity;
  return true;
}

std::vector<Bindings> Evaluator::answers(const std::string& owner, const model::Literal& l, const Bindings& bindings) const {
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
      const auto fw = forward_.find(l.name);
      if (fw == forward_.end()) break;
      if (hasA) {
        const auto range = fw->second.equal_range(a);
        for (auto it = range.first; it != range.second; ++it) offer({{1, it->second}});
      } else if (hasZ) {
        const auto range = backward_.at(l.name).equal_range(z);
        for (auto it = range.first; it != range.second; ++it) offer({{0, it->second}});
      } else {
        for (const auto& pair : fw->second) offer({{0, pair.first}, {1, pair.second}});
      }
      break;
    }
    case Form::Kind: {
      if (hasA) {
        const model::Entity* e = model_.find(a);
        if (e && !e->expected && l.name == model::kindName(e->kind)) out.push_back(bindings);
        break;
      }
      const auto it = byKind_.find(l.name);
      if (it != byKind_.end())
        for (const auto& id : it->second) offer({{0, id}});
      break;
    }
    case Form::In: {
      if (hasA) {  // what holds a, outward
        for (auto it = parent_.find(a); it != parent_.end(); it = parent_.find(it->second)) offer({{1, it->second}});
        break;
      }
      std::vector<std::string> stack{z};  // what z holds, inward
      while (!stack.empty()) {
        const std::string at = stack.back();
        stack.pop_back();
        const auto kids = children_.find(at);
        if (kids == children_.end()) continue;
        for (const auto& kid : kids->second) {
          offer({{0, kid}});
          stack.push_back(kid);
        }
      }
      break;
    }
    case Form::Equal:
      if (hasA)
        offer({{1, a}});
      else
        offer({{0, z}});
      break;
    case Form::NotEqual:
      if (a != z) out.push_back(bindings);
      break;
    case Form::Call: {
      const auto ext = extension_.find(definitionOf(owner, l.name));
      if (ext == extension_.end()) break;
      for (const auto& tuple : ext->second) {
        Bindings b = bindings;
        bool fits = tuple.size() == l.terms.size();
        for (std::size_t i = 0; i < tuple.size() && fits; ++i) fits = unify(owner, l.terms[i], tuple[i], b);
        if (fits) out.push_back(b);
      }
      break;
    }
  }
  return out;
}

}  // namespace check
}  // namespace ds
