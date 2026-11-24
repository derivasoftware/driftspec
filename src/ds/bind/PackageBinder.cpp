#include "ds/bind/PackageBinder.hpp"

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

std::vector<std::string> PackageBinder::kinds() const { return {"package"}; }

void PackageBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  // the flat form `package app@render {` reopens render inside app: the prefixes are the scope
  std::string prefix;
  const uint32_t parts = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < parts; ++j) {
    TSNode part = ts_node_named_child(node, j);
    if (TreeReader::is(part, "package_prefix")) prefix += ctx.reader.textOf(part);
  }
  model::Entity e;
  e.kind = model::Kind::Package;
  e.name = ctx.reader.fieldText(node, Field::name);
  e.scope = prefix.empty() ? scope : prefix;
  e.id = e.scope + e.name;  // every package level adds its name with @: app, app@core, app@core@model::Entity (R4)
  if (ctx.reader.fieldText(node, Field::visibility) == "-") e.fields["visibility"] = "-";
  e.location = loc;
  fields_.bind(node, ctx, e);
  const bool reopened = ctx.find(e.id) != nullptr;  // a box reopened adds no relation (DS-48)
  const model::Entity& declared = ctx.declare(e);
  for (const auto& f : e.fields) ctx.annotate(declared.id, f.first, f.second);
  annotations_.bind(node, ctx, declared.id, declared.id + "@");  // an HLD box realises requirements like a class does
  if (!reopened && !owner.empty())
    ctx.relate({"contains", owner, declared.id, loc, ""});
  else if (!reopened && !prefix.empty())  // the outer package must be declared somewhere; the checker turns part-of into contains
    ctx.refer({declared.id, prefix.substr(0, prefix.size() - 1), "", "part-of", model::Kind::Package, loc, "", {}});
  ctx.within(declared.id + "@", declared.id).children(node);
}

}  // namespace bind
}  // namespace ds
