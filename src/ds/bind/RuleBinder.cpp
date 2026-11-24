#include "ds/bind/RuleBinder.hpp"

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

std::vector<std::string> RuleBinder::kinds() const { return {"rule", "definition"}; }

void RuleBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  model::Entity e;
  e.kind = TreeReader::is(node, "rule") ? model::Kind::Rule : model::Kind::Definition;
  e.name = ctx.reader.fieldText(node, Field::name);
  const model::Entity* in = owner.empty() ? nullptr : ctx.find(owner);
  // a rule of a class or a member is named under it: p@m::Order::priced, p@m::Link::send::in-range
  const bool inClass = in && (in->kind == model::Kind::Class || in->kind == model::Kind::Method || in->kind == model::Kind::Function ||
                              in->kind == model::Kind::Operator || in->kind == model::Kind::Attribute);  // DS-224
  e.id = inClass ? owner + "::" + e.name : model::Scope(scope).qualify(e.name).text();
  e.scope = inClass ? owner : scope;
  e.location = loc;
  fields_.bind(node, ctx, e);
  formulas_.bind(node, ctx, e);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, e.id, loc, ""});
  annotations_.bind(node, ctx, e.id, scope);
}

}  // namespace bind
}  // namespace ds
