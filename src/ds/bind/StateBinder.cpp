#include "ds/bind/StateBinder.hpp"

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

std::vector<std::string> StateBinder::kinds() const { return {"state"}; }

void StateBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const model::Location loc = ctx.locationOf(node);
  const std::string entityName = ctx.reader.fieldText(node, Field::entity);
  model::Entity e;
  e.kind = model::Kind::State;
  e.name = entityName;
  e.id = model::Identity(model::Scope(scope).qualify(entityName).text()).state().text();
  e.scope = scope;
  e.location = loc;
  fields_.bind(node, ctx, e);
  // its transitions and its rules, read once as records before it is declared (DS-384)
  const uint32_t written = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < written; ++j) {
    TSNode t = ts_node_named_child(node, j);
    if (TreeReader::is(t, "state_rule")) {  // a state computed by a rule (DS-249)
      model::WrittenRule rule{ctx.reader.fieldText(t, Field::name), {}, ctx.locationOf(t)};
      for (uint32_t x = 0; x < ts_node_named_child_count(t); ++x) {
        TSNode l = ts_node_named_child(t, x);
        if (!TreeReader::is(l, "state_literal")) continue;
        rule.literals.push_back({!ctx.reader.fieldText(l, Field::negated).empty(), ctx.reader.fieldText(l, Field::left),
                                 ctx.reader.fieldText(l, Field::operator_), ctx.reader.fieldText(l, Field::right)});
      }
      e.rules.push_back(rule);
    }
    if (!TreeReader::is(t, "transition")) continue;
    const TSNode guard = ctx.reader.child(t, Field::guard);
    e.transitions.push_back({ctx.reader.fieldText(t, Field::from), ctx.reader.fieldText(t, Field::to),
                             ctx.reader.fieldText(t, Field::event),
                             ts_node_is_null(guard) ? "" : std::string(text::trimWhitespace(ctx.reader.fieldText(guard, Field::body))),
                             ctx.reader.fieldText(t, Field::action), ctx.locationOf(t)});
  }
  ctx.declare(e);
  ctx.refer({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  annotations_.bind(node, ctx, e.id, scope);
  std::vector<std::string> finals;  // what every `final` line names, in order (DS-442)
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode t = ts_node_named_child(node, j);
    if (TreeReader::is(t, "initial")) ctx.annotate(e.id, "initial", ctx.reader.fieldText(t, Field::name));
    if (TreeReader::is(t, "final")) {
      for (TSNode name : ctx.reader.childrenNamed(t, Field::name)) finals.push_back(ctx.reader.textOf(name));
      ctx.annotate(e.id, "final", join(finals, ", "));
    }
    if (TreeReader::is(t, "view")) {
      model::Entity v;
      v.kind = model::Kind::View;
      v.name = ctx.reader.fieldText(t, Field::name);
      v.id = e.id + "::" + v.name;
      v.scope = e.id;
      v.location = ctx.locationOf(t);
      fields_.bind(t, ctx, v);
      ctx.declare(v);
      ctx.relate({"contains", e.id, v.id, v.location, ""});
      annotations_.bind(t, ctx, v.id, scope);
    }
    if (!TreeReader::is(t, "transition")) continue;
    ctx.noteEvent({e.id, entityName, scope, ctx.reader.fieldText(t, Field::event), false, ctx.locationOf(t)});
    const std::string action = ctx.reader.fieldText(t, Field::action);
    if (!action.empty()) ctx.noteEvent({e.id, entityName, scope, action, true, ctx.locationOf(t)});
  }
}

}  // namespace bind
}  // namespace ds
