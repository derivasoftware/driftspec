#include "ds/bind/BindContext.hpp"

#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;

namespace {

// what an annotation means comes from the metamodel's table (Metamodel::knownAnnotations()), declared once
std::string relationForAnnotation(model::Kind from, const model::AnnotationSpec& spec) {
  return from == model::Kind::Verification && !spec.onVerification.empty() ? spec.onVerification : spec.relation;
}

}  // namespace

void BindContext::fields(TSNode node, model::Entity& out_entity) const {
  if (ts_node_is_null(node)) return;
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (TreeReader::is(c, "field")) {
      const std::string code = reader.fieldText(c, Field::code);
      const std::string key = reader.fieldText(c, Field::key) + (code.empty() ? "" : " " + code);
      const std::string value = std::string(text::trimWhitespace(reader.fieldText(c, Field::value)));
      auto existing = out_entity.fields.find(key);
      if (existing == out_entity.fields.end() || existing->second.empty()) {
        out_entity.fields[key] = value;
      } else {
        existing->second += ", " + value;  // a repeated key accumulates (include: a / include: b)
      }
    }
  }
}

void BindContext::annotations(TSNode node, const std::string& from, const std::string& scope) const {
  if (ts_node_is_null(node)) return;
  const model::Entity* entity = model.find(from);
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (!TreeReader::is(c, "annotation")) continue;
    const std::string kind = reader.fieldText(c, Field::kind);
    const model::AnnotationSpec& spec = metamodel.annotationOf(kind);
    if (spec.relation == "refers") {  // a word the language gives no meaning to: kept as a link, and said (DS-210)
      std::string known;
      for (const auto& a : metamodel.knownAnnotations()) known += (known.empty() ? "" : ", ") + a.name;
      model.report({"W-ANNOTATION-UNKNOWN", "warning",
                    "@" + kind + " is no annotation the language knows (" + known + "); it is kept as a link nothing reads",
                    locationOf(c)});
    }
    const std::string relation = relationForAnnotation(entity ? entity->kind : model::Kind::Class, spec);
    notes.references.push_back({from, reader.fieldText(c, Field::target), scope, relation, spec.target, locationOf(c), "", {}});
  }
}

void BindContext::extend(TSNode node, const std::string& id, model::Kind kind, const std::string& name, const std::string& scope,
                         const model::Location& loc) const {
  if (!model.find(id)) {
    // nobody has defined it yet (or ever): an expected node that a definition replaces (R6)
    model::Entity expected;
    expected.kind = kind;
    expected.id = id;
    expected.name = name;
    expected.scope = scope;
    expected.location = loc;
    expected.expected = true;
    expected.flags.push_back("extension");
    model.declare(expected);
  }
  annotations(node, id, scope);
}

/// An extension block: the same id declared again with annotations only (decided 2026-10-26).
/// It adds relations to an entity defined elsewhere; a definition field, member or
/// header item makes the block a definition, and a second definition is a duplicate.
bool BindContext::annotationsOnly(TSNode body, TSNode idNode) {
  if (ts_node_is_null(body)) return false;
  bool any = false;
  const uint32_t n = ts_node_named_child_count(body);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_named_child(body, i);
    if (TreeReader::is(c, "comment")) continue;
    if (!ts_node_is_null(idNode) && ts_node_eq(c, idNode)) continue;  // the id names the entity, it defines nothing
    if (!TreeReader::is(c, "annotation")) return false;
    any = true;
  }
  return any;
}

}  // namespace bind
}  // namespace ds
