#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/check/Steps.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/model/Words.hpp"

namespace ds {
namespace check {

std::string FailureFlow::name() const { return "failures"; }

// the calls must be resolved, and each behaviour bound to the operation it is the behaviour of
std::vector<std::string> FailureFlow::needs() const { return {"linker", "behaviour"}; }

std::vector<model::Finding> FailureFlow::run(Context& context) {
  model_ = &context.model();
  std::map<std::string, std::map<std::string, Escape>> escapes;  // operation -> failure -> how it escapes
  while (pass(escapes)) {
  }
  write(escapes);
  return context.take();
}

std::set<std::string> FailureFlow::handledAround(const std::string& step) const {
  // up from the step: an attempt it is reached through by the attempt's own steps handles what its
  // handlers name; a handler's steps are not handled by the attempt that holds the handler
  std::set<std::string> out;
  std::string below = step;
  for (std::string up = model_->index().parent(step); !up.empty(); below = up, up = model_->index().parent(up)) {
    const model::Entity* frame = model_->find(up);
    if (!frame || frame->kind != model::Kind::Frame || frame->field("kind") != "attempt") continue;
    const model::Entity* from = model_->find(below);
    if (from && from->kind == model::Kind::Frame && from->field("kind") == "handler") continue;
    for (const auto& r : model_->index().from(up)) {
      const model::Entity* h = r.kind == "contains" ? model_->find(r.to) : nullptr;
      if (h && h->kind == model::Kind::Frame && h->field("kind") == "handler") out.insert(h->field("failure"));
    }
  }
  return out;
}

std::string FailureFlow::originOf(const std::string& step, const std::string& sequence) const {
  // the scenario a failure starts in: the nearest frame around it that names one, else the behaviour's
  for (std::string up = model_->index().parent(step); !up.empty() && up != sequence; up = model_->index().parent(up)) {
    const model::Entity* frame = model_->find(up);
    if (frame && !frame->field("scenario").empty()) return model::Identity(sequence).withScenario(frame->field("scenario")).text();
  }
  return sequence;
}

bool FailureFlow::hasBehaviour(const std::string& operation) const {
  for (const auto& r : model_->index().to(operation))
    if (r.kind == "behaviour-of") return true;
  return false;
}

std::string FailureFlow::callee(const std::string& message) const {
  for (const auto& r : model_->index().from(message))
    if (r.kind == "calls") return r.to;
  return "";
}

bool FailureFlow::pass(std::map<std::string, std::map<std::string, Escape>>& out_escapes) const {
  bool changed = false;
  for (const auto& pair : model_->entities()) {  // what has no behaviour fails as its signature says (DS-189)
    const model::Entity& op = pair.second;
    const bool operation = op.kind == model::Kind::Method || op.kind == model::Kind::Function || op.kind == model::Kind::Operation;
    if (!operation || op.expected || op.field("fails").empty() || hasBehaviour(op.id)) continue;
    for (const auto& name : model::split(op.field("fails"), ',')) {
      if (name == "nothing" || out_escapes[op.id].count(name)) continue;
      out_escapes[op.id][name] = Escape{{op.id}, op.id};
      changed = true;
    }
  }
  for (const auto& pair : model_->entities()) {
    const model::Entity& seq = pair.second;
    if (seq.kind != model::Kind::Sequence || seq.expected) continue;
    std::string operation;
    for (const auto& r : model_->index().from(seq.id))
      if (r.kind == "behaviour-of") operation = r.to;
    if (operation.empty()) continue;
    auto& own = out_escapes[operation];
    const model::SequenceTree tree(*model_, seq.id);  // kept: its messages are read while it lives
    for (const auto& id : tree.messages()) {
      const model::Entity* m = model_->find(id);
      if (!m) continue;
      const std::set<std::string> handled = handledAround(id);
      const std::string fails = m->field("fails");
      if (!fails.empty() && !handled.count(fails) && !own.count(fails)) {  // it starts here
        own[fails] = Escape{{operation}, originOf(id, seq.id)};
        changed = true;
      }
      const std::string called = callee(id);
      if (called.empty() || called == operation) continue;
      const auto below = out_escapes.find(called);
      if (below == out_escapes.end()) continue;
      for (const auto& e : below->second) {
        if (handled.count(e.first) || own.count(e.first)) continue;  // handled here, or known to escape already
        Escape up{{operation}, e.second.origin};
        up.through.insert(up.through.end(), e.second.through.begin(), e.second.through.end());
        own[e.first] = up;
        changed = true;
      }
    }
  }
  return changed;
}

void FailureFlow::write(const std::map<std::string, std::map<std::string, Escape>>& escapes) {
  for (const auto& op : escapes) {
    model::Entity* e = model_->find(op.first);
    if (!e || op.second.empty()) continue;
    std::vector<std::string> names;
    for (const auto& f : op.second) {
      names.push_back(f.first);
      e->fields["escape." + f.first] = model::join(f.second.through, " -> ") + " from " + f.second.origin;
    }
    e->fields["escapes"] = model::join(names, ", ");
  }
}

}  // namespace check
}  // namespace ds
