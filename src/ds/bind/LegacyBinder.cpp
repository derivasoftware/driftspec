#include "ds/bind/LegacyBinder.hpp"

#include <cctype>
#include <cstring>
#include <map>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::TreeReader;
using text::join;
using text::splitTrimmed;

namespace {

// the names of a template parameter list, <F, G: Bound>
std::vector<std::string> templateParams(const parse::TreeReader& reader, TSNode params) {
  std::vector<std::string> out;
  if (ts_node_is_null(params)) return out;
  for (uint32_t i = 0; i < ts_node_named_child_count(params); ++i) out.push_back(reader.fieldText(ts_node_named_child(params, i), "name"));
  return out;
}

bool isOwnership(const std::string& relation) { return relation == "owns" || relation == "refs" || relation == "shares"; }

bool contains(const std::vector<std::string>& items, const std::string& item) {
  for (const auto& i : items) {
    if (i == item) return true;
  }
  return false;
}

}  // namespace

namespace {

model::Term termOf(const parse::TreeReader& reader, TSNode node) {
  const std::string text = reader.textOf(node);
  if (TreeReader::is(node, "field_term")) return {reader.fieldText(node, "of"), true, false, reader.fieldText(node, "field")};
  if (!TreeReader::is(node, "qualified_name")) return {text, false, true};  // a number or a string: a value
  // a bare name is a variable; an entity is always written qualified (the form decides, nothing is guessed)
  return {text, !model::Identity(text).isQualified(), false};
}

/// The text of a clause after its keyword, for the card and the export.
std::string clauseText(const parse::TreeReader& reader, TSNode clause) {
  const std::string text = reader.textOf(clause);
  const auto space = text.find_first_of(" \t\r\n");
  return space == std::string::npos ? "" : std::string(text::trimWhitespace(text.substr(space)));
}

}  // namespace

