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
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

std::string FailureCheck::name() const { return "failure-check"; }

std::vector<std::string> FailureCheck::needs() const {
  // what ContractCheck needed: the expansions behaviour typing made, over the links the linker made, and what
  // escapes each operation
  return {"linker", "behaviour", "failures"};
}

FailureCheck::FailureCheck(const Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> FailureCheck::check() {
  found_.clear();
  checkHandlers();
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    const bool operation = e.kind == model::Kind::Operation || e.kind == model::Kind::Method || e.kind == model::Kind::Function;
    if (operation && !e.expected) checkFailures(e);
    if (e.kind == model::Kind::UseCase && !e.expected) checkScenario(e);
  }
  const std::set<std::string> named = failureNames();
  for (const auto& pair : model_->entities())
    if (pair.second.kind == model::Kind::UseCase && !pair.second.expected) checkOutcomes(pair.second, named);
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// A handler names a failure that none of the steps of its attempt can fail with: it handles nothing, most often
// a misspelt name (DS-224). Said only when every call in the attempt says what it fails with.
void FailureCheck::checkHandlers() {
  const model::Index index = model_->index();
  for (const auto& pair : model_->entities()) {
    const model::Entity& handler = pair.second;
    if (handler.kind != model::Kind::Frame || handler.field("kind") != "handler") continue;
    const std::string attempt = index.parent(handler.id);
    std::set<std::string> possible;
    bool known = true;
    std::vector<std::string> pending{attempt};
    while (!pending.empty()) {
      const std::string at = pending.back();
      pending.pop_back();
      for (const auto& child : index.children(at)) {
        const model::Entity* c = model_->find(child);
        if (!c) continue;
        if (c->kind == model::Kind::Frame) {
          if (!(at == attempt && c->field("kind") == "handler")) pending.push_back(child);  // the handlers are not its steps
          continue;
        }
        if (c->kind != model::Kind::Message) continue;
        if (!c->field("fails").empty()) possible.insert(c->field("fails"));
        for (const auto& r : index.from(child)) {
          if (r.kind != "calls") continue;
          const model::Entity* op = model_->find(r.to);
          if (!op) continue;
          bool behaves = false;
          for (const auto& b : index.to(op->id)) behaves = behaves || b.kind == "behaviour-of";
          if (!behaves && op->fields.count("fails") == 0) known = false;  // says nothing of how it fails
          for (const auto& f : splitTrimmed(op->field("escapes"), ',')) possible.insert(f);
        }
      }
    }
    const std::string failure = handler.field("failure");
    if (!known || failure.empty() || possible.count(failure)) continue;
    found_.push_back(context_->metamodel().finding(
        model::Code::W_HANDLER_UNREACHED,
        "the handler of " + failure + " handles nothing: no step of its attempt fails with it" +
            (possible.empty() ? "" : " (they fail with " + join(std::vector<std::string>(possible.begin(), possible.end()), ", ") + ")"),
        handler.location, handler.id));
  }
}

void FailureCheck::checkFailures(const model::Entity& op) {
  // an interface operation is a boundary: what escapes it is said in its signature, or nothing may; any
  // other operation is held to its signature only when it writes one (DS-189)
  const bool boundary = op.kind == model::Kind::Operation;
  const bool promises = op.fields.count("fails") > 0;
  if (!promises && !boundary) return;
  std::set<std::string> declared;
  for (const auto& name : text::splitTrimmed(op.field("fails"), ','))
    if (name != "nothing") declared.insert(name);
  std::set<std::string> escaping;
  for (const auto& name : text::splitTrimmed(op.field("escapes"), ',')) {
    escaping.insert(name);
    if (declared.count(name)) continue;
    const std::string how = op.field("escape." + name);
    found_.push_back(context_->metamodel().finding(
        model::Code::W_FAILURE_UNDECLARED,
        op.id + " lets " + name + " escape (" + how + "), and " +
            (promises ? "its signature does not say it may fail with it" : "a boundary must say what it fails with"),
        op.location, op.id));
  }
  const bool behaviour = !model_->index().toOfKind(op.id, "behaviour-of").empty();
  if (!behaviour) return;  // with no behaviour, the signature is what it fails with
  for (const auto& name : declared) {
    if (escaping.count(name)) continue;
    found_.push_back(context_->metamodel().finding(model::Code::W_FAILURE_UNREACHED,
                                                   op.id + " says it may fail with " + name + ", and no path of its behaviour does",
                                                   op.location, op.id));
  }
}

// A use case has no signature to promise failures in (DS-434): what a call of its scenario fails with is handled
// there, or it escapes the use case, said at the scenario.
void FailureCheck::checkScenario(const model::Entity& usecase) {
  model::Location at = usecase.location;
  for (const auto& r : model_->index().toOfKind(usecase.id, "behaviour-of")) {
    const model::Entity* seq = model_->find(r.from);
    if (seq && model::Identity(seq->id).scenario().empty()) at = seq->location;
  }
  // a failure an outcome is the end of ends there (DS-467): the use case says it, as a signature would
  std::set<std::string> ended;
  for (const model::Entity* o : outcomesOf(usecase.id))
    if (!o->field("fails").empty()) ended.insert(o->field("fails"));
  for (const auto& name : text::splitTrimmed(usecase.field("escapes"), ',')) {
    if (ended.count(name)) continue;
    found_.push_back(context_->metamodel().finding(
        model::Code::W_FAILURE_UNDECLARED,
        usecase.id + " lets " + name + " escape (" + usecase.field("escape." + name) + "), and its scenario does not handle it", at,
        usecase.id));
  }
}

std::vector<const model::Entity*> FailureCheck::outcomesOf(const std::string& usecase) const {
  // up the chain of what it specialises, never round a cycle; an outcome redefined by name is the nearest one
  std::vector<const model::Entity*> out;
  std::set<std::string> seen, names;
  for (std::string at = usecase; !at.empty() && seen.insert(at).second;) {
    for (const auto& member : model_->index().members(at)) {
      const model::Entity* o = model_->find(member);
      if (o && o->kind == model::Kind::Outcome && names.insert(o->name).second) out.push_back(o);
    }
    const auto up = model_->index().fromOfKind(at, "inherits");
    at = up.empty() ? std::string() : up.front().to;
  }
  return out;
}

std::set<std::string> FailureCheck::failureNames() const {
  // what a failure is written as: a type, an error code an enum lists, what a signature promises, what a step fails
  std::set<std::string> out;
  const auto add = [&out](const std::string& name) {
    if (name.empty() || name == "nothing") return;
    out.insert(name);
    out.insert(model::Identity(name).lastName());
  };
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Interface) add(e.name);
    if (e.kind == model::Kind::Enum)
      for (const auto& v : text::splitTrimmed(e.field("values"), ',')) add(v);
    for (const auto& f : text::splitTrimmed(e.field("fails"), ','))
      if (e.kind != model::Kind::Outcome) add(f);
  }
  return out;
}

