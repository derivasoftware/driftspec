#include "ds/bind/FunctionBinder.hpp"

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

std::vector<std::string> FunctionBinder::kinds() const { return {"function"}; }

void FunctionBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  model::Entity e;
  e.kind = model::Kind::Function;
  e.name = ctx.reader.fieldText(node, Field::name);
  // ds@ds::load: a function is named like a class of its module; an overload carries its types (DS-455)
  e.id = SignatureBinder::identityOf(ctx.reader, node, model::Scope(scope).qualify(e.name).text());
  e.scope = scope;
  e.location = ctx.locationOf(node);
  const std::string visibility = ctx.reader.fieldText(node, Field::visibility);
  e.fields["visibility"] = visibility.empty() ? "+" : visibility;
  const std::vector<std::string> own =
      TypeBinder::params(ctx.reader, ctx.reader.child(node, Field::params));  // a templated function (DS-18)
  if (!own.empty()) e.fields["template"] = join(own, ", ");
  signatures_.bind(node, ctx, e, scope, own);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, e.id, e.location, ""});
  bodies_.bind(node, ctx, e, scope);  // annotations, rules and nested artefacts, as a method's
}

}  // namespace bind
}  // namespace ds
