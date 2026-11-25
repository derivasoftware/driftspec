#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"
#include "ds/model/WrittenType.hpp"

namespace ds {
namespace check {

std::string InterfaceContracts::name() const { return "interfaces"; }

std::vector<std::string> InterfaceContracts::needs() const {
  // what ContractCheck needed: the expansions behaviour typing made, over the links the linker made, and what
  // escapes each operation
  return {"linker", "behaviour", "failures"};
}

InterfaceContracts::InterfaceContracts(const Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> InterfaceContracts::check() {
  found_.clear();
  checkInterfaces();
  checkRealisations();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

std::string InterfaceContracts::outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const {
  if (!seen->insert(model::Identity(sequence).withScenario(path).text()).second) return "";
  const model::Paths paths(*model_);
  const std::vector<const model::Entity*> messages = paths.pathOf(sequence, path, nullptr);
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

void InterfaceContracts::checkInterfaces() {
  const model::Paths paths(*model_);
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::State) {
      const std::string subjectId = model::Identity(e.id).machineSubject().text();  // e is a state machine: its id names it
      const model::Entity* subject = subjectId.empty() ? nullptr : model_->find(subjectId);
      if (subject && subject->kind == model::Kind::Interface) checkViews(e);
      if (subject && subject->kind == model::Kind::Class) checkStates(e);
    }
    if (e.kind != model::Kind::Operation) continue;
    const std::string behaviour = model::Identity(e.id).sequence().text();
    if (!model_->find(behaviour)) {
      found_.push_back(context_->metamodel().finding(model::Code::W_OPERATION_NO_BEHAVIOUR,
                                                     e.id + " has no sequence that says how it is handled", e.location, e.id));
    }
    for (const auto& field : e.fields) {
      if (field.first.rfind("exit ", 0) != 0) continue;
      const std::string code = field.first.substr(5);
      std::string id = field.second == "default" ? behaviour : model::Identity(behaviour).withScenario(field.second).text();
      std::string path;
      if (!model_->find(id) && paths.hasScenario(behaviour, field.second)) {
        id = behaviour;  // a named branch of the one behaviour
        path = field.second;
      }
      if (!model_->find(id)) {
        found_.push_back(context_->metamodel().finding(model::Code::W_SCENARIO_UNDECLARED,
                                                       e.id + " exits " + code + " on '" + field.second + "', and it has no such scenario",
                                                       e.location, e.id));
        continue;
      }
      std::set<std::string> seen;
      const std::string outcome = outcomeOf(id, path, &seen);
      if (!outcome.empty() && outcome != code) {
        found_.push_back(context_->metamodel().finding(
            model::Code::W_OUTCOME_MISMATCH,
            e.id + " promises exit " + code + " on '" + field.second + "', and that trace ends in " + outcome, e.location, e.id));
      }
    }
  }
}

// A class that realises an interface and declares one of its operations declares it with the same
// signature: two written facts that must agree (DS-215). What the class does not declare is no finding.
void InterfaceContracts::checkRealisations() {
  // a type as the idea it commits to: no qualifiers and no spaces; the rest is the code's text (DS-18)
  const auto idea = [](const std::string& text) {
    std::string out = model::WrittenType(text).unqualified();
    out.erase(std::remove_if(out.begin(), out.end(), [](unsigned char c) { return std::isspace(c) != 0; }), out.end());
    return out;
  };
  const auto signature = [&idea](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + idea(e.params[i].type);
    const auto returns = e.fields.find("returns");
    return out + ") -> " + idea(returns == e.fields.end() ? "" : returns->second);
  };
  for (const auto& r : model_->index().ofKind("realises")) {
    const model::Entity* cls = model_->find(r.from);
    const model::Entity* iface = model_->find(r.to);
    if (!cls || !iface || cls->kind != model::Kind::Class || iface->kind != model::Kind::Interface || iface->expected) continue;
    for (const auto& id : model_->index().members(iface->id)) {
      const model::Entity* op = model_->find(id);
      if (!op || op->kind != model::Kind::Operation) continue;
      const model::Entity* method = resolver_->member(cls->id, op->name);
      if (!method || method->kind != model::Kind::Method) continue;
      if (signature(*method) == signature(*op)) continue;
      found_.push_back(context_->metamodel().finding(model::Code::W_INTERFACE_SIGNATURE,
                                                     method->id + " takes and gives " + signature(*method) + ", and " + op->id +
                                                         ", which " + cls->id + " realises, takes and gives " + signature(*op),
                                                     method->location, method->id));
    }
  }
}

void InterfaceContracts::checkViews(const model::Entity& state) {
  std::set<std::string> views;
  for (const auto& member : model_->index().members(state.id)) {
    const model::Entity& held = *model_->find(member);
    if (held.kind == model::Kind::View) views.insert(held.name);
  }
  if (views.empty()) return;  // the details of a view are optional: a bare machine asks nothing of them
  std::map<std::string, std::set<std::string>> next;
  const auto moves = state.fields.find("transitions");
  if (moves != state.fields.end()) {
    for (const auto& move : splitTrimmed(moves->second, '\n')) {
      const auto gt = move.find('>');
      const std::string from = move.substr(0, gt);
      const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
      for (const std::string& name : {from, to}) {
        if (!views.count(name)) {
          found_.push_back(context_->metamodel().finding(
              model::Code::W_VIEW_UNDECLARED, "the navigation of " + state.name + " names '" + name + "', which is not one of its views",
              state.location, state.id));
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
      found_.push_back(context_->metamodel().finding(model::Code::W_VIEW_UNREACHABLE,
                                                     "nothing leads to the view " + view + " of " + state.name, v->location, v->id));
    } else if (views.size() > 1 && next[view].empty()) {
      found_.push_back(context_->metamodel().finding(model::Code::W_VIEW_NO_WAY_OUT,
                                                     "nothing leaves the view " + view + " of " + state.name, v->location, v->id));
    }
  }
}

void InterfaceContracts::checkStates(const model::Entity& state) {
  // a state of a class's machine that no transition leads to from the initial one is never entered (DS-53).
  // The machine starts where it is written to, or in the state the defaults of its subject are in (DS-253);
  // with neither, where it starts cannot be told
  const auto moves = state.fields.find("transitions");
  if (moves == state.fields.end()) return;
  // a final state is one the machine ends in: a transition out of one says otherwise (DS-442). A state with no way out is
  // not taken as final, nor said to be: a machine written in part is complete
  const std::vector<std::string> finals = machine::Machine(*model_, state).finals();
  for (const auto& move : splitTrimmed(moves->second, '\n')) {
    const auto gt = move.find('>');
    if (gt == std::string::npos || std::find(finals.begin(), finals.end(), move.substr(0, gt)) == finals.end()) continue;
    const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
    found_.push_back(context_->metamodel().finding(
        model::Code::W_STATE_LEAVES_FINAL,
        "the transition " + move.substr(0, gt) + " -> " + to + " of " + state.name + " leaves a final state", state.location, state.id));
  }
  const std::string initial = machine::Machine(*model_, state).initial();
  if (initial.empty()) return;
  std::vector<std::string> states{initial};
  std::map<std::string, std::set<std::string>> next;
  for (const auto& move : splitTrimmed(moves->second, '\n')) {
    const auto gt = move.find('>');
    if (gt == std::string::npos) continue;
    const std::string from = move.substr(0, gt);
    const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
    for (const std::string& name : {from, to})
      if (std::find(states.begin(), states.end(), name) == states.end()) states.push_back(name);
    next[from].insert(to);
  }
  std::set<std::string> reached;
  std::vector<std::string> pending{initial};
  while (!pending.empty()) {
    const std::string at = pending.back();
    pending.pop_back();
    if (!reached.insert(at).second) continue;
    for (const auto& to : next[at]) pending.push_back(to);
  }
  for (const auto& name : states) {
    if (reached.count(name)) continue;
    found_.push_back(context_->metamodel().finding(
        model::Code::W_STATE_UNREACHABLE, "nothing leads to the state " + name + " of " + state.name + " from its initial state " + initial,
        state.location, model_->find(state.id + "::" + name) ? state.id + "::" + name : state.id));
  }
}

}  // namespace check
}  // namespace ds
