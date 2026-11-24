#include "ds/bind/ExtensionBinder.hpp"

#include <algorithm>
#include <cstring>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;
using text::join;

/// An extension block: the same id declared again with annotations only (decided 2026-10-26).
/// It adds relations to an entity defined elsewhere; a definition field, member or
/// header item makes the block a definition, and a second definition is a duplicate.
bool ExtensionBinder::extends(TSNode body, TSNode idNode) {
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

void ExtensionBinder::bind(TSNode node, const BindContext& ctx, const std::string& id, model::Kind kind, const std::string& name,
                           const std::string& scope, const model::Location& loc) const {
  if (!ctx.find(id)) {
    // nobody has defined it yet (or ever): an expected node that a definition replaces (R6)
    model::Entity expected;
    expected.kind = kind;
    expected.id = id;
    expected.name = name;
    expected.scope = scope;
    expected.location = loc;
    expected.expected = true;
    expected.flags.push_back("extension");
    ctx.declare(expected);
  }
  annotations_.bind(node, ctx, id, scope);
}

}  // namespace bind
}  // namespace ds
