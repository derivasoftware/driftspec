#include "ds/bind/InterfaceBinder.hpp"

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

std::vector<std::string> InterfaceBinder::kinds() const { return {"interface"}; }

void InterfaceBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  model::Entity e;
  e.kind = model::Kind::Interface;
  e.name = ctx.reader.fieldText(node, Field::name);
  e.id = model::Scope(scope).qualify(e.name).text();  // hola@shell: an external interface of the box
  e.scope = scope;
  e.location = loc;
  fields_.bind(node, ctx, e);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"exposes", owner, e.id, loc, ""});
  annotations_.bind(node, ctx, e.id, scope);
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode op = ts_node_named_child(node, j);
    if (!TreeReader::is(op, "operation")) continue;
    model::Entity o;
    o.kind = model::Kind::Operation;
    o.name = ctx.reader.fieldText(op, Field::name);
    o.id = SignatureBinder::identityOf(ctx.reader, op, e.id + "::" + o.name);  // an overload carries its types (DS-455)
    o.scope = e.id;
    o.location = ctx.locationOf(op);
    signatures_.bind(op, ctx, o, scope, {});
    fields_.bind(op, ctx, o);
    ctx.declare(o);
    ctx.relate({"contains", e.id, o.id, o.location, ""});
    const std::string realised = ctx.reader.fieldText(op, Field::realises);  // operation greet(...) : UC-00001-1
    if (!realised.empty())  // the use case it realises: an artefact link, resolved and pinned (DS-116)
      ctx.refer({o.id, std::string(text::trimWhitespace(realised)), scope, "realises", model::Kind::UseCase, o.location, "", {}});
    annotations_.bind(op, ctx, o.id, scope);
  }
}

}  // namespace bind
}  // namespace ds
