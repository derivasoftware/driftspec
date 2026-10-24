#include "driftspec/model/Model.hpp"

namespace driftspec {
namespace model {

const char* kindName(Kind kind) {
  switch (kind) {
    case Kind::Package: return "package";
    case Kind::Module: return "module";
    case Kind::Requirement: return "requirement";
    case Kind::Verification: return "verification";
    case Kind::Boundary: return "boundary";
    case Kind::Operation: return "operation";
    case Kind::Constraint: return "constraint";
    case Kind::Class: return "class";
    case Kind::Enum: return "enum";
    case Kind::Method: return "method";
    case Kind::Attribute: return "attribute";
    case Kind::Operator: return "operator";
    case Kind::Sequence: return "sequence";
    case Kind::State: return "state";
    case Kind::Diagram: return "diagram";
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
            std::string(kindName(entity.kind)) + " '" + entity.id + "' is declared twice (first at " +
                existing.location.file + ":" + std::to_string(existing.location.line) + ")",
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

void Model::report(const Finding& finding) { findings_.push_back(finding); }

}  // namespace model
}  // namespace driftspec
