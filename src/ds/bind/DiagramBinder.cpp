#include "ds/bind/DiagramBinder.hpp"

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

std::vector<std::string> DiagramBinder::kinds() const { return {"diagram"}; }

void DiagramBinder::bind(TSNode node, const BindContext& ctx) const {
  const model::Location loc = ctx.locationOf(node);
  model::Entity e;
  e.kind = model::Kind::Diagram;
  e.name = ctx.reader.fieldText(node, Field::name);
  e.id = model::Identity::ofDiagram(e.name).text();
  e.location = loc;
  fields_.bind(node, ctx, e);
  formulas_.bind(node, ctx, e);  // a table's rows and columns (DS-268)
  ctx.declare(e);
}

}  // namespace bind
}  // namespace ds
