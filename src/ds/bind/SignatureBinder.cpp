#include "ds/bind/SignatureBinder.hpp"

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

std::string SignatureBinder::identityOf(const parse::TreeReader& reader, TSNode node, const std::string& bare) {
  const std::string name = reader.fieldText(node, Field::name);
  const TSNode parent = ts_node_parent(node);
  bool overloaded = false;
  for (uint32_t i = 0; !ts_node_is_null(parent) && i < ts_node_named_child_count(parent) && !overloaded; ++i) {
    const TSNode other = ts_node_named_child(parent, i);
    overloaded =
        !ts_node_eq(other, node) && std::string(ts_node_type(other)) == ts_node_type(node) && reader.fieldText(other, Field::name) == name;
  }
  if (!overloaded) return bare;
  std::vector<std::string> types;
  for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
    const TSNode parameters = ts_node_named_child(node, i);
    if (!TreeReader::is(parameters, "parameters")) continue;
    for (uint32_t j = 0; j < ts_node_named_child_count(parameters); ++j) {
      const TSNode type = reader.child(ts_node_named_child(parameters, j), Field::type);
      std::string written, said = ts_node_is_null(type) ? "" : std::string(text::trimWhitespace(reader.textOf(type)));
      for (const char c : said) {  // whitespace normalised: a run of it is one space
        const bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (space && (written.empty() || written.back() == ' ')) continue;
        written += space ? ' ' : c;
      }
      types.push_back(written);
    }
  }
  return model::Identity(bare).withParameters(join(types, ", ")).text();
}

void SignatureBinder::bind(TSNode node, const BindContext& ctx, model::Entity& out_entity, const std::string& scope,
                           const std::vector<std::string>& params) const {
  TSNode parameters{};
  for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
    if (TreeReader::is(ts_node_named_child(node, i), "parameters")) parameters = ts_node_named_child(node, i);
  }
  std::vector<std::string> paramTexts;
  if (!ts_node_is_null(parameters)) {
    const uint32_t n = ts_node_named_child_count(parameters);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(parameters, i);
      paramTexts.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(p))));
      TSNode type = ctx.reader.child(p, Field::type);
      out_entity.params.push_back(
          {ctx.reader.fieldText(p, Field::name), ts_node_is_null(type) ? "" : std::string(text::trimWhitespace(ctx.reader.textOf(type)))});
      types_.bind(ctx.reader.child(p, Field::type), ctx, scope, out_entity.id, "uses", "", params, ctx.locationOf(p));
    }
  }
  out_entity.fields["params"] = join(paramTexts, ", ");  // parameters and returns give use (R16)
  const std::string returns =
      types_.bind(ctx.reader.child(node, Field::returns), ctx, scope, out_entity.id, "uses", "", params, ctx.locationOf(node));
  if (!returns.empty()) out_entity.fields["returns"] = returns;
  TSNode fails = ctx.reader.child(node, Field::fails);  // what it promises it may fail with; `fails nothing` promises none (DS-189)
  if (!ts_node_is_null(fails)) {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < ts_node_named_child_count(fails); ++i)
      names.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(ts_node_named_child(fails, i)))));
    out_entity.fields["fails"] = join(names, ", ");
  }
}

}  // namespace bind
}  // namespace ds
