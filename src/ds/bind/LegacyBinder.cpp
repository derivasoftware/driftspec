#include "ds/bind/LegacyBinder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;
using text::join;
using text::splitTrimmed;

// ---- process artefacts: requirement, verification, constraint (R5, R18) ------

// A use case is an artefact identified by its id (R5) and a contract: its signature, the base it
// specialises, its outcomes as entities of their own (DS-77).
// ---- types (R15, R16) ----------------------------------------------------------

// A type is the code's text (DS-18). Its names are read out of it; each that the design declares is related
// with the holder's relation (owns by default, refs or shares as written, uses for a signature or a local).
// ---- classes (R14, R15, R17, R19) ----------------------------------------------

void LegacyBinder::declareOperator(const BindContext& ctx, const model::Entity& cls, const std::string& symbol,
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

void LegacyBinder::bindMemberBody(const BindContext& ctx, TSNode member, model::Entity& out_entity, const std::string& scope) const {
  fields_.bind(member, ctx, out_entity);
  TSNode body = TreeReader::child(member, Field::body);
  if (!ts_node_is_null(body)) fields_.bind(body, ctx, out_entity);
  const model::Entity* declared = ctx.find(out_entity.id);
  // only the declaration the model kept takes the fields: a duplicate is reported and leaves the first as it was
  const model::Location& at = declared ? declared->location : out_entity.location;
  if (declared && at.file == out_entity.location.file && at.line == out_entity.location.line && at.column == out_entity.location.column)
    for (const auto& f : out_entity.fields) ctx.annotate(out_entity.id, f.first, f.second);
  if (ts_node_is_null(body)) return;
  annotations_.bind(body, ctx, out_entity.id, scope);
  ctx.within(scope, out_entity.id).children(body);  // nested requirements and constraints are about the member (R12)
}

void LegacyBinder::bindMember(const BindContext& ctx, TSNode m, const model::Entity& cls, const std::vector<std::string>& params,
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
      ctx.report({"E-OPERATOR-UNDECLARED", "error",
                  "operator " + symbol + " is refined in the body of " + cls.id + " but is not in its header set", loc});
    }
  } else {
    member.kind = TreeReader::is(m, "method") ? model::Kind::Method : model::Kind::Attribute;
    member.name = ctx.reader.fieldText(m, Field::name);
    member.id = cls.id + "::" + member.name;
    member.fields["visibility"] = ctx.reader.fieldText(m, Field::visibility);
  }

  if (TreeReader::is(m, "method") || TreeReader::is(m, "operator")) {
    for (TSNode mod : TreeReader::childrenNamed(m, Field::modifier)) member.flags.push_back(ctx.reader.textOf(mod));
    if (TreeReader::is(m, "method") && member.name == cls.name) member.flags.push_back("constructor");
    if (TreeReader::is(m, "method") && member.name == "~" + cls.name) member.flags.push_back("destructor");  // DS-143
    if (cls.has("dataclass") && member.has("abstract")) {
      ctx.report(
          {"E-DATACLASS-ABSTRACT", "error", cls.id + " is a dataclass: a value type has no abstract methods (" + member.name + ")", loc});
    }
    std::vector<std::string> own = params;  // a templated method's own parameters are no entities either (DS-18)
    for (const std::string& t : TypeBinder::params(ctx.reader, TreeReader::child(m, Field::params))) own.push_back(t);
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
      ctx.report({"E-MODIFIER-CONFLICT", "error",
                  member.id + " is written " + join(*said, " ") + ": an attribute has at most one " +
                      (aboutOwnership ? "ownership (owns, refs or shares)" : "multiplicity (many or optional)"),
                  loc});
    }
    const std::string counted = counting.empty() ? "" : counting.front();
    if (!counted.empty()) member.flags.push_back(counted);
    member.fields["type"] = types_.bind(TreeReader::child(m, Field::type), ctx, scope, member.id, relation,
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
  bindMemberBody(ctx, m, member, scope);
}

void LegacyBinder::bindFunction(const BindContext& ctx, TSNode f) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  model::Entity e;
  e.kind = model::Kind::Function;
  e.name = ctx.reader.fieldText(f, Field::name);
  e.id = model::Scope(scope).qualify(e.name).text();  // ds@ds::load: a function is named like a class of its module
  e.scope = scope;
  e.location = ctx.locationOf(f);
  const std::string visibility = ctx.reader.fieldText(f, Field::visibility);
  e.fields["visibility"] = visibility.empty() ? "+" : visibility;
  const std::vector<std::string> own = TypeBinder::params(ctx.reader, TreeReader::child(f, Field::params));  // a templated function (DS-18)
  if (!own.empty()) e.fields["template"] = join(own, ", ");
  signatures_.bind(f, ctx, e, scope, own);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, e.id, e.location, ""});
  bindMemberBody(ctx, f, e, scope);  // annotations, rules and nested artefacts, as a method's
}

