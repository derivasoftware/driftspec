#include "ds/bind/Binder.hpp"

#include <cstring>
#include <map>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Words.hpp"

namespace ds {
namespace bind {

using model::join;
using model::split;

namespace {

bool is(TSNode node, const char* type) { return std::strcmp(ts_node_type(node), type) == 0; }

TSNode child(TSNode node, const char* fieldName) {
  return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName)));
}

std::string fieldText(const parse::ParsedFile& file, TSNode node, const char* fieldName) {
  TSNode c = child(node, fieldName);
  return ts_node_is_null(c) ? std::string() : file.textOf(c);
}

/// Every child carrying `fieldName` (a field may repeat: bases, modifiers).
std::vector<TSNode> childrenNamed(TSNode node, const char* fieldName) {
  std::vector<TSNode> out;
  const uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    const char* f = ts_node_field_name_for_child(node, i);
    if (f && std::strcmp(f, fieldName) == 0) out.push_back(ts_node_child(node, i));
  }
  return out;
}

std::string trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

// what an annotation means comes from the metamodel's table (model::annotations()), declared once
std::string relationForAnnotation(model::Kind from, const model::AnnotationSpec& spec) {
  return from == model::Kind::Verification && !spec.onVerification.empty() ? spec.onVerification : spec.relation;
}

const char* const kPrimitives[] = {"int",  "bool",  "string", "path",   "none",
                                   "void", "float", "double", "size_t", "char"};  // the C++ sizes and chars relate nothing either

bool isPrimitive(const std::string& type) {
  for (const char* p : kPrimitives) {
    if (type == p) return true;
  }
  return false;
}

bool isOwnership(const std::string& relation) { return relation == "owns" || relation == "refs" || relation == "shares"; }

bool contains(const std::vector<std::string>& items, const std::string& item) {
  for (const auto& i : items) {
    if (i == item) return true;
  }
  return false;
}

}  // namespace

void Binder::bind(const parse::ParsedFile& file) {
  TSNode root = ts_tree_root_node(file.tree.get());
  visit(file, root, "", "");
}

model::Location Binder::locationOf(const parse::ParsedFile& file, TSNode node) const {
  const TSPoint start = ts_node_start_point(node);
  const TSPoint end = ts_node_end_point(node);
  return {file.path, static_cast<int>(start.row) + 1, static_cast<int>(start.column) + 1, static_cast<int>(end.row) + 1,
          static_cast<int>(end.column) + 1};
}

void Binder::bindFields(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity) {
  if (ts_node_is_null(node)) return;
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (is(c, "field")) {
      const std::string code = fieldText(file, c, "code");
      const std::string key = fieldText(file, c, "key") + (code.empty() ? "" : " " + code);
      const std::string value = trim(fieldText(file, c, "value"));
      auto existing = out_entity.fields.find(key);
      if (existing == out_entity.fields.end() || existing->second.empty()) {
        out_entity.fields[key] = value;
      } else {
        existing->second += ", " + value;  // a repeated key accumulates (include: a / include: b)
      }
    }
  }
}

namespace {

model::Term termOf(const parse::ParsedFile& file, TSNode node) {
  const std::string text = file.textOf(node);
  if (is(node, "field_term")) return {fieldText(file, node, "of"), true, false, fieldText(file, node, "field")};
  if (!is(node, "qualified_name")) return {text, false, true};  // a number or a string: a value
  // a bare name is a variable; an entity is always written qualified (the form decides, nothing is guessed)
  return {text, !model::Identity(text).isQualified(), false};
}

/// The text of a clause after its keyword, for the card and the export.
std::string clauseText(const parse::ParsedFile& file, TSNode clause) {
  const std::string text = file.textOf(clause);
  const auto space = text.find_first_of(" \t\r\n");
  return space == std::string::npos ? "" : trim(text.substr(space));
}

}  // namespace

