#include "ds/bind/MemberBinder.hpp"

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

void MemberBinder::bind(TSNode m, const BindContext& ctx, const model::Entity& cls, const std::vector<std::string>& params,
                        const std::set<std::string>& operators) const {
  const model::Location loc = ctx.locationOf(m);
  const std::string scope = cls.scope;
  model::Entity member;
  member.scope = cls.id;
  member.location = loc;

  if (TreeReader::is(m, "operator")) {
    const std::string symbol = ctx.reader.fieldText(m, Field::symbol);
    member.kind = model::Kind::Operator;
    member.name = "operator" + symbol;
    member.id = cls.id + "::" + member.name;
    member.fields["symbol"] = symbol;
    if (operators.count(symbol) == 0) {
      ctx.report(ctx.metamodel.finding(model::Code::E_OPERATOR_UNDECLARED,
                                       "operator " + symbol + " is refined in the body of " + cls.id + " but is not in its header set", loc,
                                       member.id));
    }
  } else {
    member.kind = TreeReader::is(m, "method") ? model::Kind::Method : model::Kind::Attribute;
    member.name = ctx.reader.fieldText(m, Field::name);
    member.id = TreeReader::is(m, "method") ? SignatureBinder::identityOf(ctx.reader, m, cls.id + "::" + member.name)  // DS-455
                                            : cls.id + "::" + member.name;
    member.fields["visibility"] = ctx.reader.fieldText(m, Field::visibility);
  }

  if (TreeReader::is(m, "method") || TreeReader::is(m, "operator")) {
    for (TSNode mod : ctx.reader.childrenNamed(m, Field::modifier)) member.flags.push_back(ctx.reader.textOf(mod));
    if (TreeReader::is(m, "method") && member.name == cls.name) member.flags.push_back("constructor");
    if (TreeReader::is(m, "method") && member.name == "~" + cls.name) member.flags.push_back("destructor");  // DS-143
    if (cls.has("dataclass") && member.has("abstract")) {
      ctx.report(ctx.metamodel.finding(model::Code::E_DATACLASS_ABSTRACT,
                                       cls.id + " is a dataclass: a value type has no abstract methods (" + member.name + ")", loc,
                                       cls.id));
    }
    std::vector<std::string> own = params;  // a templated method's own parameters are no entities either (DS-18)
    for (const std::string& t : TypeBinder::params(ctx.reader, ctx.reader.child(m, Field::params))) own.push_back(t);
    if (own.size() > params.size())
      member.fields["template"] = join(std::vector<std::string>(own.begin() + params.size(), own.end()), ", ");
    signatures_.bind(m, ctx, member, scope, own);
  } else {
    const std::string ownership = ctx.reader.fieldText(m, Field::ownership);
    const std::string relation = ownership.empty() ? "owns" : ownership;  // owns by default (R16)
    member.flags.push_back(relation);
    // the multiplicity is written, never read from the type: many is *, optional is 0..1 (DS-18). One ownership
    // and one multiplicity at most: two of either say two things at once, and the first written is kept
    std::vector<std::string> owning, counting;
    const uint32_t children = ts_node_child_count(m);
    for (uint32_t i = 0; i < children; ++i) {
      const char* f = ts_node_field_name_for_child(m, i);
      if (!f) continue;
      if (std::strcmp(f, "ownership") == 0) owning.push_back(ctx.reader.textOf(ts_node_child(m, i)));
      if (std::strcmp(f, "many") == 0 || std::strcmp(f, "optional") == 0) counting.push_back(ctx.reader.textOf(ts_node_child(m, i)));
    }
    for (const auto* said : {&owning, &counting}) {
      if (said->size() < 2) continue;
      const bool aboutOwnership = said == &owning;
      ctx.report(ctx.metamodel.finding(model::Code::E_MODIFIER_CONFLICT,
                                       member.id + " is written " + join(*said, " ") + ": an attribute has at most one " +
                                           (aboutOwnership ? "ownership (owns, refs or shares)" : "multiplicity (many or optional)"),
                                       loc, member.id));
    }
    const std::string counted = counting.empty() ? "" : counting.front();
    if (!counted.empty()) member.flags.push_back(counted);
    member.fields["type"] = types_.bind(ctx.reader.child(m, Field::type), ctx, scope, member.id, relation,
                                        counted == "many"       ? "*"
                                        : counted == "optional" ? "0..1"
                                                                : "",
                                        params, loc);
    const std::string def = ctx.reader.fieldText(m, Field::default_);
    if (!def.empty()) member.fields["default"] = def;
  }

  if (member.kind == model::Kind::Operator && ctx.find(member.id)) {
    ctx.refine(member);  // declared by the header set: the body refines it
  } else {
    ctx.declare(member);
    ctx.relate({"contains", cls.id, member.id, loc, ""});
  }
  bodies_.bind(m, ctx, member, scope);
}

}  // namespace bind
}  // namespace ds