void LegacyBinder::bindClass(const BindContext& ctx, TSNode c) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(c);
  if (ctx.reader.fieldText(c, Field::visibility).empty() && ts_node_is_null(TreeReader::child(c, Field::params)) &&
      TreeReader::childrenNamed(c, Field::base).empty() && ts_node_is_null(TreeReader::child(c, Field::operators)) &&
      ExtensionBinder::extends(TreeReader::child(c, Field::body))) {
    const std::string name = ctx.reader.fieldText(c, Field::name);
    extensions_.bind(TreeReader::child(c, Field::body), ctx, model::Scope(scope).qualify(name).text(), model::Kind::Class, name, scope,
                     loc);  // the annotations sit in the body
    return;
  }
  model::Entity e;
  e.kind = model::Kind::Class;
  e.name = ctx.reader.fieldText(c, Field::name);
  e.scope = scope;
  e.location = loc;
  if (ctx.reader.fieldText(c, Field::kind) == "dataclass") e.flags.push_back("dataclass");
  if (ctx.reader.fieldText(c, Field::visibility) == "-") e.fields["visibility"] = "-";

  std::vector<std::string> params;
  std::map<std::string, std::string> bounds;
  TSNode paramsNode = TreeReader::child(c, Field::params);
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
  TSNode opSet = TreeReader::child(c, Field::operators);
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

  TSNode body = TreeReader::child(c, Field::body);
  fields_.bind(body, ctx, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  // a base is written as the code writes it (IFormat<scene::SceneDocument>) and kept so; its first name is what the
  // class inherits, and only when the design declares it: class Error : Exception is text, no arrow (DS-18)
  std::vector<std::string> baseTexts;
  for (TSNode base : TreeReader::childrenNamed(c, Field::base))
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
      bindMember(ctx, m, declared, params, operators);
  }
  ctx.within(scope, declared.id).children(body);  // nested requirements and constraints are about the class (R12)
}

// ---- behaviour (block 3): a sequence is the behaviour of its subject ------------

void LegacyBinder::bindSteps(const BindContext& ctx, TSNode node, const model::Entity& sequence, const std::string& holder, int& out_order,
                             int& out_frames) const {
  // The steps are a tree (DS-172): each message and frame is contained by what encloses it, the sequence
  // or a frame, in the order written; an alt holds its sides as branches. `holder` is that encloser.
  const std::string sequenceId = sequence.id;
  const uint32_t n = ts_node_child_count(node);
  std::string current = holder;  // an alt's steps go to its first branch, then to its else branch
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_child(node, i);
    if (!ts_node_is_named(c)) {
      if (std::strcmp(ts_node_type(c), "else") == 0 && TreeReader::is(node, "alt")) {
        // the else opens the alt's second side; `holder` is its first, alt::b0
        const std::string alt = holder.substr(0, holder.size() - model::Identity("").branch(false).text().size());
        current = branch(ctx, c, sequenceId, alt, 1, ctx.reader.fieldText(node, Field::else_scenario));
      }
      continue;
    }
    const model::Location loc = ctx.locationOf(c);
    if (TreeReader::is(c, "message") || TreeReader::is(c, "return") || TreeReader::is(c, "fail")) {
      model::Entity m;
      m.kind = model::Kind::Message;
      m.id = sequenceId + "::m" + std::to_string(out_order);
      m.scope = sequenceId;
      m.location = loc;
      const std::string from = ctx.reader.fieldText(c, Field::from);
      const std::string to = ctx.reader.fieldText(c, Field::to);
      m.fields["from"] = from.empty() ? "self" : from;
      m.fields["to"] = to.empty() ? "self" : to;
      m.fields["op"] = ctx.reader.fieldText(c, Field::op);
      if (TreeReader::is(c, "return")) {
        m.fields["result"] = ctx.reader.fieldText(c, Field::value);
        m.fields["return"] = "true";
      } else if (TreeReader::is(c, "fail")) {  // the path ends, failing: an ending like a return, with the failure's name
        m.fields["fails"] = ctx.reader.fieldText(c, Field::failure);
        m.fields["return"] = "true";
      } else {
        const std::string result = ctx.reader.fieldText(c, Field::result);
        if (!result.empty()) m.fields["result"] = result;
        const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
        if (!scenario.empty()) m.fields["scenario"] = scenario;
      }
      int argc = 0;
      std::vector<std::string> args;
      const uint32_t k = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < k; ++j) {
        TSNode a = ts_node_named_child(c, j);
        if (!TreeReader::is(a, "arguments")) continue;
        const uint32_t na = ts_node_named_child_count(a);
        for (uint32_t x = 0; x < na; ++x) {
          args.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(ts_node_named_child(a, x)))));
          ++argc;
        }
      }
      m.fields["args"] = join(args, ", ");
      m.args = args;
      m.fields["argc"] = std::to_string(argc);
      m.name = TreeReader::is(c, "return") ? "return" : TreeReader::is(c, "fail") ? "fail" : m.fields["to"] + "." + m.fields["op"];
      ctx.declare(m);
      ctx.relate({"contains", current.empty() ? sequenceId : current, m.id, loc, ""});
      ctx.noteMessage(
          {sequenceId, m.id, m.fields["to"], m.fields["op"], argc, loc, TreeReader::is(c, "return") || TreeReader::is(c, "fail")});
      ++out_order;
    } else if (TreeReader::is(c, "loop") || TreeReader::is(c, "alt") || TreeReader::is(c, "opt") || TreeReader::is(c, "attempt") ||
               TreeReader::is(c, "handler")) {
      model::Entity f;
      f.kind = model::Kind::Frame;
      f.id = sequenceId + "::f" + std::to_string(out_frames++);
      f.scope = sequenceId;
      f.location = loc;
      f.name = ts_node_type(c);
      f.fields["kind"] = ts_node_type(c);
      f.fields["condition"] = std::string(text::trimWhitespace(ctx.reader.fieldText(c, Field::condition)));
      if (TreeReader::is(c, "handler")) f.fields["failure"] = ctx.reader.fieldText(c, Field::failure);  // the failure it handles (DS-187)
      const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
      if (!scenario.empty() && !TreeReader::is(c, "alt")) f.fields["scenario"] = scenario;  // an alt's sides name their own
      ctx.declare(f);
      ctx.relate({"contains", current.empty() ? sequenceId : current, f.id, loc, ""});
      // an alt holds its first side as a branch; the else, when written, opens the second
      const std::string inside = TreeReader::is(c, "alt") ? branch(ctx, c, sequenceId, f.id, 0, scenario) : f.id;
      bindSteps(ctx, c, *ctx.find(sequenceId), inside, out_order, out_frames);
    }
  }
}