void Binder::bindFormula(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity) {
  model::Formula formula;
  const auto literals = [&](TSNode clause) {
    std::vector<model::Literal> out;
    const uint32_t n = ts_node_named_child_count(clause);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode lit = ts_node_named_child(clause, i);
      if (!is(lit, "literal")) continue;
      model::Literal l;
      l.negated = !ts_node_is_null(child(lit, "negated"));
      l.location = locationOf(file, lit);
      TSNode atom = ts_node_named_child(lit, 0);
      if (is(atom, "relation_atom")) {
        l.form = model::LiteralForm::Relation;
        l.name = fieldText(file, atom, "relation");
        l.terms = {termOf(file, child(atom, "left")), termOf(file, child(atom, "right"))};
      } else if (is(atom, "kind_atom")) {
        l.form = model::LiteralForm::Kind;
        l.name = fieldText(file, atom, "kind");
        l.terms = {termOf(file, child(atom, "left"))};
      } else if (is(atom, "in_atom")) {
        l.form = model::LiteralForm::In;
        l.terms = {termOf(file, child(atom, "left")), termOf(file, child(atom, "right"))};
      } else if (is(atom, "compare_atom")) {
        static const std::map<std::string, model::LiteralForm> kOperators = {
            {"=", model::LiteralForm::Equal},      {"!=", model::LiteralForm::NotEqual}, {"<", model::LiteralForm::Less},
            {"<=", model::LiteralForm::LessEqual}, {">", model::LiteralForm::Greater},   {">=", model::LiteralForm::GreaterEqual}};
        l.form = kOperators.at(fieldText(file, atom, "operator"));
        l.terms = {termOf(file, child(atom, "left")), termOf(file, child(atom, "right"))};
      } else if (is(atom, "arg_atom")) {
        l.form = model::LiteralForm::Arg;
        l.name = fieldText(file, atom, "parameter");
        l.terms = {termOf(file, child(atom, "call")), termOf(file, child(atom, "value"))};
      } else if (is(atom, "call_atom")) {
        l.form = model::LiteralForm::Call;
        l.name = fieldText(file, atom, "definition");
        const uint32_t count = ts_node_child_count(atom);
        for (uint32_t j = 0; j < count; ++j) {
          TSNode arg = ts_node_child(atom, j);
          if (is(arg, "qualified_name") && !ts_node_field_name_for_child(atom, j)) l.terms.push_back(termOf(file, arg));
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
    if (is(c, "rule_for")) {
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode r = ts_node_named_child(c, j);
        if (is(r, "range"))
          formula.ranges.push_back(
              {fieldText(file, r, "kind"), fieldText(file, r, "variable"), fieldText(file, r, "scope"), locationOf(file, r)});
      }
      append("for", clauseText(file, c));
    } else if (is(c, "rule_where")) {
      for (auto& l : literals(c)) formula.where.push_back(l);
      append("where", clauseText(file, c));
    } else if (is(c, "rule_holds") || is(c, "rule_never")) {
      formula.claims.push_back(literals(c));
      formula.never = is(c, "rule_never");
      append(formula.never ? "never" : "holds", clauseText(file, c));
    } else if (is(c, "column")) {
      TSNode what = ts_node_named_child(c, ts_node_named_child_count(c) - 1);
      model::Column column;
      column.name = fieldText(file, c, "name");
      column.location = locationOf(file, c);
      if (is(what, "column_field")) {
        column.form = "field";
        column.variable = fieldText(file, what, "of");
        column.field = fieldText(file, what, "field");
      } else if (is(what, "column_related")) {
        column.form = "related";
        column.variable = fieldText(file, what, "show");
        column.body = literals(what);
      } else {
        column.form = "holds";
        column.body = literals(what);
      }
      formula.columns.push_back(column);
    } else if (is(c, "definition_case")) {
      formula.cases.push_back(literals(c));
      append("if", clauseText(file, c));
    } else if (is(c, "name") && !ts_node_field_name_for_child(node, i)) {
      formula.parameters.push_back(file.textOf(c));  // a definition's parameters
    }
  }
  if (out_entity.kind == model::Kind::Diagram && formula.ranges.empty() && formula.columns.empty()) return;  // no table written
  model_.state(out_entity.id, formula);
}

void Binder::bindAnnotations(const parse::ParsedFile& file, TSNode node, const std::string& from, const std::string& scope) {
  if (ts_node_is_null(node)) return;
  const model::Entity* entity = model_.find(from);
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (!is(c, "annotation")) continue;
    const std::string kind = fieldText(file, c, "kind");
    const model::AnnotationSpec& spec = model::annotationOf(kind);
    if (spec.relation == "refers") {  // a word the language gives no meaning to: kept as a link, and said (DS-210)
      std::string known;
      for (const auto& a : model::annotations())
        if (a.name.rfind("@", 0) == 0 && a.name.find('<') == std::string::npos) known += (known.empty() ? "" : ", ") + a.name;
      model_.report({"W-ANNOTATION-UNKNOWN", "warning",
                     "@" + kind + " is no annotation the language knows (" + known + "); it is kept as a link nothing reads",
                     locationOf(file, c)});
    }
    const std::string relation = relationForAnnotation(entity ? entity->kind : model::Kind::Class, spec);
    references_.push_back({from, fieldText(file, c, "target"), scope, relation, spec.target, locationOf(file, c), "", {}});
  }
}

// ---- process artefacts: requirement, verification, constraint (R5, R18) ------

namespace {

/// An extension block: the same id declared again with annotations only (decided 2026-10-26).
/// It adds relations to an entity defined elsewhere; a definition field, member or
/// header item makes the block a definition, and a second definition is a duplicate.
bool annotationsOnly(TSNode body, TSNode idNode = TSNode{}) {
  if (ts_node_is_null(body)) return false;
  bool any = false;
  const uint32_t n = ts_node_named_child_count(body);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_named_child(body, i);
    if (is(c, "comment")) continue;
    if (!ts_node_is_null(idNode) && ts_node_eq(c, idNode)) continue;  // the id names the entity, it defines nothing
    if (!is(c, "annotation")) return false;
    any = true;
  }
  return any;
}

}  // namespace

void Binder::extend(const parse::ParsedFile& file, TSNode node, const std::string& id, model::Kind kind, const std::string& name,
                    const std::string& scope, const model::Location& loc) {
  if (!model_.find(id)) {
    // nobody has defined it yet (or ever): an expected node that a definition replaces (R6)
    model::Entity expected;
    expected.kind = kind;
    expected.id = id;
    expected.name = name;
    expected.scope = scope;
    expected.location = loc;
    expected.expected = true;
    expected.flags.push_back("extension");
    model_.declare(expected);
  }
  bindAnnotations(file, node, id, scope);
}

