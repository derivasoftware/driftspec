#include "ds/model/Model.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

#include "ds/model/Metamodel.hpp"
#include "ds/model/SequenceTree.hpp"

namespace ds {
namespace model {

bool isArtefact(Kind kind) {
  return kind == Kind::Requirement || kind == Kind::Verification || kind == Kind::Constraint || kind == Kind::UseCase;
}

const char* kindName(Kind kind) {
  switch (kind) {
    case Kind::Package:
      return "package";
    case Kind::Module:
      return "module";
    case Kind::Requirement:
      return "requirement";
    case Kind::Verification:
      return "verification";
    case Kind::Interface:
      return "interface";
    case Kind::View:
      return "view";
    case Kind::Operation:
      return "operation";
    case Kind::Constraint:
      return "constraint";
    case Kind::Class:
      return "class";
    case Kind::Enum:
      return "enum";
    case Kind::Function:
      return "function";
    case Kind::Method:
      return "method";
    case Kind::Attribute:
      return "attribute";
    case Kind::Operator:
      return "operator";
    case Kind::Sequence:
      return "sequence";
    case Kind::Message:
      return "message";
    case Kind::Frame:
      return "frame";
    case Kind::State:
      return "state";
    case Kind::Diagram:
      return "diagram";
    case Kind::UseCase:
      return "usecase";
    case Kind::Outcome:
      return "outcome";
    case Kind::Rule:
      return "rule";
    case Kind::Definition:
      return "definition";
  }
  return "?";
}

bool Entity::has(const std::string& flag) const {
  for (const auto& f : flags) {
    if (f == flag) return true;
  }
  return false;
}

std::string Entity::field(const std::string& key) const {
  const auto found = fields.find(key);
  return found == fields.end() ? "" : found->second;
}

namespace {
/// A behaviour is named after its subject: X::state, X::run::sequence. A member written with that name takes
/// the identity of the behaviour when the behaviour exists (DS-222).
bool behaviour(Kind kind) { return kind == Kind::Sequence || kind == Kind::State; }
}  // namespace

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
            std::string(kindName(member.kind)) + " '" + member.id + "' takes the identity of the " +
                ((behaviour(entity.kind) ? entity.kind : existing.kind) == Kind::State ? "state machine" : "sequence") +
                " written for what holds it: rename the " + kindName(member.kind),
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
            std::string(kindName(entity.kind)) + " '" + entity.id + "' is declared twice (first at " + existing.location.file + ":" +
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
  if (!relationOf(relation.kind)) throw std::logic_error("the metamodel does not declare the relation '" + relation.kind + "'");
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
            std::string(kindName(it->second.kind)) + " '" + to + "' is declared twice (first at " + other->location.file + ":" +
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

namespace ds {
namespace model {

namespace {

std::string fieldIn(const Entity& e, const std::string& key) {
  const auto f = e.fields.find(key);
  return f == e.fields.end() ? "" : f->second;
}

/// The frames of one behaviour, read from its tree, and for each what its two sides name.
struct Branches {
  const SequenceTree& tree;

  std::set<std::string> names(const Entity& f, bool elseSide) const {
    std::set<std::string> out;
    const std::string own = tree.scenario(f.id, elseSide);
    if (!own.empty()) out.insert(own);
    for (const auto& child : tree.inside(f.id, elseSide)) {
      for (const bool side : {false, true}) {
        for (const auto& n : names(*tree.model().find(child), side)) out.insert(n);
      }
    }
    return out;
  }

  /// Then, Else: the path takes that side, drawn inline. Skip: it does not. Keep: it cannot tell, the frame is
  /// drawn. Enter: a loop the path goes into, drawn as the loop it is.
  enum class Way { Then, Else, Skip, Keep, Enter };

  Way decide(const Entity& f, const std::string& scenario) const {
    const std::set<std::string> thenNames = names(f, false);
    const std::set<std::string> elseNames = names(f, true);
    // a loop is not a choice: every path goes through it, and one that names a branch inside goes into it
    if (fieldIn(f, "kind") == "loop") return !scenario.empty() && thenNames.count(scenario) ? Way::Enter : Way::Keep;
    // an attempt is not a choice either: its steps run on every path (DS-187)
    if (fieldIn(f, "kind") == "attempt") return Way::Enter;
    // a handler runs only on a path that fails as it handles: one that names it; the default path does not fail
    if (fieldIn(f, "kind") == "handler") return !scenario.empty() && thenNames.count(scenario) ? Way::Then : Way::Skip;
    if (!scenario.empty() && thenNames.count(scenario)) return Way::Then;
    if (!scenario.empty() && elseNames.count(scenario)) return Way::Else;
    if (tree.twoSides(f.id)) {
      if (!thenNames.empty() && elseNames.empty()) return Way::Else;
      if (thenNames.empty() && !elseNames.empty()) return Way::Then;
      return Way::Keep;
    }
    return thenNames.empty() ? Way::Keep : Way::Skip;  // a branch nobody asked for is not taken
  }
};

}  // namespace

bool hasScenario(const Model& model, const std::string& sequence, const std::string& scenario) {
  // a loop, an opt or a side of an alt that names it
  for (const auto& id : model.index().members(sequence)) {
    const Entity* f = model.find(id);
    if (f && f->kind == Kind::Frame && fieldIn(*f, "scenario") == scenario) return true;
  }
  return false;
}

std::vector<const Entity*> pathOf(const Model& model, const std::string& sequence, const std::string& scenario,
                                  std::set<std::string>* kept) {
  const SequenceTree tree(model, sequence);
  const Branches branches{tree};
  std::vector<const Entity*> out;
  for (const auto& id : tree.messages()) {
    const Entity* m = model.find(id);
    bool taken = true;
    bool certain = true;  // every frame around it is a side the path takes, none it cannot tell
    std::vector<std::string> around;
    for (std::string frame = tree.frameOf(m->id); !frame.empty() && taken; frame = tree.frameOf(frame)) {
      const Entity& f = *model.find(frame);
      const Branches::Way way = branches.decide(f, scenario);
      const bool elseSide = tree.onElse(f.id, m->id);
      if (way == Branches::Way::Skip || (way == Branches::Way::Then && elseSide) || (way == Branches::Way::Else && !elseSide)) {
        taken = false;
      } else if (way == Branches::Way::Keep) {
        certain = false;
        around.push_back(f.id);
      } else if (way == Branches::Way::Enter) {
        around.push_back(f.id);
      }
    }
    if (!taken) continue;
    if (kept) kept->insert(around.begin(), around.end());
    out.push_back(m);
    if (certain && fieldIn(*m, "return") == "true") break;  // the path ends here
  }
  return out;
}

}  // namespace model
}  // namespace ds
