#include "ds/check/Facts.hpp"

namespace ds {
namespace check {

// What holds what (contains, else the declared scope), the entities of each kind and every relation
// both ways: the facts a rule is evaluated over.
// The joins a rule asks for: its relations by their ends, and the declared entities of each kind.
// What holds what is the model's own index (DS-171).
void Facts::index() {
  for (const auto& r : model_.index().all()) {
    forward_[r.kind].insert({r.from, r.to});
    backward_[r.kind].insert({r.to, r.from});
  }
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (!e.expected) byKind_[metamodel_.nameOf(e.kind)].push_back(e.id);
  }
  // what holds what, as the index answers it: every entity and every end a `contains` reaches may have a holder
  const model::Index index = model_.index();
  const auto copy = [&](const std::string& id) {
    if (parent_.count(id)) return;
    const std::string up = index.parent(id);
    if (up.empty()) return;
    parent_.emplace(id, up);
    if (!children_.count(up)) children_.emplace(up, index.children(up));
  };
  for (const auto& pair : model_.entities()) copy(pair.first);
  const auto contains = forward_.find("contains");
  if (contains != forward_.end())
    for (const auto& r : contains->second) copy(r.second);
}

bool Facts::kindIs(const std::string& id, model::Kind kind) const {
  const model::Entity* e = model_.find(id);
  return e && e->kind == kind;
}

bool Facts::kindOf(const std::string& id, model::Kind& out_kind) const {
  const model::Entity* e = model_.find(id);
  if (!e) return false;
  out_kind = e->kind;
  return true;
}

bool Facts::isOfKind(const std::string& id, const std::string& kind) const {
  const model::Entity* e = model_.find(id);
  return e && !e->expected && kind == metamodel_.nameOf(e->kind);
}

std::string Facts::scopeOf(const std::string& id) const {
  const model::Entity* e = model_.find(id);
  return e ? e->scope : "";
}

model::Location Facts::locationOf(const std::string& id) const {
  const model::Entity* e = model_.find(id);
  return e ? e->location : model::Location{};
}

std::string Facts::fieldOf(const std::string& id, const std::string& field) const {
  const model::Entity* e = model_.find(id);
  return !e ? "" : field == "name" ? e->name : field == "id" ? e->id : e->field(field);
}

bool Facts::holds(const std::string& rule) const {
  const auto broken = model_.breaches().find(rule);
  return broken == model_.breaches().end() || broken->second.empty();
}

bool Facts::contains(const std::string& outer, const std::string& inner) const {
  const std::string* up = parentOf(inner);
  std::string box;
  if (!up) {  // an artefact hangs from no box by `contains`: it is in the box it was written in (DS-268)
    const model::Entity* e = model_.find(inner);
    box = e ? e->scope : "";
    while (!box.empty() && (box.back() == '@' || box.back() == ':')) box.pop_back();
    if (box.empty()) return false;
    if (box == outer) return true;
    up = parentOf(box);
  }
  for (; up; up = parentOf(*up))
    if (*up == outer) return true;
  return false;
}

std::vector<std::pair<std::string, std::string>> Facts::argsTo(const std::string& call) const {
  std::vector<std::pair<std::string, std::string>> out;
  const auto called = forward_.find("calls");
  if (called == forward_.end()) return out;
  const model::Entity* message = model_.find(call);
  const auto target = called->second.find(call);
  if (!message || target == called->second.end()) return out;
  const model::Entity* op = model_.find(target->second);
  if (!op) return out;
  for (std::size_t i = 0; i < op->params.size() && i < message->args.size(); ++i) out.push_back({op->params[i].name, message->args[i]});
  return out;
}

}  // namespace check
}  // namespace ds
