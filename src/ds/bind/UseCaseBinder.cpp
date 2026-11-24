#include "ds/bind/UseCaseBinder.hpp"

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

std::vector<std::string> UseCaseBinder::kinds() const { return {"usecase"}; }

void UseCaseBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  model::Entity e;
  e.kind = model::Kind::UseCase;
  e.name = ctx.reader.fieldText(node, Field::id);
  e.id = e.name;
  e.scope = scope;
  e.location = ctx.locationOf(node);
  const std::string operation = ctx.reader.fieldText(node, Field::name);
  if (!operation.empty()) {
    e.fields["operation"] = operation;
    signatures_.bind(node, ctx, e, scope, {});
  }
  fields_.bind(node, ctx, e);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"about", e.id, owner, e.location, ""});
  const std::string base = ctx.reader.fieldText(node, Field::base);
  // the use case it specialises is resolved like any artefact link: pinned, followed to its revision (DS-116)
  if (!base.empty())
    ctx.refer({e.id, std::string(text::trimWhitespace(base)), scope, "inherits", model::Kind::UseCase, e.location, "", {}});
  annotations_.bind(node, ctx, e.id, scope);
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode o = ts_node_named_child(node, i);
    if (!TreeReader::is(o, "outcome")) continue;
    model::Entity out;
    out.kind = model::Kind::Outcome;
    out.name = ctx.reader.fieldText(o, Field::name);
    out.id = e.id + "::" + out.name;
    out.scope = e.id;
    out.location = ctx.locationOf(o);
    out.fields["text"] = std::string(text::trimWhitespace(ctx.reader.fieldText(o, Field::text)));
    const std::string returns = types_.bind(TreeReader::child(o, Field::returns), ctx, scope, out.id, "uses", "", {}, out.location);
    if (!returns.empty()) out.fields["returns"] = returns;
    ctx.declare(out);
    ctx.relate({"contains", e.id, out.id, out.location, ""});
  }
}

}  // namespace bind
}  // namespace ds
