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
  }
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
        handler.location));
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
        op.location));
  }
  const bool behaviour = !model_->index().toOfKind(op.id, "behaviour-of").empty();
  if (!behaviour) return;  // with no behaviour, the signature is what it fails with
  for (const auto& name : declared) {
    if (escaping.count(name)) continue;
    found_.push_back(context_->metamodel().finding(
        model::Code::W_FAILURE_UNREACHED, op.id + " says it may fail with " + name + ", and no path of its behaviour does", op.location));
  }
}

}  // namespace check
}  // namespace ds