void FailureCheck::checkOutcomes(const model::Entity& usecase, const std::set<std::string>& named) {
  std::map<std::string, const model::Entity*> ending;  // each failure, by the first outcome that is its end
  for (const model::Entity* o : outcomesOf(usecase.id)) {
    const std::string failure = o->field("fails");
    if (failure.empty()) continue;
    const bool own = model::Identity(o->id).text().rfind(usecase.id + "::", 0) == 0;
    // what is named nowhere is a slip; a failure declared and failed by nothing yet is a design partly written (DS-467)
    if (own && !named.count(failure) && !named.count(model::Identity(failure).lastName()))
      found_.push_back(context_->metamodel().finding(
          model::Code::W_OUTCOME_FAILURE_UNKNOWN,
          o->id + " is the end of " + failure + ", which the workspace names nowhere: no type, value or signature has it", o->location,
          o->id));
    const auto first = ending.find(failure);
    if (first == ending.end()) {
      ending[failure] = o;
    } else if (own || model::Identity(first->second->id).text().rfind(usecase.id + "::", 0) == 0) {
      found_.push_back(context_->metamodel().finding(model::Code::W_OUTCOME_FAILURE_TWICE,
                                                     usecase.id + "'s outcomes " + first->second->name + " and " + o->name +
                                                         " are both the end of " + failure + ": one failure ends in one outcome",
                                                     o->location, usecase.id));  // the sentence is about the use case's outcomes
    }
  }
}

}  // namespace check
}  // namespace ds
