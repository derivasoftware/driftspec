#include "ds/check/Evaluator.hpp"

#include <functional>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

using Bindings = std::vector<const std::string*>;

Evaluator::Evaluator(model::Model& model, const model::Metamodel& metamodel, const Resolver& resolver)
    : model_(model),
      metamodel_(metamodel),
      facts_(model, metamodel),
      validator_(metamodel, resolver, facts_),
      strata_(facts_),
      solver_(facts_) {}

std::string Evaluator::describe(const std::vector<std::pair<std::string, std::string>>& bindings) {
  std::string out;
  for (const auto& b : bindings) out += (out.empty() ? "" : ", ") + b.first + " = " + b.second;
  return out;
}

void Evaluator::report(model::Code code, const std::string& owner, const std::string& message, const model::Location& at) {
  found_.push_back(metamodel_.finding(code, owner + " " + message, at));
}

std::vector<model::Finding> Evaluator::run() {
  if (facts_.formulas().empty()) return {};
  facts_.index();
  for (const auto& f : facts_.formulas()) variables_.emplace(f.first, Variables(f.second));  // their slots, once (DS-381)
  for (const auto& f : facts_.formulas()) {
    std::vector<std::pair<std::string, std::string>> resolved;
    if (!validator_.validate(f.first, f.second, resolved)) invalid_.insert(f.first);
    for (const auto& r : resolved) facts_.record(f.first, r.first, r.second);                       // the one who directs writes the facts
    for (auto& p : validator_.takePatterns()) facts_.learn(f.first, p.first, std::move(p.second));  // compiled once (DS-403)
    validator_.checkVacuity(f.first, f.second);
  }
  found_ = validator_.take();
  for (const auto& d : strata_.stratify(invalid_))
    report(model::Code::E_RULE_UNSTRATIFIED, d, "negates itself through recursion: no order of evaluation decides it",
           facts_.locationOf(d));
  computeDefinitions();
  for (const auto& f : facts_.formulas())
    if (facts_.kindIs(f.first, model::Kind::Rule) && !invalid_.count(f.first)) evaluateRule(f.first, f.second);
  for (const auto& f : facts_.formulas())  // after the rules: a table may say which of them hold
    if (facts_.kindIs(f.first, model::Kind::Diagram) && !invalid_.count(f.first)) tabulate(f.first, f.second);
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

void Evaluator::computeDefinitions() {
  for (const auto& level : strata_.levels(invalid_)) {
    // a stratum where no definition reaches itself through the others is fixed in one pass, each definition after
    // those of its stratum it applies; one with recursion keeps the passes to its fixpoint (DS-338)
    const std::vector<std::string> order = strata_.dependencyOrder(level.second);
    const bool recursive = order.empty() && !level.second.empty();
    const std::vector<std::string>& definitions = recursive ? level.second : order;
    for (bool changed = true; changed;) {
      changed = false;
      for (const auto& d : definitions) {
        const model::Formula& f = facts_.formulas().at(d);
        const Variables& variables = variables_.at(d);
        for (const auto& c : f.cases) {
          for (const auto& b : solver_.solve(d, c, variables, Bindings(variables.size(), nullptr), false)) {
            std::vector<std::string> t;
            for (const auto& p : f.parameters) t.push_back(*b[variables.slotOf(p)]);
            if (facts_.derive(d, t)) changed = true;
          }
        }
      }
      if (!recursive) break;  // every definition was computed after what it applies: a second pass adds nothing
    }
  }
}

void Evaluator::tabulate(const std::string& id, const model::Formula& formula) {
  const Variables& variables = variables_.at(id);
  const auto order = [&variables](const Bindings& a, const Bindings& b) { return variables.before(a, b); };
  std::set<Bindings, decltype(order)> subjects(order);
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (auto& w : solver_.solve(id, formula.where, variables, b, false)) subjects.insert(std::move(w));
      return;
    }
    const model::Range& r = formula.ranges[i];
    std::string scope;
    if (!r.scope.empty()) facts_.constant(id, r.scope, scope);
    const std::vector<std::string>* entities = facts_.ofKind(r.kind);
    if (!entities) return;
    const int slot = variables.slotOf(r.variable);
    for (const auto& entity : *entities) {
      if (!scope.empty() && !facts_.contains(scope, entity)) continue;
      Bindings next = b;
      next[slot] = &entity;
      range(i + 1, next);
    }
  };
  range(0, Bindings(variables.size(), nullptr));
  model::Tabulated table;
  for (const auto& column : formula.columns) table.columns.push_back(column.name);
  const auto shown = [&](const std::string& entity) {  // an artefact by its id, anything else by its name
    const std::string name = facts_.fieldOf(entity, "name");
    return !name.empty() ? name : entity;
  };
  for (const auto& subject : subjects) {
    std::vector<std::string> cells, kinds;
    for (const auto& column : formula.columns) {
      std::string text, kind;
      if (column.form == "field") {
        const int slot = variables.slotOf(column.variable);
        const bool bound = slot >= 0 && subject[slot];
        const std::string entity = bound ? *subject[slot] : "";
        if (column.field == "holds" && facts_.kindIs(entity, model::Kind::Rule)) {  // a rule: whether the model keeps it
          const bool holds = facts_.holds(entity);
          text = holds ? "holds" : "broken";
          kind = holds ? "" : "against";
        } else if (bound) {
          text = facts_.fieldOf(entity, column.field);
        }
      } else if (column.form == "property") {  // what the entity is, as a rule reads it (DS-437)
        const int slot = variables.slotOf(column.variable);
        if (slot >= 0 && subject[slot]) text = facts_.propertyOf(*subject[slot], column.property);
      } else if (column.form == "related") {
        std::set<std::string> seen;
        const int slot = variables.slotOf(column.variable);
        for (const auto& answer : solver_.solve(id, column.body, variables, subject, false)) {
          if (slot < 0 || !answer[slot] || !seen.insert(*answer[slot]).second) continue;
          text += (text.empty() ? "" : ", ") + shown(*answer[slot]);
        }
      } else {
        const bool yes = !solver_.solve(id, column.body, variables, subject, true).empty();
        text = yes ? "yes" : "no";
        kind = yes ? "" : "against";
      }
      if (text.empty()) kind = "none";
      cells.push_back(text);
      kinds.push_back(kind);
    }
    table.rows.push_back(cells);
    table.kinds.push_back(kinds);
  }
  model_.tabulate(id, table);
}

