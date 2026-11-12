#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

using model::outer;

std::string ContractCheck::name() const { return "contracts"; }

std::vector<std::string> ContractCheck::needs() const {
  // the expansions behaviour typing made, over the links the linker made
  return {"linker", "behaviour"};
}

std::vector<model::Finding> ContractCheck::check(const Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  found_.clear();
  checkInterfaces();
  checkUseCases();
  checkDiagrams();
  checkScenarios();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

std::string ContractCheck::outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const {
  if (!seen->insert(model::withScenario(sequence, path)).second) return "";
  const std::vector<const model::Entity*> messages = model::pathOf(*model_, sequence, path, nullptr);
  std::string wanted;  // the name of the result the behaviour returns; empty: its last call decides
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    const model::Entity& m = **it;
    const auto result = m.fields.find("result");
    if (m.fields.count("return")) {
      const std::string value = result == m.fields.end() ? "" : result->second;
      if (value.empty()) return "";
      if (!(std::isdigit(static_cast<unsigned char>(value.front())) || value.front() == '-')) {
        wanted = value;
        continue;
      }
      return value;
    }
    if (!wanted.empty() && (result == m.fields.end() || result->second != wanted)) continue;
    const auto expands = m.fields.find("expands");
    const auto into = m.fields.find("expands-path");
    return expands == m.fields.end() ? "" : outcomeOf(expands->second, into == m.fields.end() ? "" : into->second, seen);
  }
  return "";
}

// A use case is a contract (DS-77): what goes in, what comes out, the outcomes it ends in. A
// specific one keeps its base's signature and outcomes; an interface operation that realises it
// takes and returns the same and handles every outcome as a scenario of its behaviour; a scenario
// no outcome names is behaviour no use case asked for.
void ContractCheck::checkUseCases() {
  const auto isUseCase = [this](const std::string& id) {
    const model::Entity* e = model_->find(id);
    return e && !e->expected && e->kind == model::Kind::UseCase;
  };
  std::map<std::string, std::string> base;
  std::map<std::string, std::vector<model::Relation>> realisedBy;  // copies: the index answers with copies
  for (const auto& r : model_->index().all()) {
    const model::Entity* from = model_->find(r.from);
    if (!from) continue;
    if (r.kind == "inherits" && from->kind == model::Kind::UseCase) {
      if (isUseCase(r.to))
        base[r.from] = r.to;
      else
        found_.push_back(
            {"W-USECASE-UNDECLARED", "warning", r.from + " specialises '" + r.to + "', which is no use case of the workspace", r.location});
    } else if (r.kind == "realises" && from->kind == model::Kind::Operation) {
      if (isUseCase(r.to))
        realisedBy[r.to].push_back(r);
      else
        found_.push_back(
            {"W-USECASE-UNDECLARED", "warning", r.from + " realises '" + r.to + "', which is no use case of the workspace", r.location});
    }
  }
  // the chain of a use case up to its most generic, never round a cycle
  const auto chain = [&base](const std::string& id) {
    std::vector<std::string> out{id};
    std::set<std::string> seen{id};
    for (auto it = base.find(id); it != base.end() && seen.insert(it->second).second; it = base.find(it->second)) out.push_back(it->second);
    return out;
  };
  const auto outcomes = [this, &chain](const std::string& id) {
    std::set<std::string> out;
    for (const auto& uc : chain(id))
      for (const auto& member : model_->index().members(uc)) {
        const model::Entity& held = *model_->find(member);
        if (held.kind == model::Kind::Outcome) out.insert(held.name);
      }
    return out;
  };
  const auto contract = [this, &chain](const std::string& id) -> const model::Entity* {  // the nearest that writes a signature
    for (const auto& uc : chain(id)) {
      const model::Entity* e = model_->find(uc);
      if (e && e->fields.count("operation")) return e;
    }
    return nullptr;
  };
  const auto signature = [](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + model::lastName(e.params[i].type);
    const auto returns = e.fields.find("returns");
    return out + ") -> " + (returns == e.fields.end() ? std::string("none") : model::lastName(returns->second));
  };
  for (const auto& pair : base) {  // a specific use case keeps the contract of its base
    const model::Entity* own = model_->find(pair.first);
    const model::Entity* inherited = contract(pair.second);
    if (own && inherited && own->fields.count("operation") && signature(*own) != signature(*inherited)) {
      found_.push_back(
          {"W-USECASE-SIGNATURE", "warning",
           pair.first + " specialises " + pair.second + " but takes and gives " + signature(*own) + ", not " + signature(*inherited),
           own->location});
    }
  }
  std::set<std::string> realised;
  for (const auto& pair : realisedBy) {
    for (const auto& uc : chain(pair.first)) realised.insert(uc);
    const model::Entity* promised = contract(pair.first);
    const std::set<std::string> ends = outcomes(pair.first);
    for (const model::Relation& r : pair.second) {
      const model::Entity* op = model_->find(r.from);
      if (promised && signature(*op) != signature(*promised)) {
        found_.push_back({"W-USECASE-SIGNATURE", "warning",
                          op->id + " realises " + pair.first + " but takes and gives " + signature(*op) + ", not " + signature(*promised),
                          r.location});
      }
      const std::string behaviour = model::behaviourId(op->id);
      for (const auto& end : ends) {
        if (end == "default" || model_->find(model::withScenario(behaviour, end)) || model::hasScenario(*model_, behaviour, end)) continue;
        found_.push_back({"W-USECASE-OUTCOME-UNHANDLED", "warning",
                          op->id + " realises " + pair.first + " and has no scenario for its outcome '" + end + "'", r.location});
      }
      for (const auto& other : model_->entities()) {  // a scenario written apart that no outcome names
        const model::Entity& s = other.second;
        if (s.kind != model::Kind::Sequence || model::behaviourId(model::subjectOf(s.id)) != behaviour || model::scenarioOf(s.id).empty())
          continue;
        const std::string scenario = s.id.substr(behaviour.size() + 1);
        if (!ends.count(scenario)) {
          found_.push_back({"W-SCENARIO-NO-OUTCOME", "warning",
                            op->id + " handles '" + scenario + "', and " + pair.first + " names no such outcome", s.location});
        }
      }
    }
  }
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::UseCase || e.expected || realised.count(e.id)) continue;
    found_.push_back({"W-USECASE-UNREALISED", "warning",
                      e.id + " is realised by no interface operation, nor is any use case that specialises it", e.location});
  }
}

