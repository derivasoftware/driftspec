#include "ds/bind/MemberBodyBinder.hpp"

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

void MemberBodyBinder::bind(TSNode member, const BindContext& ctx, model::Entity& out_entity, const std::string& scope) const {
  fields_.bind(member, ctx, out_entity);
  TSNode body = ctx.reader.child(member, Field::body);
  if (!ts_node_is_null(body)) fields_.bind(body, ctx, out_entity);
  const model::Entity* declared = ctx.find(out_entity.id);
  // only the declaration the model kept takes the fields: a duplicate is reported and leaves the first as it was
  const model::Location& at = declared ? declared->location : out_entity.location;
  if (declared && at.file == out_entity.location.file && at.line == out_entity.location.line && at.column == out_entity.location.column)
    for (const auto& f : out_entity.fields) ctx.annotate(out_entity.id, f.first, f.second);
  if (ts_node_is_null(body)) return;
  annotations_.bind(body, ctx, out_entity.id, scope);
  ctx.within(scope, out_entity.id).children(body);  // nested requirements and constraints are about the member (R12)
}

}  // namespace bind
}  // namespace ds
