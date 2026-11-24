#include "ds/bind/SignatureBinder.hpp"

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
      TSNode type = TreeReader::child(p, Field::type);
      out_entity.params.push_back(
          {ctx.reader.fieldText(p, Field::name), ts_node_is_null(type) ? "" : std::string(text::trimWhitespace(ctx.reader.textOf(type)))});
      types_.bind(TreeReader::child(p, Field::type), ctx, scope, out_entity.id, "uses", "", params, ctx.locationOf(p));
    }
  }
  out_entity.fields["params"] = join(paramTexts, ", ");  // parameters and returns give use (R16)
  const std::string returns =
      types_.bind(TreeReader::child(node, Field::returns), ctx, scope, out_entity.id, "uses", "", params, ctx.locationOf(node));
  if (!returns.empty()) out_entity.fields["returns"] = returns;
  TSNode fails = TreeReader::child(node, Field::fails);  // what it promises it may fail with; `fails nothing` promises none (DS-189)
  if (!ts_node_is_null(fails)) {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < ts_node_named_child_count(fails); ++i)
      names.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(ts_node_named_child(fails, i)))));
    out_entity.fields["fails"] = join(names, ", ");
  }
}

}  // namespace bind
}  // namespace ds
