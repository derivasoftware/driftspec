#include "ds/bind/DiagramBinder.hpp"

#include <algorithm>
#include <cstring>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Identity.hpp"
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
  bindSteps(node, ctx, e);
}

// a walkthrough's steps, each its own entity in the order written, named by its place (DS-468): what it shows and
// lights are references the linker relates when they name something, and leaves alone when they do not (DS-469 says so)
void DiagramBinder::bindSteps(TSNode node, const BindContext& ctx, const model::Entity& diagram) const {
  int n = 0;
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode s = ts_node_named_child(node, i);
    if (!TreeReader::is(s, "walk_step")) continue;
    model::Entity step;
    step.kind = model::Kind::Step;
    step.id = diagram.id + "::" + std::to_string(++n);
    step.name = std::string(text::trimWhitespace(ctx.reader.fieldText(s, Field::title)));
    step.scope = diagram.id;
    step.location = ctx.locationOf(s);
    fields_.bind(s, ctx, step);
    // show: DIAGRAM / scenario: the diagram, and the scenario of the behaviour it draws
    const std::string shown = step.field("show");
    const auto slash = shown.find('/');
    if (slash != std::string::npos) {
      step.fields["show"] = std::string(text::trimWhitespace(shown.substr(0, slash)));
      step.fields["scenario"] = std::string(text::trimWhitespace(shown.substr(slash + 1)));
    }
    ctx.declare(step);
    ctx.relate({"contains", diagram.id, step.id, step.location, ""});
    const auto refer = [&](const std::string& written, const char* relation, model::Kind expected) {
      if (written.empty()) return;
      bind::Reference r{step.id, written, "", relation, expected, step.location, "", {}};
      r.soft = true;  // what names nothing is told by the walkthrough's check, not the linker (DS-469)
      ctx.refer(r);
    };
    if (!step.field("show").empty()) refer(model::Identity::ofDiagram(step.field("show")).text(), "shows", model::Kind::Diagram);
    for (const auto& id : text::splitTrimmed(step.field("focus"), ',')) refer(id, "focuses", model::Kind::Class);
    for (const auto& id : text::splitTrimmed(step.field("highlight"), ',')) refer(id, "highlights", model::Kind::Class);
    refer(step.field("at"), "points-at", model::Kind::Class);
  }
}

}  // namespace bind
}  // namespace ds
