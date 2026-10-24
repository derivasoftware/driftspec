#include "driftspec/bind/Binder.hpp"

#include <cstring>
#include <map>

namespace driftspec {
namespace bind {

namespace {

bool is(TSNode node, const char* type) { return std::strcmp(ts_node_type(node), type) == 0; }

TSNode child(TSNode node, const char* fieldName) { return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName))); }

std::string fieldText(const parse::ParsedFile& file, TSNode node, const char* fieldName) {
  TSNode c = child(node, fieldName);
  return ts_node_is_null(c) ? std::string() : parse::textOf(file, c);
}

std::string trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

model::Kind kindForRelation(const std::string& relation) {
  if (relation == "verifies" || relation == "refines" || relation == "requires") return model::Kind::Requirement;
  if (relation == "realises") return model::Kind::Boundary;
  if (relation == "exercised-by") return model::Kind::Verification;
  return model::Kind::Class;
}

std::string relationForAnnotation(model::Kind from, const std::string& annotation) {
  if (annotation == "requirement") return from == model::Kind::Verification ? "verifies" : "requires";
  if (annotation == "boundary") return "realises";
  if (annotation == "verification") return "exercised-by";
  return "refers";
}

const char* const kPrimitives[] = {"int", "bool", "string", "path", "none", "void", "float", "double"};

bool isPrimitive(const std::string& type) {
  for (const char* p : kPrimitives) {
    if (type == p) return true;
  }
  return false;
}

}  // namespace

void Binder::bind(const parse::ParsedFile& file) {
  TSNode root = ts_tree_root_node(file.tree.get());
  visit(file, root, "", "");
}

model::Location Binder::locationOf(const parse::ParsedFile& file, TSNode node) const {
  const TSPoint start = ts_node_start_point(node);
  return {file.path, static_cast<int>(start.row) + 1, static_cast<int>(start.column) + 1};
}

std::string Binder::qualify(const std::string& scope, const std::string& name) const {
  return scope.empty() ? name : scope + "::" + name;
}

void Binder::bindFields(const parse::ParsedFile& file, TSNode node, model::Entity& entity) {
  const uint32_t count = ts_node_named_child_count(node);
  int contracts = 0;
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (is(c, "field")) {
      entity.fields[fieldText(file, c, "key")] = trim(fieldText(file, c, "value"));
    } else if (is(c, "contract")) {
      const std::string key = fieldText(file, c, "key");
      entity.fields["contract." + std::to_string(contracts++)] = trim((key.empty() ? "" : key + ": ") + fieldText(file, c, "body"));
    }
  }
}

void Binder::bindAnnotations(const parse::ParsedFile& file, TSNode node, const std::string& from, const std::string& scope) {
  const model::Entity* entity = model_.find(from);
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (!is(c, "annotation")) continue;
    const std::string kind = fieldText(file, c, "kind");
    const std::string relation = relationForAnnotation(entity ? entity->kind : model::Kind::Class, kind);
    references_.push_back({from, fieldText(file, c, "target"), scope, relation, kindForRelation(relation), locationOf(file, c)});
  }
}