void Binder::bindArtefact(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner) {
  const model::Location loc = locationOf(file, node);
  if (annotationsOnly(node, child(node, "id"))) {
    const model::Kind kind = is(node, "requirement")    ? model::Kind::Requirement
                             : is(node, "verification") ? model::Kind::Verification
                                                        : model::Kind::Constraint;
    extend(file, node, fieldText(file, node, "id"), kind, fieldText(file, node, "id"), scope, loc);
    return;
  }
  model::Entity e;
  e.kind = is(node, "requirement")    ? model::Kind::Requirement
           : is(node, "verification") ? model::Kind::Verification
                                      : model::Kind::Constraint;
  e.name = fieldText(file, node, "id");
  e.id = e.name;  // process artefacts are identified by id (R5)
  e.scope = scope;
  e.location = loc;
  bindFields(file, node, e);
  const std::string title = fieldText(file, node, "title");
  if (!title.empty()) e.fields["title"] = trim(title);  // the one-line form
  model_.declare(e);
  if (!owner.empty()) {
    // nesting gives the relation (R3): a verification verifies the requirement it is in,
    // a requirement refines the requirement it is in, anything else is about its owner
    const model::Entity* ownerEntity = model_.find(owner);
    const bool inRequirement = ownerEntity && ownerEntity->kind == model::Kind::Requirement;
    // nesting an artefact in a class, a member or a function says what annotating it says: the entity answers
    // to the requirement or obeys the constraint (Owner, 2026-11-15; DS-217). In a box it gives its level
    const bool inDesign = ownerEntity && ownerEntity->kind != model::Kind::Package && ownerEntity->kind != model::Kind::Module &&
                          ownerEntity->kind != model::Kind::Requirement;
    if (inDesign && e.kind == model::Kind::Requirement) {
      model_.relate({"satisfies", owner, e.id, loc, ""});
    } else if (inDesign && e.kind == model::Kind::Constraint) {
      model_.relate({"constrained-by", owner, e.id, loc, ""});
    } else {
      std::string relation = "about";
      if (inRequirement && e.kind == model::Kind::Verification) relation = "verifies";
      if (inRequirement && e.kind == model::Kind::Requirement) relation = "refines";
      model_.relate({relation, e.id, owner, loc, ""});
    }
  }
  if (e.fields.count("parent")) {
    // one representation per concept: nesting or @refines, never a field
    model_.report(
        {"W-PARENT-IGNORED", "warning", e.id + " carries a parent: field; nest it in its parent or write @refines <id-revision>", loc});
  }
  bindAnnotations(file, node, e.id, scope);
  visit(file, node, scope, e.id);  // nested verifications and constraints
}

// A use case is an artefact identified by its id (R5) and a contract: its signature, the base it
// specialises, its outcomes as entities of their own (DS-77).
void Binder::bindUseCase(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner) {
  model::Entity e;
  e.kind = model::Kind::UseCase;
  e.name = fieldText(file, node, "id");
  e.id = e.name;
  e.scope = scope;
  e.location = locationOf(file, node);
  const std::string operation = fieldText(file, node, "name");
  if (!operation.empty()) {
    e.fields["operation"] = operation;
    bindSignature(file, node, e, scope, {});
  }
  bindFields(file, node, e);
  model_.declare(e);
  if (!owner.empty()) model_.relate({"about", e.id, owner, e.location, ""});
  const std::string base = fieldText(file, node, "base");
  // the use case it specialises is resolved like any artefact link: pinned, followed to its revision (DS-116)
  if (!base.empty()) references_.push_back({e.id, trim(base), scope, "inherits", model::Kind::UseCase, e.location, "", {}});
  bindAnnotations(file, node, e.id, scope);
  const uint32_t n = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode o = ts_node_named_child(node, i);
    if (!is(o, "outcome")) continue;
    model::Entity out;
    out.kind = model::Kind::Outcome;
    out.name = fieldText(file, o, "name");
    out.id = e.id + "::" + out.name;
    out.scope = e.id;
    out.location = locationOf(file, o);
    out.fields["text"] = trim(fieldText(file, o, "text"));
    const TypeUse returns = bindType(file, child(o, "returns"), scope, out.id, "uses", "", {}, out.location);
    if (!returns.text.empty()) out.fields["returns"] = returns.text;
    model_.declare(out);
    model_.relate({"contains", e.id, out.id, out.location, ""});
  }
}

// ---- types (R15, R16) ----------------------------------------------------------

TypeUse Binder::bindType(const parse::ParsedFile& file, TSNode type, const std::string& scope, const std::string& from,
                         const std::string& relation, const std::string& multiplicity, const std::vector<std::string>& params,
                         const model::Location& location) {
  TypeUse use;
  if (ts_node_is_null(type)) return use;
  use.text = trim(file.textOf(type));
  const std::string name = fieldText(file, type, "name");
  std::vector<TSNode> args;
  TSNode argsNode = child(type, "args");
  if (!ts_node_is_null(argsNode)) {
    const uint32_t n = ts_node_named_child_count(argsNode);
    for (uint32_t i = 0; i < n; ++i) args.push_back(ts_node_named_child(argsNode, i));
  }

  // built-in generics decide the multiplicity and pass the relation to their element (R16);
  // a qualified container (std::vector, std::map) is the same container, read from one table
  const std::string text = use.text;
  if (const model::ContainerSpec* container = model::containerOf(name)) {
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (static_cast<int>(i) == container->element)
        use = bindType(file, args[i], scope, from, relation, container->multiplicity, params, location);
      else
        bindType(file, args[i], scope, from, "uses", "", params, location);  // a map's key is used
    }
    use.text = text;
    return use;
  }
  if (contains(params, name)) {
    use.parameter = true;
    return use;
  }
  if (isPrimitive(name)) return use;  // nothing to relate

  use.classLike = true;
  Reference ref;
  ref.from = from;
  ref.target = args.empty() ? name : model::Identity(name).withArity(args.size()).text();  // identity = name + arity (R15)
  ref.scope = scope;
  ref.relation = relation;
  ref.expectedKind = model::Kind::Class;
  ref.location = location;
  ref.multiplicity = isOwnership(relation) ? (multiplicity.empty() ? "1" : multiplicity) : "";
  for (TSNode a : args) {
    ref.args.push_back(trim(file.textOf(a)));
    bindType(file, a, scope, from, "uses", "", params, location);  // the argument is used by the holder
  }
  references_.push_back(ref);
  return use;
}

