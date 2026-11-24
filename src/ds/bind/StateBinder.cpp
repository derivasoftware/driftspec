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
  ctx.declare(e);
  ctx.refer({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  annotations_.bind(node, ctx, e.id, scope);
  std::vector<std::string> transitions;
  std::vector<std::string> rules;
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode t = ts_node_named_child(node, j);
    if (TreeReader::is(t, "initial")) ctx.annotate(e.id, "initial", ctx.reader.fieldText(t, Field::name));
    if (TreeReader::is(t, "state_rule")) {  // a state computed by a rule (DS-249): state|literal,literal|line, `not` as a leading !
      std::vector<std::string> literals;
      const uint32_t nl = ts_node_named_child_count(t);
      for (uint32_t x = 0; x < nl; ++x) {
        TSNode l = ts_node_named_child(t, x);
        if (!TreeReader::is(l, "state_literal")) continue;
        literals.push_back((ctx.reader.fieldText(l, Field::negated).empty() ? "" : "!") + ctx.reader.fieldText(l, Field::left) +
                           ctx.reader.fieldText(l, Field::operator_) + ctx.reader.fieldText(l, Field::right));
      }
      rules.push_back(ctx.reader.fieldText(t, Field::name) + "|" + join(literals, ",") + "|" + std::to_string(ctx.locationOf(t).line));
      ctx.annotate(e.id, "rules", join(rules, "\n"));
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
    const std::string guard = std::string(text::trimWhitespace(ctx.reader.fieldText(t, Field::guard)));
    const std::string does = ctx.reader.fieldText(t, Field::action);
    transitions.push_back(ctx.reader.fieldText(t, Field::from) + ">" + ctx.reader.fieldText(t, Field::to) + ">" +
                          ctx.reader.fieldText(t, Field::event) + (guard.empty() ? "" : " " + guard) + (does.empty() ? "" : " / " + does));
    ctx.annotate(e.id, "transitions", join(transitions, "\n"));  // from>to>label, one per line
    ctx.noteEvent({e.id, entityName, scope, ctx.reader.fieldText(t, Field::event), false, ctx.locationOf(t)});
    const std::string action = ctx.reader.fieldText(t, Field::action);
    if (!action.empty()) ctx.noteEvent({e.id, entityName, scope, action, true, ctx.locationOf(t)});
  }
}

}  // namespace bind
}  // namespace ds
