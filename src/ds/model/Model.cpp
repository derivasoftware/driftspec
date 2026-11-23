#include "ds/model/Model.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace model {

bool Entity::has(const std::string& flag) const {
  for (const auto& f : flags) {
    if (f == flag) return true;
  }
  return false;
}

const std::string& Entity::field(const std::string& key) const {
  static const std::string kNone;  // no copy for a field read, written or not (DS-345)
  const auto found = fields.find(key);
  return found == fields.end() ? kNone : found->second;
}

// A member written with a behaviour's name takes the identity of the behaviour when the behaviour exists (DS-222).
bool Model::behaviour(Kind kind) { return kind == Kind::Sequence || kind == Kind::State; }

Entity& Model::declare(const Entity& entity) {
  auto it = entities_.find(entity.id);
  if (it == entities_.end()) {
    indexEntity(entity, true);
    return entities_.emplace(entity.id, entity).first->second;
  }
  Entity& existing = it->second;
  if (existing.expected && !entity.expected) {
    // a declaration arrives for something referenced earlier (R6)
    indexEntity(existing, false);
    existing = entity;
    indexEntity(existing, true);
    return existing;
  }
  if (entity.expected) {
    return existing;  // a second reference to the same expected node
  }
  // a behaviour and a member named after it: said once, on the member, and the behaviour keeps the identity, so
  // what is written about it (its jumps, its diagram) is still read as it was meant (DS-222)
  if (behaviour(entity.kind) != behaviour(existing.kind)) {
    const Entity& member = behaviour(entity.kind) ? existing : entity;
    report({"E-NAME-RESERVED", "error",
            metamodel_->nameOf(member.kind) + " '" + member.id + "' takes the identity of the " +
                ((behaviour(entity.kind) ? entity.kind : existing.kind) == Kind::State ? "state machine" : "sequence") +
                " written for what holds it: rename the " + metamodel_->nameOf(member.kind),
            member.location});
    if (behaviour(entity.kind)) {
      indexEntity(existing, false);
      existing = entity;
      indexEntity(existing, true);
    }
    return existing;
  }
  const bool reopenable = entity.kind == Kind::Package || entity.kind == Kind::Module;
  if (!reopenable) {
    report({"E-DUPLICATE-IDENTITY", "error",
            metamodel_->nameOf(entity.kind) + " '" + entity.id + "' is declared twice (first at " + existing.location.file + ":" +
                std::to_string(existing.location.line) + ")",
            entity.location});
  }
  return existing;
}

const Entity* Model::find(const std::string& id) const {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

Entity* Model::find(const std::string& id) {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

void Model::relate(const Relation& relation) {
  // a relation the metamodel does not declare is a defect of ds, not of the specification read
  if (!metamodel_->relationOf(relation.kind)) throw std::logic_error("the metamodel does not declare the relation '" + relation.kind + "'");
  relations_.push_back(relation);
  const std::size_t position = relations_.size() - 1;
  indexed_.from[relation.from].push_back(position);
  indexed_.to[relation.to].push_back(position);
  indexed_.kind[relation.kind].push_back(position);
  if (relation.kind == "contains") indexed_.treeStale = true;
}

void Model::indexEntity(const Entity& entity, bool enters) {
  indexed_.treeStale = true;
  auto& members = indexed_.members[entity.scope];
  auto& revisions = indexed_.revisions[model::ArtefactName(entity.id).withoutRevision().text()];
  if (enters) {
    members.insert(entity.id);
    if (!entity.expected) revisions.insert(entity.id);
  } else {
    members.erase(entity.id);
    revisions.erase(entity.id);
  }
  // a bare name the resolver finds by package needs no pass over the model (DS-336)
  if (entity.kind == Kind::Class || entity.kind == Kind::Enum) {
    auto& named = indexed_.named[{Scope(entity.scope).package().text(), entity.name}];
    if (enters && !entity.expected) named.insert(entity.id);
    if (!enters) named.erase(entity.id);
  }
}

void Model::indexAgain() {
  indexed_ = Indexed();
  for (const auto& pair : entities_) indexEntity(pair.second, true);
  for (std::size_t position = 0; position < relations_.size(); ++position) {
    const Relation& r = relations_[position];
    indexed_.from[r.from].push_back(position);
    indexed_.to[r.to].push_back(position);
    indexed_.kind[r.kind].push_back(position);
  }
}

bool Model::rename(const std::string& from, const std::string& to) {
  if (from == to) return true;
  auto it = entities_.find(from);
  if (it == entities_.end()) return false;
  if (const Entity* other = find(to); other && !other->expected) {
    report({"E-DUPLICATE-IDENTITY", "error",
            metamodel_->nameOf(it->second.kind) + " '" + to + "' is declared twice (first at " + other->location.file + ":" +
                std::to_string(other->location.line) + ")",
            it->second.location});
    return false;
  }
  entities_.erase(to);  // an expected node gives way to the declaration
  Entity moved = it->second;
  moved.id = to;
  entities_.erase(it);
  entities_.emplace(to, moved);
  // members: everything whose identity or scope begins with the old one
  std::vector<std::pair<std::string, Entity>> members;
  for (auto m = entities_.begin(); m != entities_.end();) {
    if (m->first != from && model::Identity(m->first).within(model::Identity(from))) {  // a member or a scenario of what is renamed
      Entity e = m->second;
      e.id = to + m->first.substr(from.size());
      if (e.scope.rfind(from, 0) == 0) e.scope = to + e.scope.substr(from.size());
      members.push_back({e.id, e});
      m = entities_.erase(m);
    } else {
      if (m->second.scope == from) m->second.scope = to;
      ++m;
    }
  }
  for (auto& m : members) entities_.emplace(m.first, m.second);
  auto translate = [&](std::string& id) {
    if (id == from)
      id = to;
    else if (model::Identity(id).within(model::Identity(from)))
      id = to + id.substr(from.size());
  };
  // what a rule of a renamed class says moves with it
  std::map<std::string, Formula> formulas;
  for (auto& f : formulas_) {
    std::string id = f.first;
    translate(id);
    formulas.emplace(id, f.second);
  }
  formulas_.swap(formulas);
  for (Relation& r : relations_) {
    translate(r.from);
    translate(r.to);
  }
  for (auto& e : entities_) {
    for (auto& f : e.second.fields) {
      if (f.first == "expands" || f.first == "subject-of") translate(f.second);
    }
  }
  indexAgain();  // identities moved: what was indexed by the old ones is built again
  return true;
}

void Model::report(const Finding& finding) { findings_.push_back(finding); }

}  // namespace model
}  // namespace ds
