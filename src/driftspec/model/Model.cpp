#include "driftspec/model/Model.hpp"

namespace driftspec {
namespace model {

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
    case Kind::Boundary:
      return "boundary";
    case Kind::Operation:
      return "operation";
    case Kind::Constraint:
      return "constraint";
    case Kind::Class:
      return "class";
    case Kind::Enum:
      return "enum";
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
  }
  return "?";
}

bool Entity::has(const std::string& flag) const {
  for (const auto& f : flags) {
    if (f == flag) return true;
  }
  return false;
}

Entity& Model::declare(const Entity& entity) {
  auto it = entities_.find(entity.id);
  if (it == entities_.end()) {
    return entities_.emplace(entity.id, entity).first->second;
  }
  Entity& existing = it->second;
  if (existing.expected && !entity.expected) {
    // a declaration arrives for something referenced earlier (R6)
    existing = entity;
    return existing;
  }
  if (entity.expected) {
    return existing;  // a second reference to the same expected node
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

void Model::relate(const Relation& relation) { relations_.push_back(relation); }

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
    if (m->first.rfind(from + "::", 0) == 0 || m->first.rfind(from + "/", 0) == 0) {
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
    else if (id.rfind(from + "::", 0) == 0 || id.rfind(from + "/", 0) == 0)
      id = to + id.substr(from.size());
  };
  for (Relation& r : relations_) {
    translate(r.from);
    translate(r.to);
  }
  for (auto& e : entities_) {
    for (auto& f : e.second.fields) {
      if (f.first == "expands" || f.first == "subject-of") translate(f.second);
    }
  }
  return true;
}

void Model::report(const Finding& finding) { findings_.push_back(finding); }

}  // namespace model
}  // namespace driftspec

namespace driftspec {
namespace model {

bool isSystemScope(const std::string& scope) { return !scope.empty() && scope.back() == '@' && scope.find('@') == scope.size() - 1; }

std::string lastName(const std::string& id) {
  const auto cut = id.find_last_of(":@");
  return cut == std::string::npos ? id : id.substr(cut + 1);
}

}  // namespace model
}  // namespace driftspec