std::string LegacyBinder::branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                                 const std::string& scenario) const {
  model::Entity b;
  b.kind = model::Kind::Frame;
  b.id = model::Identity(alt).branch(side == 1).text();
  b.scope = sequence;
  b.location = ctx.locationOf(at);  // the first side where the alt is written, the else where else is
  b.name = side == 0 ? "branch" : "else";
  b.fields["kind"] = "branch";
  if (!scenario.empty()) b.fields["scenario"] = scenario;
  ctx.declare(b);
  ctx.relate({"contains", alt, b.id, b.location, ""});
  return b.id;
}

void LegacyBinder::bindSequence(const BindContext& ctx, TSNode c) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(c);
  const std::string subject = ctx.reader.fieldText(c, Field::subject);
  const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
  model::Entity e;
  e.kind = model::Kind::Sequence;
  e.name = subject + (scenario.empty() ? "" : "/" + scenario);
  e.id = model::Identity(model::Scope(scope).qualify(subject).text()).sequence(scenario).text();  // identity = the subject (B1)
  e.scope = scope;
  e.location = loc;
  e.fields["subject"] = subject;
  if (!scenario.empty()) e.fields["scenario"] = scenario;
  fields_.bind(c, ctx, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  ctx.refer({declared.id, subject, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  annotations_.bind(c, ctx, declared.id, scope);
  // locals (and the older `participant`) are the only participants written (B3), at any depth of frames
  std::vector<TSNode> pending{c};
  while (!pending.empty()) {
    TSNode node = pending.back();
    pending.pop_back();
    const uint32_t n = ts_node_named_child_count(node);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode s = ts_node_named_child(node, j);
      if (TreeReader::is(s, "loop") || TreeReader::is(s, "alt") || TreeReader::is(s, "opt") || TreeReader::is(s, "attempt") ||
          TreeReader::is(s, "handler"))
        pending.push_back(s);
      if (!TreeReader::is(s, "local") && !TreeReader::is(s, "participant")) continue;
      const std::string name = ctx.reader.fieldText(s, Field::name);
      ctx.annotate(declared.id, "local." + name,
                   types_.bind(TreeReader::child(s, Field::type), ctx, scope, declared.id, "uses", "", {}, ctx.locationOf(s)));
    }
  }
  int order = 0, frames = 0;
  bindSteps(ctx, c, *ctx.find(declared.id), "", order, frames);
}

// ---- the walk --------------------------------------------------------------------

std::vector<std::string> LegacyBinder::kinds() const { return {"class", "function", "sequence"}; }

void LegacyBinder::bind(TSNode c, const BindContext& ctx) const {
  if (TreeReader::is(c, "class")) {
    bindClass(ctx, c);
  } else if (TreeReader::is(c, "function")) {
    bindFunction(ctx, c);
  } else if (TreeReader::is(c, "sequence")) {
    bindSequence(ctx, c);
  }
}

}  // namespace bind
}  // namespace ds