std::string Evaluator::through(const std::string& owner, const std::vector<model::Literal>& claim, const Variables& variables,
                               const Bindings& answer) const {
  std::string out;
  for (const auto& l : claim) {
    if (l.form != model::LiteralForm::Call || l.negated) continue;
    const std::string definition = facts_.definitionOf(owner, l.name);
    const auto f = facts_.formulas().find(definition);
    const auto v = variables_.find(definition);
    if (f == facts_.formulas().end() || v == variables_.end() || f->second.parameters.size() != l.terms.size()) continue;
    const Variables& inner = v->second;
    Bindings seed(inner.size(), nullptr);  // the definition's parameters, as the answer gives its arguments
    bool known = true;
    for (std::size_t i = 0; i < l.terms.size() && known; ++i) {
      const model::Term& t = l.terms[i];
      const std::string* given = nullptr;
      if (t.variable && t.field.empty() && t.property.empty()) {
        const int slot = variables.slotOf(t.text);
        given = slot < 0 ? nullptr : answer[slot];
      } else if (!t.variable && !t.value) {
        given = facts_.constantOf(owner, t.text);
      }
      const int at = inner.slotOf(f->second.parameters[i]);
      known = given && at >= 0;
      if (known) seed[at] = given;
    }
    if (!known) continue;  // an argument read from a field, or a value: the case is not looked for
    for (const auto& c : f->second.cases) {
      const auto found = solver_.solve(definition, c, inner, seed, true);
      if (found.empty()) continue;
      Bindings extra(inner.size(), nullptr);  // what the case binds beyond the parameters
      for (std::size_t s = 0; s < inner.size(); ++s)
        if (!seed[s]) extra[s] = found.front()[s];
      const auto named = inner.pairs(extra);
      if (!named.empty()) out += " (" + l.name + ": " + describe(named) + ")";
      break;
    }
  }
  return out;
}

void Evaluator::evaluateRule(const std::string& id, const model::Formula& formula) {
  if (formula.claims.size() != 1) return;  // the checker says so (W-RULE-NO-CLAIM)
  const model::Location ruleAt = facts_.locationOf(id);
  const auto& claim = formula.claims.front();
  const Variables& variables = variables_.at(id);
  const auto order = [&variables](const Bindings& a, const Bindings& b) { return variables.before(a, b); };
  std::set<Bindings, decltype(order)> subjects(order);
  // every binding of the ranges, then of where
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (auto& w : solver_.solve(id, formula.where, variables, b, false)) subjects.insert(std::move(w));
      return;
    }
    const model::Range& r = formula.ranges[i];
    std::string scope;
    if (!r.scope.empty()) facts_.constant(id, r.scope, scope);
    const std::vector<std::string>* entities = facts_.ofKind(r.kind);
    if (!entities) return;
    const int slot = variables.slotOf(r.variable);
    for (const auto& entity : *entities) {
      if (!scope.empty() && !facts_.contains(scope, entity)) continue;
      if (b[slot] && *b[slot] != entity) continue;
      Bindings next = b;
      next[slot] = &entity;
      range(i + 1, next);
    }
  };
  range(0, Bindings(variables.size(), nullptr));
  std::size_t broken = 0;
  for (const auto& subject : subjects) {
    const auto found = solver_.solve(id, claim, variables, subject, true);
    const bool answered = !found.empty();
    if (answered != formula.never) continue;
    Bindings witness(variables.size(), nullptr);  // what the never's answer binds beyond the subject
    if (answered)
      for (std::size_t s = 0; s < variables.size(); ++s)
        if (!subject[s]) witness[s] = found.front()[s];
    const std::vector<std::pair<std::string, std::string>> named = variables.pairs(subject);
    const std::vector<std::pair<std::string, std::string>> beyond = variables.pairs(witness);
    const model::Location first =
        formula.ranges.empty() ? model::Location{} : facts_.locationOf(*subject[variables.slotOf(formula.ranges.front().variable)]);
    const model::Location at = !first.file.empty() ? first : ruleAt;
    model_.breach(id, {named, formula.never ? beyond : std::vector<std::pair<std::string, std::string>>{},
                       at});  // every breach is kept, the findings stop at a few
    if (++broken > kReportsPerRule) continue;
    std::string message = "is broken";
    if (!named.empty()) message += " for " + describe(named);
    if (formula.never && !beyond.empty()) message += ": " + describe(beyond);
    if (formula.never && answered) message += through(id, claim, variables, found.front());
    // the finding sits on what breaks the rule, the first thing it ranges over (a call in its sequence,
    // a class in its file), so the reader lands there; the message names the rule
    report(model::Code::E_RULE_BROKEN, id, message, at);
  }
  if (broken > kReportsPerRule)
    report(model::Code::E_RULE_BROKEN, id, "is broken " + std::to_string(broken - kReportsPerRule) + " more times", ruleAt);
}

}  // namespace check
}  // namespace ds
