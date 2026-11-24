#include "ds/bind/ClassBinder.hpp"

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

std::vector<std::string> ClassBinder::kinds() const { return {"class"}; }

void ClassBinder::declareOperator(const BindContext& ctx, const model::Entity& cls, const std::string& symbol,
                                  const model::Location& location) const {
  model::Entity op;
  op.kind = model::Kind::Operator;
  op.name = "operator" + symbol;
  op.id = cls.id + "::" + op.name;
  op.scope = cls.id;
  op.location = location;
  op.fields["symbol"] = symbol;
  ctx.declare(op);
  ctx.relate({"contains", cls.id, op.id, location, ""});
}

void ClassBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  if (ctx.reader.fieldText(node, Field::visibility).empty() && ts_node_is_null(TreeReader::child(node, Field::params)) &&
      TreeReader::childrenNamed(node, Field::base).empty() && ts_node_is_null(TreeReader::child(node, Field::operators)) &&
      ExtensionBinder::extends(TreeReader::child(node, Field::body))) {
    const std::string name = ctx.reader.fieldText(node, Field::name);
    extensions_.bind(TreeReader::child(node, Field::body), ctx, model::Scope(scope).qualify(name).text(), model::Kind::Class, name, scope,
                     loc);  // the annotations sit in the body
    return;
  }
  model::Entity e;
  e.kind = model::Kind::Class;
  e.name = ctx.reader.fieldText(node, Field::name);
  e.scope = scope;
  e.location = loc;
  if (ctx.reader.fieldText(node, Field::kind) == "dataclass") e.flags.push_back("dataclass");
  if (ctx.reader.fieldText(node, Field::visibility) == "-") e.fields["visibility"] = "-";

  std::vector<std::string> params;
  std::map<std::string, std::string> bounds;
  TSNode paramsNode = TreeReader::child(node, Field::params);
  if (!ts_node_is_null(paramsNode)) {
    const uint32_t n = ts_node_named_child_count(paramsNode);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(paramsNode, i);
      const std::string name = ctx.reader.fieldText(p, Field::name);
      params.push_back(name);
      const std::string bound = ctx.reader.fieldText(p, Field::bound);
      e.params.push_back({name, bound});
      if (!bound.empty()) bounds[name] = bound;
    }
  }
  e.id = model::Scope(scope).qualify(params.empty() ? e.name : model::Identity(e.name).withArity(params.size()).text()).text();  // R15
  if (!params.empty()) e.fields["params"] = join(params, ", ");
  for (const auto& b : bounds) e.fields["bound." + b.first] = b.second;

  std::set<std::string> operators;
  std::vector<std::string> operatorTexts;
  TSNode opSet = TreeReader::child(node, Field::operators);
  if (!ts_node_is_null(opSet)) {
    const uint32_t n = ts_node_named_child_count(opSet);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode item = ts_node_named_child(opSet, i);
      const std::string text = std::string(text::trimWhitespace(ctx.reader.textOf(item)));
      operatorTexts.push_back(text);
      if (ts_node_is_null(TreeReader::child(item, Field::family))) operators.insert(text);
    }
    e.fields["operators"] = join(operatorTexts, ", ");
  }

  TSNode body = TreeReader::child(node, Field::body);
  fields_.bind(body, ctx, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  // a base is written as the code writes it (IFormat<scene::SceneDocument>) and kept so; its first name is what the
  // class inherits, and only when the design declares it: class Error : Exception is text, no arrow (DS-18)
  std::vector<std::string> baseTexts;
  for (TSNode base : TreeReader::childrenNamed(node, Field::base))
    baseTexts.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(base))));
  if (!baseTexts.empty()) ctx.annotate(declared.id, "bases", join(baseTexts, "; "));
  for (const std::string& written : baseTexts) {
    const std::vector<TypeToken> tokens = scanTypeTokens(written);
    if (tokens.empty()) continue;
    const TypeToken& token = tokens.front();
    std::vector<std::string> args = token.args;
    for (auto& a : args)
      if (std::find(params.begin(), params.end(), a) != params.end())
        a.clear();  // class Holder<T> : Base<T>: T is Holder's parameter, not a class T
    Reference ref{declared.id,
                  token.args.empty() ? token.name : model::Identity(token.name).withArity(token.args.size()).text(),
                  scope,
                  "inherits",
                  model::Kind::Class,
                  loc,
                  "",
                  args};
    ref.soft = true;
    ctx.refer(ref);
  }
  for (const auto& b : bounds) {
    Reference bound{declared.id, b.second, scope, "bounded-by", model::Kind::Class, loc, "", {}};
    bound.soft = true;  // a bound nobody designs is text, as a base is
    ctx.refer(bound);
  }
  for (const auto& symbol : operators) declareOperator(ctx, declared, symbol, loc);
  if (ts_node_is_null(body)) return;
  annotations_.bind(body, ctx, declared.id, scope);
  const uint32_t n = ts_node_named_child_count(body);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode m = ts_node_named_child(body, j);
    if (TreeReader::is(m, "method") || TreeReader::is(m, "attribute") || TreeReader::is(m, "operator"))
      members_.bind(m, ctx, declared, params, operators);
  }
  ctx.within(scope, declared.id).children(body);  // nested requirements and constraints are about the class (R12)
}

}  // namespace bind
}  // namespace ds
