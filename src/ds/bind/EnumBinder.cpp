#include "ds/bind/EnumBinder.hpp"

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

std::vector<std::string> EnumBinder::kinds() const { return {"enum"}; }

void EnumBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  model::Entity e;
  e.kind = model::Kind::Enum;
  e.name = ctx.reader.fieldText(node, Field::name);
  if (ctx.reader.fieldText(node, Field::visibility) == "-") e.fields["visibility"] = "-";
  e.id = model::Scope(scope).qualify(e.name).text();
  e.scope = scope;
  e.location = loc;
  std::vector<std::string> values;
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode v = ts_node_named_child(node, j);
    if (TreeReader::is(v, "enum_value")) values.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(v))));
  }
  e.fields["values"] = join(values, ", ");
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, e.id, loc, ""});
  annotations_.bind(node, ctx, e.id, scope);  // an enum realises requirements like a class
}

}  // namespace bind
}  // namespace ds