void ContractCheck::checkInterfaces() {
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::State) {
      const std::string subjectId = model::machineSubjectOf(e.id);  // e is a state machine: its id names it
      const model::Entity* subject = subjectId.empty() ? nullptr : model_->find(subjectId);
      if (subject && subject->kind == model::Kind::Interface) checkViews(e);
    }
    if (e.kind != model::Kind::Operation) continue;
    const std::string behaviour = model::behaviourId(e.id);
    if (!model_->find(behaviour)) {
      found_.push_back({"W-OPERATION-NO-BEHAVIOUR", "warning", e.id + " has no sequence that says how it is handled", e.location});
    }
    for (const auto& field : e.fields) {
      if (field.first.rfind("exit ", 0) != 0) continue;
      const std::string code = field.first.substr(5);
      std::string id = field.second == "default" ? behaviour : model::withScenario(behaviour, field.second);
      std::string path;
      if (!model_->find(id) && model::hasScenario(*model_, behaviour, field.second)) {
        id = behaviour;  // a named branch of the one behaviour
        path = field.second;
      }
      if (!model_->find(id)) {
        found_.push_back({"W-SCENARIO-UNDECLARED", "warning",
                          e.id + " exits " + code + " on '" + field.second + "', and it has no such scenario", e.location});
        continue;
      }
      std::set<std::string> seen;
      const std::string outcome = outcomeOf(id, path, &seen);
      if (!outcome.empty() && outcome != code) {
        found_.push_back({"W-OUTCOME-MISMATCH", "warning",
                          e.id + " promises exit " + code + " on '" + field.second + "', and that trace ends in " + outcome, e.location});
      }
    }
  }
}