// ---- classes (R14, R15, R17, R19) ----------------------------------------------

void Binder::declareOperator(model::Entity& out_cls, const std::string& symbol, const model::Location& location) {
  model::Entity op;
  op.kind = model::Kind::Operator;
  op.name = "operator" + symbol;
  op.id = out_cls.id + "::" + op.name;
  op.scope = out_cls.id;
  op.location = location;
  op.fields["symbol"] = symbol;
  model_.declare(op);
  model_.relate({"contains", out_cls.id, op.id, location, ""});
}

void Binder::bindMemberBody(const parse::ParsedFile& file, TSNode member, model::Entity& out_entity, const std::string& scope) {
  bindFields(file, member, out_entity);
  TSNode body = child(member, "body");
  if (!ts_node_is_null(body)) bindFields(file, body, out_entity);
  model::Entity* declared = model_.find(out_entity.id);
  // only the declaration the model kept takes the fields: a duplicate is reported and leaves the first as it was
  const model::Location& at = declared ? declared->location : out_entity.location;
  if (declared && at.file == out_entity.location.file && at.line == out_entity.location.line && at.column == out_entity.location.column)
    declared->fields = out_entity.fields;
  if (ts_node_is_null(body)) return;
  bindAnnotations(file, body, out_entity.id, scope);
  visit(file, body, scope, out_entity.id);  // nested requirements and constraints are about the member (R12)
}

void Binder::bindMember(const parse::ParsedFile& file, TSNode m, model::Entity& out_cls, const std::vector<std::string>& params,
                        const std::set<std::string>& operators) {
  const model::Location loc = locationOf(file, m);
  const std::string scope = out_cls.scope;
  model::Entity member;
  member.scope = out_cls.id;
  member.location = loc;

  if (is(m, "operator")) {
    const std::string symbol = fieldText(file, m, "symbol");
    member.kind = model::Kind::Operator;
    member.name = "operator" + symbol;
    member.id = out_cls.id + "::" + member.name;
    member.fields["symbol"] = symbol;
    if (operators.count(symbol) == 0) {
      model_.report({"E-OPERATOR-UNDECLARED", "error",
                     "operator " + symbol + " is refined in the body of " + out_cls.id + " but is not in its header set", loc});
    }
  } else {
    member.kind = is(m, "method") ? model::Kind::Method : model::Kind::Attribute;
    member.name = fieldText(file, m, "name");
    member.id = out_cls.id + "::" + member.name;
    member.fields["visibility"] = fieldText(file, m, "visibility");
  }

  if (is(m, "method") || is(m, "operator")) {
    for (TSNode mod : childrenNamed(m, "modifier")) member.flags.push_back(file.textOf(mod));
    if (is(m, "method") && member.name == out_cls.name) member.flags.push_back("constructor");
    if (is(m, "method") && member.name == "~" + out_cls.name) member.flags.push_back("destructor");  // DS-143
    if (out_cls.has("dataclass") && member.has("abstract")) {
      model_.report({"E-DATACLASS-ABSTRACT", "error",
                     out_cls.id + " is a dataclass: a value type has no abstract methods (" + member.name + ")", loc});
    }
    bindSignature(file, m, member, scope, params);
  } else {
    const std::string ownership = fieldText(file, m, "ownership");
    const std::string relation = ownership.empty() ? "owns" : ownership;  // owns by default (R16)
    member.flags.push_back(relation);
    TypeUse use = bindType(file, child(m, "type"), scope, member.id, relation, "", params, loc);
    member.fields["type"] = use.text;
    const std::string def = fieldText(file, m, "default");
    if (!def.empty()) member.fields["default"] = def;
    if (!ownership.empty() && !use.classLike && !use.parameter) {
      model_.report({"W-MODIFIER-ON-VALUE", "warning",
                     "'" + ownership + "' on " + member.id + " means nothing: " + use.text + " is a value, not an entity", loc});
    }
  }

  if (member.kind == model::Kind::Operator && model_.find(member.id)) {
    model::Entity* declared = model_.find(member.id);  // declared by the header set: the body refines it
    for (const auto& f : member.fields) declared->fields[f.first] = f.second;
    for (const auto& f : member.flags) declared->flags.push_back(f);
    if (!member.params.empty()) declared->params = member.params;
    declared->location = loc;
  } else {
    model_.declare(member);
    model_.relate({"contains", out_cls.id, member.id, loc, ""});
  }
  bindMemberBody(file, m, member, scope);
}