void LegacyBinder::bindFormula(const BindContext& ctx, TSNode node, model::Entity& out_entity) const {
  model::Formula formula;
  const auto literals = [&](TSNode clause) {
    std::vector<model::Literal> out;
    const uint32_t n = ts_node_named_child_count(clause);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode lit = ts_node_named_child(clause, i);
      if (!TreeReader::is(lit, "literal")) continue;
      model::Literal l;
      l.negated = !ts_node_is_null(TreeReader::child(lit, "negated"));
      l.location = ctx.locationOf(lit);
      TSNode atom = ts_node_named_child(lit, 0);
      if (TreeReader::is(atom, "relation_atom")) {
        l.form = model::LiteralForm::Relation;
        l.name = ctx.reader.fieldText(atom, "relation");
        l.terms = {termOf(ctx.reader, TreeReader::child(atom, "left")), termOf(ctx.reader, TreeReader::child(atom, "right"))};
      } else if (TreeReader::is(atom, "kind_atom")) {
        l.form = model::LiteralForm::Kind;
        l.name = ctx.reader.fieldText(atom, "kind");
        l.terms = {termOf(ctx.reader, TreeReader::child(atom, "left"))};
      } else if (TreeReader::is(atom, "in_atom")) {
        l.form = model::LiteralForm::In;
        l.terms = {termOf(ctx.reader, TreeReader::child(atom, "left")), termOf(ctx.reader, TreeReader::child(atom, "right"))};
      } else if (TreeReader::is(atom, "compare_atom")) {
        static const std::map<std::string, model::LiteralForm> kOperators = {
            {"=", model::LiteralForm::Equal},      {"!=", model::LiteralForm::NotEqual}, {"<", model::LiteralForm::Less},
            {"<=", model::LiteralForm::LessEqual}, {">", model::LiteralForm::Greater},   {">=", model::LiteralForm::GreaterEqual}};
        l.form = kOperators.at(ctx.reader.fieldText(atom, "operator"));
        l.terms = {termOf(ctx.reader, TreeReader::child(atom, "left")), termOf(ctx.reader, TreeReader::child(atom, "right"))};
      } else if (TreeReader::is(atom, "arg_atom")) {
        l.form = model::LiteralForm::Arg;
        l.name = ctx.reader.fieldText(atom, "parameter");
        l.terms = {termOf(ctx.reader, TreeReader::child(atom, "call")), termOf(ctx.reader, TreeReader::child(atom, "value"))};
      } else if (TreeReader::is(atom, "call_atom")) {
        l.form = model::LiteralForm::Call;
        l.name = ctx.reader.fieldText(atom, "definition");
        const uint32_t count = ts_node_child_count(atom);
        for (uint32_t j = 0; j < count; ++j) {
          TSNode arg = ts_node_child(atom, j);
          if (TreeReader::is(arg, "qualified_name") && !ts_node_field_name_for_child(atom, j)) l.terms.push_back(termOf(ctx.reader, arg));
        }
      } else {
        continue;
      }
      out.push_back(l);
    }
    return out;
  };
  const auto append = [&](const char* key, const std::string& text) {
    std::string& field = out_entity.fields[key];
    field += (field.empty() ? "" : "; ") + text;
  };
  const uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_child(node, i);
    if (TreeReader::is(c, "rule_for")) {
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode r = ts_node_named_child(c, j);
        if (TreeReader::is(r, "range"))
          formula.ranges.push_back(
              {ctx.reader.fieldText(r, "kind"), ctx.reader.fieldText(r, "variable"), ctx.reader.fieldText(r, "scope"), ctx.locationOf(r)});
      }
      append("for", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "rule_where")) {
      for (auto& l : literals(c)) formula.where.push_back(l);
      append("where", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "rule_holds") || TreeReader::is(c, "rule_never")) {
      formula.claims.push_back(literals(c));
      formula.never = TreeReader::is(c, "rule_never");
      append(formula.never ? "never" : "holds", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "column")) {
      TSNode what = ts_node_named_child(c, ts_node_named_child_count(c) - 1);
      model::Column column;
      column.name = ctx.reader.fieldText(c, "name");
      column.location = ctx.locationOf(c);
      if (TreeReader::is(what, "column_field")) {
        column.form = "field";
        column.variable = ctx.reader.fieldText(what, "of");
        column.field = ctx.reader.fieldText(what, "field");
      } else if (TreeReader::is(what, "column_related")) {
        column.form = "related";
        column.variable = ctx.reader.fieldText(what, "show");
        column.body = literals(what);
      } else {
        column.form = "holds";
        column.body = literals(what);
      }
      formula.columns.push_back(column);
    } else if (TreeReader::is(c, "definition_case")) {
      formula.cases.push_back(literals(c));
      append("if", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "name") && !ts_node_field_name_for_child(node, i)) {
      formula.parameters.push_back(ctx.reader.textOf(c));  // a definition's parameters
    }
  }
  if (out_entity.kind == model::Kind::Diagram && formula.ranges.empty() && formula.columns.empty()) return;  // no table written
  ctx.state(out_entity.id, formula);
}

// ---- process artefacts: requirement, verification, constraint (R5, R18) ------

// A use case is an artefact identified by its id (R5) and a contract: its signature, the base it
// specialises, its outcomes as entities of their own (DS-77).
void LegacyBinder::bindUseCase(const BindContext& ctx, TSNode node) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  model::Entity e;
  e.kind = model::Kind::UseCase;
  e.name = ctx.reader.fieldText(node, "id");
  e.id = e.name;
  e.scope = scope;
  e.location = ctx.locationOf(node);
  const std::string operation = ctx.reader.fieldText(node, "name");
  if (!operation.empty()) {
    e.fields["operation"] = operation;
    bindSignature(ctx, node, e, scope, {});
  }
  ctx.fields(node, e);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"about", e.id, owner, e.location, ""});
  const std::string base = ctx.reader.fieldText(node, "base");
  // the use case it specialises is resolved like any artefact link: pinned, followed to its revision (DS-116)
  if (!base.empty())
    ctx.refer({e.id, std::string(text::trimWhitespace(base)), scope, "inherits", model::Kind::UseCase, e.location, "", {}});
  ctx.annotations(node, e.id, scope);
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode o = ts_node_named_child(node, i);
    if (!TreeReader::is(o, "outcome")) continue;
    model::Entity out;
    out.kind = model::Kind::Outcome;
    out.name = ctx.reader.fieldText(o, "name");
    out.id = e.id + "::" + out.name;
    out.scope = e.id;
    out.location = ctx.locationOf(o);
    out.fields["text"] = std::string(text::trimWhitespace(ctx.reader.fieldText(o, "text")));
    const std::string returns = bindType(ctx, TreeReader::child(o, "returns"), scope, out.id, "uses", "", {}, out.location);
    if (!returns.empty()) out.fields["returns"] = returns;
    ctx.declare(out);
    ctx.relate({"contains", e.id, out.id, out.location, ""});
  }
}