void Binder::visit(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner) {
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    const model::Location loc = locationOf(file, c);

    if (is(c, "package")) {
      model::Entity e;
      e.kind = model::Kind::Package;
      e.name = fieldText(file, c, "name");
      e.id = "package:" + e.name;  // packages do not contribute to names (R4)
      e.location = loc;
      bindFields(file, c, e);
      model::Entity& declared = model_.declare(e);
      for (const auto& f : e.fields) declared.fields[f.first] = f.second;
      visit(file, c, scope, declared.id);

    } else if (is(c, "module")) {
      model::Entity e;
      e.kind = model::Kind::Module;
      e.name = fieldText(file, c, "name");
      e.id = qualify(scope, e.name);
      e.scope = scope;
      e.location = loc;
      model::Entity& declared = model_.declare(e);
      if (!owner.empty()) model_.relate({"contains", owner, declared.id, loc});
      visit(file, c, declared.id, declared.id);

    } else if (is(c, "requirement") || is(c, "verification")) {
      model::Entity e;
      e.kind = is(c, "requirement") ? model::Kind::Requirement : model::Kind::Verification;
      e.name = fieldText(file, c, "id");
      e.id = e.name;  // process artefacts are identified by id (R5)
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      if (!owner.empty()) {
        const model::Entity* ownerEntity = model_.find(owner);
        const bool nestedInRequirement = ownerEntity && ownerEntity->kind == model::Kind::Requirement;
        model_.relate({nestedInRequirement ? "verifies" : "about", e.id, owner, loc});  // nesting gives the relation (R3)
      }
      auto parent = e.fields.find("parent");
      if (parent != e.fields.end() && !parent->second.empty()) {
        references_.push_back({e.id, parent->second, scope, "refines", model::Kind::Requirement, loc});
      }
      bindAnnotations(file, c, e.id, scope);
      visit(file, c, scope, e.id);  // nested verifications

    } else if (is(c, "boundary")) {
      model::Entity e;
      e.kind = model::Kind::Boundary;
      e.name = fieldText(file, c, "name");
      e.id = e.name;
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      if (!owner.empty()) model_.relate({"exposes", owner, e.id, loc});
      bindAnnotations(file, c, e.id, scope);
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode op = ts_node_named_child(c, j);
        if (!is(op, "operation")) continue;
        model::Entity o;
        o.kind = model::Kind::Operation;
        o.name = fieldText(file, op, "name");
        o.id = e.id + "::" + o.name;
        o.scope = e.id;
        o.location = locationOf(file, op);
        o.fields["returns"] = fieldText(file, op, "returns");
        model_.declare(o);
        model_.relate({"contains", e.id, o.id, o.location});
        if (!o.fields["returns"].empty() && !isPrimitive(o.fields["returns"])) {
          references_.push_back({o.id, o.fields["returns"], scope, "uses", model::Kind::Class, o.location});
        }
      }

    } else if (is(c, "class")) {
      model::Entity e;
      e.kind = model::Kind::Class;
      e.name = fieldText(file, c, "name");
      e.id = qualify(scope, e.name);
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      if (!owner.empty()) model_.relate({"contains", owner, e.id, loc});
      const std::string base = fieldText(file, c, "base");
      if (!base.empty()) references_.push_back({e.id, base, scope, "inherits", model::Kind::Class, loc});
      bindAnnotations(file, c, e.id, scope);
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode m = ts_node_named_child(c, j);
        if (is(m, "method") || is(m, "attribute")) {
          model::Entity member;
          member.kind = is(m, "method") ? model::Kind::Method : model::Kind::Attribute;
          member.name = fieldText(file, m, "name");
          member.id = e.id + "::" + member.name;
          member.scope = e.id;
          member.location = locationOf(file, m);
          member.fields["visibility"] = fieldText(file, m, "visibility");
          const std::string type = fieldText(file, m, is(m, "method") ? "returns" : "type");
          if (!type.empty()) member.fields[is(m, "method") ? "returns" : "type"] = type;
          if (is(m, "method")) bindFields(file, m, member);
          model_.declare(member);
          model_.relate({"contains", e.id, member.id, member.location});
          if (!type.empty() && !isPrimitive(type)) {
            references_.push_back({member.id, type, scope, "uses", model::Kind::Class, member.location});
          }
        }
      }

    } else if (is(c, "sequence")) {
      model::Entity e;
      e.kind = model::Kind::Sequence;
      e.name = fieldText(file, c, "name");
      e.id = qualify(scope, e.name);
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      if (!owner.empty()) model_.relate({"contains", owner, e.id, loc});
      bindAnnotations(file, c, e.id, scope);
      std::map<std::string, std::string> participants;
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode s = ts_node_named_child(c, j);
        if (is(s, "participant")) {
          const std::string name = fieldText(file, s, "name");
          const std::string type = fieldText(file, s, "type");
          participants[name] = type;
          references_.push_back({e.id, type, scope, "uses", model::Kind::Class, locationOf(file, s)});
        } else if (is(s, "message") || is(s, "return")) {
          const std::string to = fieldText(file, s, "to");
          auto p = participants.find(to);
          if (p == participants.end()) {
            model_.report({"W-PARTICIPANT-UNDECLARED", "warning", "'" + to + "' is not a participant of " + e.id, locationOf(file, s)});
            continue;
          }
          messages_.push_back({e.id, p->second, scope, fieldText(file, s, "op"), locationOf(file, s)});
        }
      }

    } else if (is(c, "state")) {
      const std::string entityName = fieldText(file, c, "entity");
      model::Entity e;
      e.kind = model::Kind::State;
      e.name = entityName;
      e.id = qualify(scope, entityName) + "::state";
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      references_.push_back({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc});
      bindAnnotations(file, c, e.id, scope);
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode t = ts_node_named_child(c, j);
        if (is(t, "transition")) events_.push_back({e.id, entityName, scope, fieldText(file, t, "event"), locationOf(file, t)});
      }

    } else if (is(c, "diagram")) {
      model::Entity e;
      e.kind = model::Kind::Diagram;
      e.name = fieldText(file, c, "name");
      e.id = "diagram:" + e.name;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
    }
  }
}

}  // namespace bind
}  // namespace driftspec
