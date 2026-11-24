#include "ds/bind/FieldBinder.hpp"

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

void FieldBinder::bind(TSNode node, const BindContext& ctx, model::Entity& out_entity) const {
  if (ts_node_is_null(node)) return;
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (TreeReader::is(c, "field")) {
      const std::string code = ctx.reader.fieldText(c, Field::code);
      const std::string key = ctx.reader.fieldText(c, Field::key) + (code.empty() ? "" : " " + code);
      const std::string value = std::string(text::trimWhitespace(ctx.reader.fieldText(c, Field::value)));
      auto existing = out_entity.fields.find(key);
      if (existing == out_entity.fields.end() || existing->second.empty()) {
        out_entity.fields[key] = value;
      } else {
        existing->second += ", " + value;  // a repeated key accumulates (include: a / include: b)
      }
    }
  }
}

}  // namespace bind
}  // namespace ds
