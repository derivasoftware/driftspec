#include "ds/bind/TypeBinder.hpp"

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

std::vector<std::string> TypeBinder::params(const parse::TreeReader& reader, TSNode node) {
  std::vector<std::string> out;
  if (ts_node_is_null(node)) return out;
  for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) out.push_back(reader.fieldText(ts_node_named_child(node, i), Field::name));
  return out;
}

bool TypeBinder::isOwnership(const std::string& relation) { return relation == "owns" || relation == "refs" || relation == "shares"; }

bool TypeBinder::contains(const std::vector<std::string>& items, const std::string& item) {
  for (const auto& i : items) {
    if (i == item) return true;
  }
  return false;
}

std::string TypeBinder::bind(TSNode type, const BindContext& ctx, const std::string& scope, const std::string& from,
                             const std::string& relation, const std::string& multiplicity, const std::vector<std::string>& params,
                             const model::Location& location) const {
  if (ts_node_is_null(type)) return "";
  const std::string text = std::string(text::trimWhitespace(ctx.reader.textOf(type)));
  for (const TypeToken& token : scanTypeTokens(text)) {
    if (contains(params, token.name)) continue;  // a template parameter may be anything (R15)
    Reference ref;
    ref.from = from;
    ref.target = token.args.empty() ? token.name : model::Identity(token.name).withArity(token.args.size()).text();
    ref.scope = scope;
    ref.relation = relation;
    ref.expectedKind = model::Kind::Class;
    ref.location = location;
    ref.args = token.args;
    for (auto& a : ref.args)
      if (contains(params, a)) a.clear();  // Repository<T> inside a template of T: a parameter, no bound to check
    // the multiplicity is the one written (many, optional); an ownership without one holds one (DS-18)
    ref.multiplicity = isOwnership(relation) ? (multiplicity.empty() ? "1" : multiplicity) : "";
    ref.soft = true;
    ctx.refer(ref);
  }
  return text;
}

}  // namespace bind
}  // namespace ds