void Binder::bindSignature(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity, const std::string& scope,
                           const std::vector<std::string>& params) {
  TSNode parameters{};
  for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
    if (is(ts_node_named_child(node, i), "parameters")) parameters = ts_node_named_child(node, i);
  }
  std::vector<std::string> paramTexts;
  if (!ts_node_is_null(parameters)) {
    const uint32_t n = ts_node_named_child_count(parameters);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(parameters, i);
      paramTexts.push_back(trim(file.textOf(p)));
      TSNode type = child(p, "type");
      out_entity.params.push_back({fieldText(file, p, "name"), ts_node_is_null(type) ? "" : trim(file.textOf(type))});
      bindType(file, child(p, "type"), scope, out_entity.id, "uses", "", params, locationOf(file, p));
    }
  }
  out_entity.fields["params"] = join(paramTexts, ", ");  // parameters and returns give use (R16)
  TypeUse returns = bindType(file, child(node, "returns"), scope, out_entity.id, "uses", "", params, locationOf(file, node));
  if (!returns.text.empty()) out_entity.fields["returns"] = returns.text;
  TSNode fails = child(node, "fails");  // what it promises it may fail with; `fails nothing` promises none (DS-189)
  if (!ts_node_is_null(fails)) {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < ts_node_named_child_count(fails); ++i) names.push_back(trim(file.textOf(ts_node_named_child(fails, i))));
    out_entity.fields["fails"] = join(names, ", ");
  }
}

void Binder::bindFunction(const parse::ParsedFile& file, TSNode f, const std::string& scope, const std::string& owner) {
  model::Entity e;
  e.kind = model::Kind::Function;
  e.name = fieldText(file, f, "name");
  e.id = model::Scope(scope).qualify(e.name).text();  // ds@ds::load: a function is named like a class of its module
  e.scope = scope;
  e.location = locationOf(file, f);
  const std::string visibility = fieldText(file, f, "visibility");
  e.fields["visibility"] = visibility.empty() ? "+" : visibility;
  const model::Entity* ownerEntity = owner.empty() ? nullptr : model_.find(owner);
  if (ownerEntity && ownerEntity->has("extern")) e.flags.push_back("extern");  // R11: a name, no obligations
  bindSignature(file, f, e, scope, {});
  model_.declare(e);
  if (!owner.empty()) model_.relate({"contains", owner, e.id, e.location, ""});
  bindMemberBody(file, f, e, scope);  // annotations, rules and nested artefacts, as a method's
}

void Binder::bindClass(const parse::ParsedFile& file, TSNode c, const std::string& scope, const std::string& owner) {
  const model::Location loc = locationOf(file, c);
  if (fieldText(file, c, "visibility").empty() && ts_node_is_null(child(c, "params")) && childrenNamed(c, "base").empty() &&
      ts_node_is_null(child(c, "operators")) && annotationsOnly(child(c, "body"))) {
    const std::string name = fieldText(file, c, "name");
    extend(file, child(c, "body"), model::Scope(scope).qualify(name).text(), model::Kind::Class, name, scope,
           loc);  // the annotations sit in the body
    return;
  }
  model::Entity e;
  e.kind = model::Kind::Class;
  e.name = fieldText(file, c, "name");
  e.scope = scope;
  e.location = loc;
  if (fieldText(file, c, "kind") == "dataclass") e.flags.push_back("dataclass");
  if (fieldText(file, c, "visibility") == "-") e.fields["visibility"] = "-";
  const model::Entity* ownerEntity = owner.empty() ? nullptr : model_.find(owner);
  if (ownerEntity && ownerEntity->has("extern")) e.flags.push_back("extern");

  std::vector<std::string> params;
  std::map<std::string, std::string> bounds;
  TSNode paramsNode = child(c, "params");
  if (!ts_node_is_null(paramsNode)) {
    const uint32_t n = ts_node_named_child_count(paramsNode);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(paramsNode, i);
      const std::string name = fieldText(file, p, "name");
      params.push_back(name);
      const std::string bound = fieldText(file, p, "bound");
      e.params.push_back({name, bound});
      if (!bound.empty()) bounds[name] = bound;
    }
  }
  e.id = model::Scope(scope).qualify(params.empty() ? e.name : model::Identity(e.name).withArity(params.size()).text()).text();  // R15
  if (!params.empty()) e.fields["params"] = join(params, ", ");
  for (const auto& b : bounds) e.fields["bound." + b.first] = b.second;

  std::set<std::string> operators;
  std::vector<std::string> operatorTexts;
  TSNode opSet = child(c, "operators");
  if (!ts_node_is_null(opSet)) {
    const uint32_t n = ts_node_named_child_count(opSet);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode item = ts_node_named_child(opSet, i);
      const std::string text = trim(file.textOf(item));
      operatorTexts.push_back(text);
      if (ts_node_is_null(child(item, "family"))) operators.insert(text);
    }
    e.fields["operators"] = join(operatorTexts, ", ");
  }

  TSNode body = child(c, "body");
  bindFields(file, body, e);
  model::Entity& declared = model_.declare(e);
  if (!owner.empty()) model_.relate({"contains", owner, declared.id, loc, ""});
  for (TSNode base : childrenNamed(c, "base")) {
    references_.push_back({declared.id, file.textOf(base), scope, "inherits", model::Kind::Class, loc, "", {}});
  }
  for (const auto& b : bounds) {
    references_.push_back({declared.id, b.second, scope, "bounded-by", model::Kind::Class, loc, "", {}});
  }
  for (const auto& symbol : operators) declareOperator(declared, symbol, loc);
  if (ts_node_is_null(body)) return;
  bindAnnotations(file, body, declared.id, scope);
  const uint32_t n = ts_node_named_child_count(body);
  for (uint32_t j = 0; j < n; ++j) {
    TSNode m = ts_node_named_child(body, j);
    if (is(m, "method") || is(m, "attribute") || is(m, "operator")) bindMember(file, m, declared, params, operators);
  }
  visit(file, body, scope, declared.id);  // nested requirements and constraints are about the class (R12)
}

// ---- behaviour (block 3): a sequence is the behaviour of its subject ------------