// ---- types (R15, R16) ----------------------------------------------------------

// A type is the code's text (DS-18). Its names are read out of it; each that the design declares is related
// with the holder's relation (owns by default, refs or shares as written, uses for a signature or a local).
std::string LegacyBinder::bindType(const BindContext& ctx, TSNode type, const std::string& scope, const std::string& from,
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
  ctx.fields(member, out_entity);
  TSNode body = TreeReader::child(member, "body");
  if (!ts_node_is_null(body)) ctx.fields(body, out_entity);
  const model::Entity* declared = ctx.find(out_entity.id);
  // only the declaration the model kept takes the fields: a duplicate is reported and leaves the first as it was
  const model::Location& at = declared ? declared->location : out_entity.location;
  if (declared && at.file == out_entity.location.file && at.line == out_entity.location.line && at.column == out_entity.location.column)
    for (const auto& f : out_entity.fields) ctx.annotate(out_entity.id, f.first, f.second);
  if (ts_node_is_null(body)) return;
  ctx.annotations(body, out_entity.id, scope);
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
    const std::string symbol = ctx.reader.fieldText(m, "symbol");
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
    member.name = ctx.reader.fieldText(m, "name");
    member.id = cls.id + "::" + member.name;
    member.fields["visibility"] = ctx.reader.fieldText(m, "visibility");
  }

  if (TreeReader::is(m, "method") || TreeReader::is(m, "operator")) {
    for (TSNode mod : TreeReader::childrenNamed(m, "modifier")) member.flags.push_back(ctx.reader.textOf(mod));
    if (TreeReader::is(m, "method") && member.name == cls.name) member.flags.push_back("constructor");
    if (TreeReader::is(m, "method") && member.name == "~" + cls.name) member.flags.push_back("destructor");  // DS-143
    if (cls.has("dataclass") && member.has("abstract")) {
      ctx.report(
          {"E-DATACLASS-ABSTRACT", "error", cls.id + " is a dataclass: a value type has no abstract methods (" + member.name + ")", loc});
    }
    std::vector<std::string> own = params;  // a templated method's own parameters are no entities either (DS-18)
    for (const std::string& t : templateParams(ctx.reader, TreeReader::child(m, "params"))) own.push_back(t);
    if (own.size() > params.size())
      member.fields["template"] = join(std::vector<std::string>(own.begin() + params.size(), own.end()), ", ");
    bindSignature(ctx, m, member, scope, own);
  } else {
    const std::string ownership = ctx.reader.fieldText(m, "ownership");
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
    member.fields["type"] = bindType(ctx, TreeReader::child(m, "type"), scope, member.id, relation,
                                     counted == "many"       ? "*"
                                     : counted == "optional" ? "0..1"
                                                             : "",
                                     params, loc);
    const std::string def = ctx.reader.fieldText(m, "default");
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

void LegacyBinder::bindSignature(const BindContext& ctx, TSNode node, model::Entity& out_entity, const std::string& scope,
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
      TSNode type = TreeReader::child(p, "type");
      out_entity.params.push_back(
          {ctx.reader.fieldText(p, "name"), ts_node_is_null(type) ? "" : std::string(text::trimWhitespace(ctx.reader.textOf(type)))});
      bindType(ctx, TreeReader::child(p, "type"), scope, out_entity.id, "uses", "", params, ctx.locationOf(p));
    }
  }
  out_entity.fields["params"] = join(paramTexts, ", ");  // parameters and returns give use (R16)
  const std::string returns =
      bindType(ctx, TreeReader::child(node, "returns"), scope, out_entity.id, "uses", "", params, ctx.locationOf(node));
  if (!returns.empty()) out_entity.fields["returns"] = returns;
  TSNode fails = TreeReader::child(node, "fails");  // what it promises it may fail with; `fails nothing` promises none (DS-189)
  if (!ts_node_is_null(fails)) {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < ts_node_named_child_count(fails); ++i)
      names.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(ts_node_named_child(fails, i)))));
    out_entity.fields["fails"] = join(names, ", ");
  }
}

