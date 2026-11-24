#include "ds/bind/ModuleBinder.hpp"

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

std::vector<std::string> ModuleBinder::kinds() const { return {"module"}; }

void ModuleBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  model::Entity e;
  e.kind = model::Kind::Module;
  e.name = ctx.reader.fieldText(node, Field::name);
  e.id = model::Scope(scope).qualify(e.name).text();
  e.scope = scope;
  e.location = loc;
  const bool isPrivate = ctx.reader.fieldText(node, Field::visibility) == "-";
  if (isPrivate) e.fields["visibility"] = "-";
  fields_.bind(node, ctx, e);
  const bool reopened = ctx.find(e.id) != nullptr;  // a module reopened adds no relation (DS-48)
  const model::Entity& declared = ctx.declare(e);
  for (const auto& f : e.fields) {  // a module reopens: its fields add up, and two that differ are both kept
    const std::string& kept = declared.field(f.first);
    if (kept.empty())
      ctx.annotate(declared.id, f.first, f.second);
    else if (kept != f.second && (", " + kept + ",").find(", " + f.second + ",") == std::string::npos)
      ctx.annotate(declared.id, f.first, kept + ", " + f.second);
  }
  if (isPrivate) ctx.annotate(declared.id, "visibility", "-");  // a module reopens: any declaration may say it
  if (!owner.empty() && !reopened) ctx.relate({"contains", owner, declared.id, loc, ""});
  annotations_.bind(node, ctx, declared.id, declared.id);  // a module answers to its requirements, as a package does
  ctx.within(declared.id, declared.id).children(node);
}

}  // namespace bind
}  // namespace ds
