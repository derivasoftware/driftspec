#include "ds/bind/AnnotationBinder.hpp"

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

std::string AnnotationBinder::relationOf(model::Kind from, const model::AnnotationSpec& spec) {
  return from == model::Kind::Verification && !spec.onVerification.empty() ? spec.onVerification : spec.relation;
}

void AnnotationBinder::bind(TSNode node, const BindContext& ctx, const std::string& from, const std::string& scope) const {
  if (ts_node_is_null(node)) return;
  const model::Entity* entity = ctx.find(from);
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (!TreeReader::is(c, "annotation")) continue;
    const std::string kind = ctx.reader.fieldText(c, Field::kind);
    const model::AnnotationSpec& spec = ctx.metamodel.annotationOf(kind);
    if (spec.relation == "refers") {  // a word the language gives no meaning to: kept as a link, and said (DS-210)
      std::string known;
      for (const auto& a : ctx.metamodel.knownAnnotations()) known += (known.empty() ? "" : ", ") + a.name;
      ctx.report(ctx.metamodel.finding(
          model::Code::W_ANNOTATION_UNKNOWN,
          "@" + kind + " is no annotation the language knows (" + known + "); it is kept as a link nothing reads", ctx.locationOf(c)));
    }
    const std::string relation = relationOf(entity ? entity->kind : model::Kind::Class, spec);
    ctx.refer({from, ctx.reader.fieldText(c, Field::target), scope, relation, spec.target, ctx.locationOf(c), "", {}});
  }
}

}  // namespace bind
}  // namespace ds