void Binder::bindSteps(const parse::ParsedFile& file, TSNode node, model::Entity& out_sequence, const std::string& holder, int& out_order,
                       int& out_frames) {
  // The steps are a tree (DS-172): each message and frame is contained by what encloses it, the sequence
  // or a frame, in the order written; an alt holds its sides as branches. `holder` is that encloser.
  const std::string sequenceId = out_sequence.id;
  const uint32_t n = ts_node_child_count(node);
  std::string current = holder;  // an alt's steps go to its first branch, then to its else branch
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_child(node, i);
    if (!ts_node_is_named(c)) {
      if (std::strcmp(ts_node_type(c), "else") == 0 && is(node, "alt")) {
        // the else opens the alt's second side; `holder` is its first, alt::b0
        const std::string alt = holder.substr(0, holder.size() - std::string("::b0").size());
        current = branch(file, c, sequenceId, alt, 1, fieldText(file, node, "else_scenario"));
      }
      continue;
    }
    const model::Location loc = locationOf(file, c);
    if (is(c, "message") || is(c, "return") || is(c, "fail")) {
      model::Entity m;
      m.kind = model::Kind::Message;
      m.id = sequenceId + "::m" + std::to_string(out_order);
      m.scope = sequenceId;
      m.location = loc;
      const std::string from = fieldText(file, c, "from");
      const std::string to = fieldText(file, c, "to");
      m.fields["from"] = from.empty() ? "self" : from;
      m.fields["to"] = to.empty() ? "self" : to;
      m.fields["op"] = fieldText(file, c, "op");
      if (is(c, "return")) {
        m.fields["result"] = fieldText(file, c, "value");
        m.fields["return"] = "true";
      } else if (is(c, "fail")) {  // the path ends, failing: an ending like a return, with the failure's name
        m.fields["fails"] = fieldText(file, c, "failure");
        m.fields["return"] = "true";
      } else {
        const std::string result = fieldText(file, c, "result");
        if (!result.empty()) m.fields["result"] = result;
        const std::string scenario = fieldText(file, c, "scenario");
        if (!scenario.empty()) m.fields["scenario"] = scenario;
      }
      int argc = 0;
      std::vector<std::string> args;
      const uint32_t k = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < k; ++j) {
        TSNode a = ts_node_named_child(c, j);
        if (!is(a, "arguments")) continue;
        const uint32_t na = ts_node_named_child_count(a);
        for (uint32_t x = 0; x < na; ++x) {
          args.push_back(trim(file.textOf(ts_node_named_child(a, x))));
          ++argc;
        }
      }
      m.fields["args"] = join(args, ", ");
      m.args = args;
      m.fields["argc"] = std::to_string(argc);
      m.name = is(c, "return") ? "return" : is(c, "fail") ? "fail" : m.fields["to"] + "." + m.fields["op"];
      model_.declare(m);
      model_.relate({"contains", current.empty() ? sequenceId : current, m.id, loc, ""});
      messages_.push_back({sequenceId, m.id, m.fields["to"], m.fields["op"], argc, loc, is(c, "return") || is(c, "fail")});
      ++out_order;
    } else if (is(c, "loop") || is(c, "alt") || is(c, "opt") || is(c, "attempt") || is(c, "handler")) {
      model::Entity f;
      f.kind = model::Kind::Frame;
      f.id = sequenceId + "::f" + std::to_string(out_frames++);
      f.scope = sequenceId;
      f.location = loc;
      f.name = ts_node_type(c);
      f.fields["kind"] = ts_node_type(c);
      f.fields["condition"] = trim(fieldText(file, c, "condition"));
      if (is(c, "handler")) f.fields["failure"] = fieldText(file, c, "failure");  // the failure it handles (DS-187)
      const std::string scenario = fieldText(file, c, "scenario");
      if (!scenario.empty() && !is(c, "alt")) f.fields["scenario"] = scenario;  // an alt's sides name their own
      model_.declare(f);
      model_.relate({"contains", current.empty() ? sequenceId : current, f.id, loc, ""});
      // an alt holds its first side as a branch; the else, when written, opens the second
      const std::string inside = is(c, "alt") ? branch(file, c, sequenceId, f.id, 0, scenario) : f.id;
      bindSteps(file, c, *model_.find(sequenceId), inside, out_order, out_frames);
    }
  }
}

std::string Binder::branch(const parse::ParsedFile& file, TSNode at, const std::string& sequence, const std::string& alt, int side,
                           const std::string& scenario) {
  model::Entity b;
  b.kind = model::Kind::Frame;
  b.id = alt + "::b" + std::to_string(side);
  b.scope = sequence;
  b.location = locationOf(file, at);  // the first side where the alt is written, the else where else is
  b.name = side == 0 ? "branch" : "else";
  b.fields["kind"] = "branch";
  if (!scenario.empty()) b.fields["scenario"] = scenario;
  model_.declare(b);
  model_.relate({"contains", alt, b.id, b.location, ""});
  return b.id;
}

