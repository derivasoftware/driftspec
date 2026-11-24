#include "ds/check/Evaluator.hpp"

#include <functional>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

using Bindings = std::map<std::string, std::string>;

Evaluator::Evaluator(model::Model& model, const model::Metamodel& metamodel, const Resolver& resolver)
    : model_(model),
      metamodel_(metamodel),
      facts_(model, metamodel),
      validator_(metamodel, resolver, facts_),
      strata_(facts_),
      solver_(facts_) {}

std::string Evaluator::describe(const Bindings& bindings) {
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
  for (const auto& f : facts_.formulas()) {
    std::vector<std::pair<std::string, std::string>> resolved;
    if (!validator_.validate(f.first, f.second, resolved)) invalid_.insert(f.first);
    for (const auto& r : resolved) facts_.record(f.first, r.first, r.second);  // the one who directs writes the facts
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
        for (const auto& c : f.cases) {
          for (const auto& b : solver_.solve(d, c, {}, false)) {
            std::vector<std::string> t;
            for (const auto& p : f.parameters) t.push_back(b.at(p));
            if (facts_.derive(d, t)) changed = true;
          }
        }
      }
      if (!recursive) break;  // every definition was computed after what it applies: a second pass adds nothing
    }
  }
}

void Evaluator::tabulate(const std::string& id, const model::Formula& formula) {
  std::set<Bindings> subjects;
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (const auto& w : solver_.solve(id, formula.where, b, false)) subjects.insert(w);
      return;
    }
    const model::Range& r = formula.ranges[i];
    std::string scope;
    if (!r.scope.empty()) facts_.constant(id, r.scope, scope);
    const std::vector<std::string>* entities = facts_.ofKind(r.kind);
    if (!entities) return;
    for (const auto& entity : *entities) {
      if (!scope.empty() && !facts_.contains(scope, entity)) continue;
      Bindings next = b;
      next[r.variable] = entity;
      range(i + 1, next);
    }
  };
  range(0, {});
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
        const auto bound = subject.find(column.variable);
        const std::string entity = bound == subject.end() ? "" : bound->second;
        if (column.field == "holds" && facts_.kindIs(entity, model::Kind::Rule)) {  // a rule: whether the model keeps it
          const bool holds = facts_.holds(entity);
          text = holds ? "holds" : "broken";
          kind = holds ? "" : "against";
        } else if (bound != subject.end()) {
          text = facts_.fieldOf(entity, column.field);
        }
      } else if (column.form == "related") {
        std::set<std::string> seen;
        for (const auto& answer : solver_.solve(id, column.body, subject, false)) {
          const auto at = answer.find(column.variable);
          if (at == answer.end() || !seen.insert(at->second).second) continue;
          text += (text.empty() ? "" : ", ") + shown(at->second);
        }
      } else {
        const bool yes = !solver_.solve(id, column.body, subject, true).empty();
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

void Evaluator::evaluateRule(const std::string& id, const model::Formula& formula) {
  if (formula.claims.size() != 1) return;  // the checker says so (W-RULE-NO-CLAIM)
  const model::Location ruleAt = facts_.locationOf(id);
  const auto& claim = formula.claims.front();
  std::set<Bindings> subjects;
  // every binding of the ranges, then of where
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (const auto& w : solver_.solve(id, formula.where, b, false)) subjects.insert(w);
      return;
    }
    const model::Range& r = formula.ranges[i];
    std::string scope;
    if (!r.scope.empty()) facts_.constant(id, r.scope, scope);
    const std::vector<std::string>* entities = facts_.ofKind(r.kind);
    if (!entities) return;
    for (const auto& entity : *entities) {
      if (!scope.empty() && !facts_.contains(scope, entity)) continue;
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
    const auto found = solver_.solve(id, claim, subject, true);
    const bool answered = !found.empty();
    if (answered)
      for (const auto& v : found.front())
        if (!subject.count(v.first)) witness.insert(v);
    if (answered != formula.never) continue;
    const model::Location first =
        formula.ranges.empty() ? model::Location{} : facts_.locationOf(subject.at(formula.ranges.front().variable));
    const model::Location at = !first.file.empty() ? first : ruleAt;
    model_.breach(id, {{subject.begin(), subject.end()},
                       formula.never ? std::vector<std::pair<std::string, std::string>>(witness.begin(), witness.end())
                                     : std::vector<std::pair<std::string, std::string>>{},
                       at});  // every breach is kept, the findings stop at a few
    if (++broken > kReportsPerRule) continue;
    std::string message = "is broken";
    if (!subject.empty()) message += " for " + describe(subject);
    if (formula.never && !witness.empty()) message += ": " + describe(witness);
    // the finding sits on what breaks the rule, the first thing it ranges over (a call in its sequence,
    // a class in its file), so the reader lands there; the message names the rule
    report(model::Code::E_RULE_BROKEN, id, message, at);
  }
  if (broken > kReportsPerRule)
    report(model::Code::E_RULE_BROKEN, id, "is broken " + std::to_string(broken - kReportsPerRule) + " more times", ruleAt);
}

}  // namespace check
}  // namespace ds