void LegacyBinder::bindFunction(const BindContext& ctx, TSNode f) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  model::Entity e;
  e.kind = model::Kind::Function;
  e.name = ctx.reader.fieldText(f, "name");
  e.id = model::Scope(scope).qualify(e.name).text();  // ds@ds::load: a function is named like a class of its module
  e.scope = scope;
  e.location = ctx.locationOf(f);
  const std::string visibility = ctx.reader.fieldText(f, "visibility");
  e.fields["visibility"] = visibility.empty() ? "+" : visibility;
  const std::vector<std::string> own = templateParams(ctx.reader, TreeReader::child(f, "params"));  // a templated function (DS-18)
  if (!own.empty()) e.fields["template"] = join(own, ", ");
  bindSignature(ctx, f, e, scope, own);
  ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, e.id, e.location, ""});
  bindMemberBody(ctx, f, e, scope);  // annotations, rules and nested artefacts, as a method's
}

void LegacyBinder::bindClass(const BindContext& ctx, TSNode c) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(c);
  if (ctx.reader.fieldText(c, "visibility").empty() && ts_node_is_null(TreeReader::child(c, "params")) &&
      TreeReader::childrenNamed(c, "base").empty() && ts_node_is_null(TreeReader::child(c, "operators")) &&
      BindContext::annotationsOnly(TreeReader::child(c, "body"))) {
    const std::string name = ctx.reader.fieldText(c, "name");
    ctx.extend(TreeReader::child(c, "body"), model::Scope(scope).qualify(name).text(), model::Kind::Class, name, scope,
               loc);  // the annotations sit in the body
    return;
  }
  model::Entity e;
  e.kind = model::Kind::Class;
  e.name = ctx.reader.fieldText(c, "name");
  e.scope = scope;
  e.location = loc;
  if (ctx.reader.fieldText(c, "kind") == "dataclass") e.flags.push_back("dataclass");
  if (ctx.reader.fieldText(c, "visibility") == "-") e.fields["visibility"] = "-";

  std::vector<std::string> params;
  std::map<std::string, std::string> bounds;
  TSNode paramsNode = TreeReader::child(c, "params");
  if (!ts_node_is_null(paramsNode)) {
    const uint32_t n = ts_node_named_child_count(paramsNode);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(paramsNode, i);
      const std::string name = ctx.reader.fieldText(p, "name");
      params.push_back(name);
      const std::string bound = ctx.reader.fieldText(p, "bound");
      e.params.push_back({name, bound});
      if (!bound.empty()) bounds[name] = bound;
    }
  }
  e.id = model::Scope(scope).qualify(params.empty() ? e.name : model::Identity(e.name).withArity(params.size()).text()).text();  // R15
  if (!params.empty()) e.fields["params"] = join(params, ", ");
  for (const auto& b : bounds) e.fields["bound." + b.first] = b.second;

  std::set<std::string> operators;
  std::vector<std::string> operatorTexts;
  TSNode opSet = TreeReader::child(c, "operators");
  if (!ts_node_is_null(opSet)) {
    const uint32_t n = ts_node_named_child_count(opSet);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode item = ts_node_named_child(opSet, i);
      const std::string text = std::string(text::trimWhitespace(ctx.reader.textOf(item)));
      operatorTexts.push_back(text);
      if (ts_node_is_null(TreeReader::child(item, "family"))) operators.insert(text);
    }
    e.fields["operators"] = join(operatorTexts, ", ");
  }

  TSNode body = TreeReader::child(c, "body");
  ctx.fields(body, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  // a base is written as the code writes it (IFormat<scene::SceneDocument>) and kept so; its first name is what the
  // class inherits, and only when the design declares it: class Error : Exception is text, no arrow (DS-18)
  std::vector<std::string> baseTexts;
  for (TSNode base : TreeReader::childrenNamed(c, "base")) baseTexts.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(base))));
  if (!baseTexts.empty()) ctx.annotate(declared.id, "bases", join(baseTexts, "; "));
  for (const std::string& written : baseTexts) {
    const std::vector<TypeToken> tokens = scanTypeTokens(written);
    if (tokens.empty()) continue;
    const TypeToken& token = tokens.front();
    std::vector<std::string> args = token.args;
    for (auto& a : args)
      if (contains(params, a)) a.clear();  // class Holder<T> : Base<T>: T is Holder's parameter, not a class T
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
  ctx.annotations(body, declared.id, scope);
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
        current = branch(ctx, c, sequenceId, alt, 1, ctx.reader.fieldText(node, "else_scenario"));
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
      const std::string from = ctx.reader.fieldText(c, "from");
      const std::string to = ctx.reader.fieldText(c, "to");
      m.fields["from"] = from.empty() ? "self" : from;
      m.fields["to"] = to.empty() ? "self" : to;
      m.fields["op"] = ctx.reader.fieldText(c, "op");
      if (TreeReader::is(c, "return")) {
        m.fields["result"] = ctx.reader.fieldText(c, "value");
        m.fields["return"] = "true";
      } else if (TreeReader::is(c, "fail")) {  // the path ends, failing: an ending like a return, with the failure's name
        m.fields["fails"] = ctx.reader.fieldText(c, "failure");
        m.fields["return"] = "true";
      } else {
        const std::string result = ctx.reader.fieldText(c, "result");
        if (!result.empty()) m.fields["result"] = result;
        const std::string scenario = ctx.reader.fieldText(c, "scenario");
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
      f.fields["condition"] = std::string(text::trimWhitespace(ctx.reader.fieldText(c, "condition")));
      if (TreeReader::is(c, "handler")) f.fields["failure"] = ctx.reader.fieldText(c, "failure");  // the failure it handles (DS-187)
      const std::string scenario = ctx.reader.fieldText(c, "scenario");
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
  const std::string subject = ctx.reader.fieldText(c, "subject");
  const std::string scenario = ctx.reader.fieldText(c, "scenario");
  model::Entity e;
  e.kind = model::Kind::Sequence;
  e.name = subject + (scenario.empty() ? "" : "/" + scenario);
  e.id = model::Identity(model::Scope(scope).qualify(subject).text()).sequence(scenario).text();  // identity = the subject (B1)
  e.scope = scope;
  e.location = loc;
  e.fields["subject"] = subject;
  if (!scenario.empty()) e.fields["scenario"] = scenario;
  ctx.fields(c, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  ctx.refer({declared.id, subject, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  ctx.annotations(c, declared.id, scope);
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
      const std::string name = ctx.reader.fieldText(s, "name");
      ctx.annotate(declared.id, "local." + name,
                   bindType(ctx, TreeReader::child(s, "type"), scope, declared.id, "uses", "", {}, ctx.locationOf(s)));
    }
  }
  int order = 0, frames = 0;
  bindSteps(ctx, c, *ctx.find(declared.id), "", order, frames);
}

// ---- the walk --------------------------------------------------------------------

std::vector<std::string> LegacyBinder::kinds() const {
  return {"package", "module", "usecase", "rule", "definition", "interface", "class", "function", "enum", "sequence", "state", "diagram"};
}

void LegacyBinder::bind(TSNode c, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(c);

  if (TreeReader::is(c, "package")) {
    // the flat form `package app@render {` reopens render inside app: the prefixes are the scope
    std::string prefix;
    const uint32_t parts = ts_node_named_child_count(c);
    for (uint32_t j = 0; j < parts; ++j) {
      TSNode part = ts_node_named_child(c, j);
      if (TreeReader::is(part, "package_prefix")) prefix += ctx.reader.textOf(part);
    }
    model::Entity e;
    e.kind = model::Kind::Package;
    e.name = ctx.reader.fieldText(c, "name");
    e.scope = prefix.empty() ? scope : prefix;
    e.id = e.scope + e.name;  // every package level adds its name with @: app, app@core, app@core@model::Entity (R4)
    if (ctx.reader.fieldText(c, "visibility") == "-") e.fields["visibility"] = "-";
    e.location = loc;
    ctx.fields(c, e);
    const bool reopened = ctx.find(e.id) != nullptr;  // a box reopened adds no relation (DS-48)
    const model::Entity& declared = ctx.declare(e);
    for (const auto& f : e.fields) ctx.annotate(declared.id, f.first, f.second);
    ctx.annotations(c, declared.id, declared.id + "@");  // an HLD box realises requirements like a class does
    if (!reopened && !owner.empty())
      ctx.relate({"contains", owner, declared.id, loc, ""});
    else if (!reopened && !prefix.empty())  // the outer package must be declared somewhere; the checker turns part-of into contains
      ctx.refer({declared.id, prefix.substr(0, prefix.size() - 1), "", "part-of", model::Kind::Package, loc, "", {}});
    ctx.within(declared.id + "@", declared.id).children(c);

  } else if (TreeReader::is(c, "module")) {
    model::Entity e;
    e.kind = model::Kind::Module;
    e.name = ctx.reader.fieldText(c, "name");
    e.id = model::Scope(scope).qualify(e.name).text();
    e.scope = scope;
    e.location = loc;
    const bool isPrivate = ctx.reader.fieldText(c, "visibility") == "-";
    if (isPrivate) e.fields["visibility"] = "-";
    ctx.fields(c, e);
    const bool reopened = ctx.find(e.id) != nullptr;  // a module reopened adds no relation (DS-48)
    const model::Entity& declared = ctx.declare(e);
    for (const auto& f : e.fields) {  // a module reopens: its fields add up, and two that differ are both kept
      const std::string& kept = declared.field(f.first);
      if (kept.empty())
        ctx.annotate(declared.id, f.first, f.second);
      else if (kept != f.second && (", " + kept + ",").find(", " + f.second + ",") == std::string::npos)
        ctx.annotate(declared.id, f.first, kept + ", " + f.second);
    }
    if (isPrivate) ctx.annotate(declared.id, "visibility", "-");  // a module reopens: any declaration may say it
    if (!owner.empty() && !reopened) ctx.relate({"contains", owner, declared.id, loc, ""});
    ctx.annotations(c, declared.id, declared.id);  // a module answers to its requirements, as a package does
    ctx.within(declared.id, declared.id).children(c);

  } else if (TreeReader::is(c, "usecase")) {
    bindUseCase(ctx, c);

  } else if (TreeReader::is(c, "rule") ||
             TreeReader::is(c, "definition")) {  // a design piece with a name, in the box it applies to (DS-94)
    model::Entity e;
    e.kind = TreeReader::is(c, "rule") ? model::Kind::Rule : model::Kind::Definition;
    e.name = ctx.reader.fieldText(c, "name");
    const model::Entity* in = owner.empty() ? nullptr : ctx.find(owner);
    // a rule of a class or a member is named under it: p@m::Order::priced, p@m::Link::send::in-range
    const bool inClass = in && (in->kind == model::Kind::Class || in->kind == model::Kind::Method || in->kind == model::Kind::Function ||
                                in->kind == model::Kind::Operator || in->kind == model::Kind::Attribute);  // DS-224
    e.id = inClass ? owner + "::" + e.name : model::Scope(scope).qualify(e.name).text();
    e.scope = inClass ? owner : scope;
    e.location = loc;
    ctx.fields(c, e);
    bindFormula(ctx, c, e);
    ctx.declare(e);
    if (!owner.empty()) ctx.relate({"contains", owner, e.id, loc, ""});
    ctx.annotations(c, e.id, scope);

  } else if (TreeReader::is(c, "interface")) {
    model::Entity e;
    e.kind = model::Kind::Interface;
    e.name = ctx.reader.fieldText(c, "name");
    e.id = model::Scope(scope).qualify(e.name).text();  // hola@shell: an external interface of the box
    e.scope = scope;
    e.location = loc;
    ctx.fields(c, e);
    ctx.declare(e);
    if (!owner.empty()) ctx.relate({"exposes", owner, e.id, loc, ""});
    ctx.annotations(c, e.id, scope);
    const uint32_t n = ts_node_named_child_count(c);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode op = ts_node_named_child(c, j);
      if (!TreeReader::is(op, "operation")) continue;
      model::Entity o;
      o.kind = model::Kind::Operation;
      o.name = ctx.reader.fieldText(op, "name");
      o.id = e.id + "::" + o.name;
      o.scope = e.id;
      o.location = ctx.locationOf(op);
      bindSignature(ctx, op, o, scope, {});
      ctx.fields(op, o);
      ctx.declare(o);
      ctx.relate({"contains", e.id, o.id, o.location, ""});
      const std::string realised = ctx.reader.fieldText(op, "realises");  // operation greet(...) : UC-00001-1
      if (!realised.empty())  // the use case it realises: an artefact link, resolved and pinned (DS-116)
        ctx.refer({o.id, std::string(text::trimWhitespace(realised)), scope, "realises", model::Kind::UseCase, o.location, "", {}});
      ctx.annotations(op, o.id, scope);
    }

  } else if (TreeReader::is(c, "class")) {
    bindClass(ctx, c);

  } else if (TreeReader::is(c, "function")) {
    bindFunction(ctx, c);

  } else if (TreeReader::is(c, "enum")) {
    model::Entity e;
    e.kind = model::Kind::Enum;
    e.name = ctx.reader.fieldText(c, "name");
    if (ctx.reader.fieldText(c, "visibility") == "-") e.fields["visibility"] = "-";
    e.id = model::Scope(scope).qualify(e.name).text();
    e.scope = scope;
    e.location = loc;
    std::vector<std::string> values;
    const uint32_t n = ts_node_named_child_count(c);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode v = ts_node_named_child(c, j);
      if (TreeReader::is(v, "enum_value")) values.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(v))));
    }
    e.fields["values"] = join(values, ", ");
    ctx.declare(e);
    if (!owner.empty()) ctx.relate({"contains", owner, e.id, loc, ""});
    ctx.annotations(c, e.id, scope);  // an enum realises requirements like a class

  } else if (TreeReader::is(c, "sequence")) {
    bindSequence(ctx, c);

  } else if (TreeReader::is(c, "state")) {
    const std::string entityName = ctx.reader.fieldText(c, "entity");
    model::Entity e;
    e.kind = model::Kind::State;
    e.name = entityName;
    e.id = model::Identity(model::Scope(scope).qualify(entityName).text()).state().text();
    e.scope = scope;
    e.location = loc;
    ctx.fields(c, e);
    ctx.declare(e);
    ctx.refer({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
    ctx.annotations(c, e.id, scope);
    std::vector<std::string> transitions;
    std::vector<std::string> rules;
    const uint32_t n = ts_node_named_child_count(c);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode t = ts_node_named_child(c, j);
      if (TreeReader::is(t, "initial")) ctx.annotate(e.id, "initial", ctx.reader.fieldText(t, "name"));
      if (TreeReader::is(t, "state_rule")) {  // a state computed by a rule (DS-249): state|literal,literal|line, `not` as a leading !
        std::vector<std::string> literals;
        const uint32_t nl = ts_node_named_child_count(t);
        for (uint32_t x = 0; x < nl; ++x) {
          TSNode l = ts_node_named_child(t, x);
          if (!TreeReader::is(l, "state_literal")) continue;
          literals.push_back((ctx.reader.fieldText(l, "negated").empty() ? "" : "!") + ctx.reader.fieldText(l, "left") +
                             ctx.reader.fieldText(l, "operator") + ctx.reader.fieldText(l, "right"));
        }
        rules.push_back(ctx.reader.fieldText(t, "name") + "|" + join(literals, ",") + "|" + std::to_string(ctx.locationOf(t).line));
        ctx.annotate(e.id, "rules", join(rules, "\n"));
      }
      if (TreeReader::is(t, "view")) {
        model::Entity v;
        v.kind = model::Kind::View;
        v.name = ctx.reader.fieldText(t, "name");
        v.id = e.id + "::" + v.name;
        v.scope = e.id;
        v.location = ctx.locationOf(t);
        ctx.fields(t, v);
        ctx.declare(v);
        ctx.relate({"contains", e.id, v.id, v.location, ""});
        ctx.annotations(t, v.id, scope);
      }
      if (!TreeReader::is(t, "transition")) continue;
      const std::string guard = std::string(text::trimWhitespace(ctx.reader.fieldText(t, "guard")));
      const std::string does = ctx.reader.fieldText(t, "action");
      transitions.push_back(ctx.reader.fieldText(t, "from") + ">" + ctx.reader.fieldText(t, "to") + ">" + ctx.reader.fieldText(t, "event") +
                            (guard.empty() ? "" : " " + guard) + (does.empty() ? "" : " / " + does));
      ctx.annotate(e.id, "transitions", join(transitions, "\n"));  // from>to>label, one per line
      ctx.noteEvent({e.id, entityName, scope, ctx.reader.fieldText(t, "event"), false, ctx.locationOf(t)});
      const std::string action = ctx.reader.fieldText(t, "action");
      if (!action.empty()) ctx.noteEvent({e.id, entityName, scope, action, true, ctx.locationOf(t)});
    }

  } else if (TreeReader::is(c, "diagram")) {
    model::Entity e;
    e.kind = model::Kind::Diagram;
    e.name = ctx.reader.fieldText(c, "name");
    e.id = model::Identity::ofDiagram(e.name).text();
    e.location = loc;
    ctx.fields(c, e);
    bindFormula(ctx, c, e);  // a table's rows and columns (DS-268)
    ctx.declare(e);
  }
}

}  // namespace bind
}  // namespace ds