void ContractCheck::checkViews(const model::Entity& state) {
  std::set<std::string> views;
  for (const auto& member : model_->index().members(state.id)) {
    const model::Entity& held = *model_->find(member);
    if (held.kind == model::Kind::View) views.insert(held.name);
  }
  if (views.empty()) return;  // the details of a view are optional: a bare machine asks nothing of them
  std::map<std::string, std::set<std::string>> next;
  const auto moves = state.fields.find("transitions");
  if (moves != state.fields.end()) {
    for (const auto& move : split(moves->second, '\n')) {
      const auto gt = move.find('>');
      const std::string from = move.substr(0, gt);
      const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
      for (const std::string& name : {from, to}) {
        if (!views.count(name)) {
          found_.push_back({"W-VIEW-UNDECLARED", "warning",
                            "the navigation of " + state.name + " names '" + name + "', which is not one of its views", state.location});
        }
      }
      if (from != to) next[from].insert(to);  // staying where you are is no way in or out
    }
  }
  const auto initial = state.fields.find("initial");
  std::set<std::string> reached;
  std::vector<std::string> pending;
  if (initial != state.fields.end()) pending.push_back(initial->second);
  while (!pending.empty()) {
    const std::string view = pending.back();
    pending.pop_back();
    if (!reached.insert(view).second) continue;
    for (const auto& to : next[view]) pending.push_back(to);
  }
  for (const auto& view : views) {
    const model::Entity* v = model_->find(state.id + "::" + view);
    if (!reached.count(view)) {
      found_.push_back({"W-VIEW-UNREACHABLE", "warning", "nothing leads to the view " + view + " of " + state.name, v->location});
    } else if (views.size() > 1 && next[view].empty()) {
      found_.push_back({"W-VIEW-NO-WAY-OUT", "warning", "nothing leaves the view " + view + " of " + state.name, v->location});
    }
  }
}

void ContractCheck::checkScenarios() {
  // a scenario has one name: a branch of the behaviour, or a behaviour of its own, never both nor twice
  std::map<std::string, int> named;
  for (const auto& pair : model_->entities()) {
    const model::Entity& f = pair.second;
    if (f.kind != model::Kind::Frame) continue;
    for (const char* key : {"scenario", "else-scenario"}) {
      const auto name = f.fields.find(key);
      if (name == f.fields.end()) continue;
      const std::string base = model::behaviourId(model::subjectOf(f.scope));  // the behaviour, whatever scenario holds the frame
      if (++named[model::withScenario(base, name->second)] == 2 || model_->find(model::withScenario(base, name->second))) {
        found_.push_back(
            {"W-SCENARIO-TWICE", "warning", base + " names the scenario '" + name->second + "' twice: a scenario is one path", f.location});
      }
    }
  }
}

void ContractCheck::checkDiagrams() {
  for (const auto& pair : model_->entities()) {
    if (pair.second.kind != model::Kind::Diagram) continue;
    // a sequence diagram that asks for a scenario: it is a behaviour of its own, or a named branch of one
    const auto include = pair.second.fields.find("include");
    if (include != pair.second.fields.end() && pair.second.fields.count("kind") && pair.second.fields.at("kind") == "sequence") {
      for (const auto& pattern : split(include->second, ',')) {
        const auto slash = pattern.find(" / ");
        if (slash == std::string::npos) continue;
        const std::string behaviour = model::behaviourId(pattern.substr(0, slash));
        const std::string scenario = pattern.substr(slash + 3);
        if (!model_->find(behaviour) || model_->find(model::withScenario(behaviour, scenario)) ||
            model::hasScenario(*model_, behaviour, scenario))
          continue;
        found_.push_back({"W-SCENARIO-UNDECLARED", "warning", pair.second.name + " asks for " + pattern + ", a scenario nobody declares",
                          pair.second.location});
      }
    }
    const auto depth = pair.second.fields.find("depth");
    if (depth == pair.second.fields.end() || depth->second == "*") continue;
    if (depth->second.empty() || depth->second.find_first_not_of("0123456789") != std::string::npos) {
      found_.push_back({"W-DIAGRAM-DEPTH", "warning",
                        pair.second.name + " asks for depth '" + depth->second + "': a number, or * for every level",
                        pair.second.location});
    }
  }
}

}  // namespace check
}  // namespace ds