void Binder::bindSequence(const parse::ParsedFile& file, TSNode c, const std::string& scope, const std::string& owner) {
  const model::Location loc = locationOf(file, c);
  const std::string subject = fieldText(file, c, "subject");
  const std::string scenario = fieldText(file, c, "scenario");
  model::Entity e;
  e.kind = model::Kind::Sequence;
  e.name = subject + (scenario.empty() ? "" : "/" + scenario);
  e.id = model::Identity(model::Scope(scope).qualify(subject).text()).sequence(scenario).text();  // identity = the subject (B1)
  e.scope = scope;
  e.location = loc;
  e.fields["subject"] = subject;
  if (!scenario.empty()) e.fields["scenario"] = scenario;
  bindFields(file, c, e);
  model::Entity& declared = model_.declare(e);
  if (!owner.empty()) model_.relate({"contains", owner, declared.id, loc, ""});
  references_.push_back({declared.id, subject, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  bindAnnotations(file, c, declared.id, scope);
  // locals (and the older `participant`) are the only participants written (B3), at any depth of frames
  std::vector<TSNode> pending{c};
  while (!pending.empty()) {
    TSNode node = pending.back();
    pending.pop_back();
    const uint32_t n = ts_node_named_child_count(node);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode s = ts_node_named_child(node, j);
      if (is(s, "loop") || is(s, "alt") || is(s, "opt") || is(s, "attempt") || is(s, "handler")) pending.push_back(s);
      if (!is(s, "local") && !is(s, "participant")) continue;
      const std::string name = fieldText(file, s, "name");
      TypeUse use = bindType(file, child(s, "type"), scope, declared.id, "uses", "", {}, locationOf(file, s));
      model_.find(declared.id)->fields["local." + name] = use.text;
    }
  }
  int order = 0, frames = 0;
  bindSteps(file, c, *model_.find(declared.id), "", order, frames);
}

// ---- the walk --------------------------------------------------------------------

void Binder::visit(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner) {
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    const model::Location loc = locationOf(file, c);

    if (is(c, "package")) {
      // the flat form `package app@render {` reopens render inside app: the prefixes are the scope
      std::string prefix;
      const uint32_t parts = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < parts; ++j) {
        TSNode part = ts_node_named_child(c, j);
        if (is(part, "package_prefix")) prefix += file.textOf(part);
      }
      model::Entity e;
      e.kind = model::Kind::Package;
      e.name = fieldText(file, c, "name");
      e.scope = prefix.empty() ? scope : prefix;
      e.id = e.scope + e.name;  // every package level adds its name with @: app, app@core, app@core@model::Entity (R4)
      if (fieldText(file, c, "visibility") == "-") e.fields["visibility"] = "-";
      e.location = loc;
      bindFields(file, c, e);
      const bool reopened = model_.find(e.id) != nullptr;  // a box reopened adds no relation (DS-48)
      model::Entity& declared = model_.declare(e);
      for (const auto& f : e.fields) declared.fields[f.first] = f.second;
      bindAnnotations(file, c, declared.id, declared.id + "@");  // an HLD box realises requirements like a class does
      if (!reopened && !owner.empty())
        model_.relate({"contains", owner, declared.id, loc, ""});
      else if (!reopened && !prefix.empty())  // the outer package must be declared somewhere; the checker turns part-of into contains
        references_.push_back({declared.id, prefix.substr(0, prefix.size() - 1), "", "part-of", model::Kind::Package, loc, "", {}});
      visit(file, c, declared.id + "@", declared.id);

    } else if (is(c, "module")) {
      model::Entity e;
      e.kind = model::Kind::Module;
      e.name = fieldText(file, c, "name");
      e.id = model::Scope(scope).qualify(e.name).text();
      e.scope = scope;
      e.location = loc;
      const model::Entity* ownerEntity = owner.empty() ? nullptr : model_.find(owner);
      if (!ts_node_is_null(child(c, "extern")) || (ownerEntity && ownerEntity->has("extern"))) e.flags.push_back("extern");  // R11
      const bool isPrivate = fieldText(file, c, "visibility") == "-";
      if (isPrivate) e.fields["visibility"] = "-";
      bindFields(file, c, e);
      const bool reopened = model_.find(e.id) != nullptr;  // a module reopened adds no relation (DS-48)
      model::Entity& declared = model_.declare(e);
      for (const auto& f : e.fields) {  // a module reopens: its fields add up, and two that differ are both kept
        auto& kept = declared.fields[f.first];
        if (kept.empty())
          kept = f.second;
        else if (kept != f.second && (", " + kept + ",").find(", " + f.second + ",") == std::string::npos)
          kept += ", " + f.second;
      }
      if (isPrivate) declared.fields["visibility"] = "-";  // a module reopens: any declaration may say it
      if (!owner.empty() && !reopened) model_.relate({"contains", owner, declared.id, loc, ""});
      bindAnnotations(file, c, declared.id, declared.id);  // a module answers to its requirements, as a package does
      visit(file, c, declared.id, declared.id);

    } else if (is(c, "requirement") || is(c, "verification") || is(c, "constraint")) {
      bindArtefact(file, c, scope, owner);

    } else if (is(c, "usecase")) {
      bindUseCase(file, c, scope, owner);

    } else if (is(c, "rule") || is(c, "definition")) {  // a design piece with a name, in the box it applies to (DS-94)
      model::Entity e;
      e.kind = is(c, "rule") ? model::Kind::Rule : model::Kind::Definition;
      e.name = fieldText(file, c, "name");
      const model::Entity* in = owner.empty() ? nullptr : model_.find(owner);
      // a rule of a class or a member is named under it: p@m::Order::priced, p@m::Link::send::in-range
      const bool inClass = in && (in->kind == model::Kind::Class || in->kind == model::Kind::Method || in->kind == model::Kind::Function ||
                                  in->kind == model::Kind::Operator || in->kind == model::Kind::Attribute);  // DS-224
      e.id = inClass ? owner + "::" + e.name : model::Scope(scope).qualify(e.name).text();
      e.scope = inClass ? owner : scope;
      e.location = loc;
      bindFields(file, c, e);
      bindFormula(file, c, e);
      model_.declare(e);
      if (!owner.empty()) model_.relate({"contains", owner, e.id, loc, ""});
      bindAnnotations(file, c, e.id, scope);

    } else if (is(c, "interface")) {
      model::Entity e;
      e.kind = model::Kind::Interface;
      e.name = fieldText(file, c, "name");
      e.id = model::Scope(scope).qualify(e.name).text();  // hola@shell: an external interface of the box
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      if (!owner.empty()) model_.relate({"exposes", owner, e.id, loc, ""});
      bindAnnotations(file, c, e.id, scope);
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode op = ts_node_named_child(c, j);
        if (!is(op, "operation")) continue;
        model::Entity o;
        o.kind = model::Kind::Operation;
        o.name = fieldText(file, op, "name");
        o.id = e.id + "::" + o.name;
        o.scope = e.id;
        o.location = locationOf(file, op);
        bindSignature(file, op, o, scope, {});
        bindFields(file, op, o);
        model_.declare(o);
        model_.relate({"contains", e.id, o.id, o.location, ""});
        const std::string realised = fieldText(file, op, "realises");  // operation greet(...) : UC-00001-1
        if (!realised.empty())  // the use case it realises: an artefact link, resolved and pinned (DS-116)
          references_.push_back({o.id, trim(realised), scope, "realises", model::Kind::UseCase, o.location, "", {}});
        bindAnnotations(file, op, o.id, scope);
      }

    } else if (is(c, "class")) {
      bindClass(file, c, scope, owner);

    } else if (is(c, "function")) {
      bindFunction(file, c, scope, owner);

    } else if (is(c, "enum")) {
      model::Entity e;
      e.kind = model::Kind::Enum;
      e.name = fieldText(file, c, "name");
      if (fieldText(file, c, "visibility") == "-") e.fields["visibility"] = "-";
      e.id = model::Scope(scope).qualify(e.name).text();
      e.scope = scope;
      e.location = loc;
      std::vector<std::string> values;
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode v = ts_node_named_child(c, j);
        if (is(v, "enum_value")) values.push_back(trim(file.textOf(v)));
      }
      e.fields["values"] = join(values, ", ");
      model_.declare(e);
      if (!owner.empty()) model_.relate({"contains", owner, e.id, loc, ""});
      bindAnnotations(file, c, e.id, scope);  // an enum realises requirements like a class

    } else if (is(c, "sequence")) {
      bindSequence(file, c, scope, owner);

    } else if (is(c, "state")) {
      const std::string entityName = fieldText(file, c, "entity");
      model::Entity e;
      e.kind = model::Kind::State;
      e.name = entityName;
      e.id = model::Identity(model::Scope(scope).qualify(entityName).text()).state().text();
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      references_.push_back({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
      bindAnnotations(file, c, e.id, scope);
      std::vector<std::string> transitions;
      std::vector<std::string> rules;
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode t = ts_node_named_child(c, j);
        if (is(t, "initial")) model_.find(e.id)->fields["initial"] = fieldText(file, t, "name");
        if (is(t, "state_rule")) {  // a state computed by a rule (DS-249): state|literal,literal|line, `not` as a leading !
          std::vector<std::string> literals;
          const uint32_t nl = ts_node_named_child_count(t);
          for (uint32_t x = 0; x < nl; ++x) {
            TSNode l = ts_node_named_child(t, x);
            if (!is(l, "state_literal")) continue;
            literals.push_back((fieldText(file, l, "negated").empty() ? "" : "!") + fieldText(file, l, "left") +
                               fieldText(file, l, "operator") + fieldText(file, l, "right"));
          }
          rules.push_back(fieldText(file, t, "name") + "|" + join(literals, ",") + "|" + std::to_string(locationOf(file, t).line));
          model_.find(e.id)->fields["rules"] = join(rules, "\n");
        }
        if (is(t, "view")) {
          model::Entity v;
          v.kind = model::Kind::View;
          v.name = fieldText(file, t, "name");
          v.id = e.id + "::" + v.name;
          v.scope = e.id;
          v.location = locationOf(file, t);
          bindFields(file, t, v);
          model_.declare(v);
          model_.relate({"contains", e.id, v.id, v.location, ""});
          bindAnnotations(file, t, v.id, scope);
        }
        if (!is(t, "transition")) continue;
        const std::string guard = trim(fieldText(file, t, "guard"));
        const std::string does = fieldText(file, t, "action");
        transitions.push_back(fieldText(file, t, "from") + ">" + fieldText(file, t, "to") + ">" + fieldText(file, t, "event") +
                              (guard.empty() ? "" : " " + guard) + (does.empty() ? "" : " / " + does));
        model_.find(e.id)->fields["transitions"] = join(transitions, "\n");  // from>to>label, one per line
        events_.push_back({e.id, entityName, scope, fieldText(file, t, "event"), false, locationOf(file, t)});
        const std::string action = fieldText(file, t, "action");
        if (!action.empty()) events_.push_back({e.id, entityName, scope, action, true, locationOf(file, t)});
      }

    } else if (is(c, "diagram")) {
      model::Entity e;
      e.kind = model::Kind::Diagram;
      e.name = fieldText(file, c, "name");
      e.id = model::Identity::ofDiagram(e.name).text();
      e.location = loc;
      bindFields(file, c, e);
      bindFormula(file, c, e);  // a table's rows and columns (DS-268)
      model_.declare(e);
    }
  }
}

}  // namespace bind
}  // namespace ds
