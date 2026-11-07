#include "ds/bind/Binder.hpp"

#include <cstring>
#include <map>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace bind {

namespace {

bool is(TSNode node, const char* type) { return std::strcmp(ts_node_type(node), type) == 0; }

TSNode child(TSNode node, const char* fieldName) {
  return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName)));
}

std::string fieldText(const parse::ParsedFile& file, TSNode node, const char* fieldName) {
  TSNode c = child(node, fieldName);
  return ts_node_is_null(c) ? std::string() : parse::textOf(file, c);
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

std::string join(const std::vector<std::string>& items, const char* sep) {
  std::string out;
  for (const auto& i : items) out += (out.empty() ? "" : sep) + i;
  return out;
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
  return {file.path, static_cast<int>(start.row) + 1, static_cast<int>(start.column) + 1};
}

std::string Binder::qualify(const std::string& scope, const std::string& name) const {
  // the package gives its name with @, modules with :: (R4): ds@ds::Model, ds@cli
  if (scope.empty()) return name;
  if (scope.back() == '@') return scope + name;
  return scope + "::" + name;
}

void Binder::bindFields(const parse::ParsedFile& file, TSNode node, model::Entity& entity) {
  if (ts_node_is_null(node)) return;
  const uint32_t count = ts_node_named_child_count(node);
  int contracts = 0;
  for (const auto& f : entity.fields) {
    if (f.first.rfind("contract.", 0) == 0) ++contracts;  // keep numbering across trailing contracts and the body
  }
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_named_child(node, i);
    if (is(c, "field")) {
      const std::string code = fieldText(file, c, "code");
      const std::string key = fieldText(file, c, "key") + (code.empty() ? "" : " " + code);
      const std::string value = trim(fieldText(file, c, "value"));
      auto existing = entity.fields.find(key);
      if (existing == entity.fields.end() || existing->second.empty()) {
        entity.fields[key] = value;
      } else {
        existing->second += ", " + value;  // a repeated key accumulates (include: a / include: b)
      }
    } else if (is(c, "contract")) {
      const std::string key = fieldText(file, c, "key");
      entity.fields["contract." + std::to_string(contracts++)] = trim((key.empty() ? "" : key + ": ") + fieldText(file, c, "body"));
    }
  }
}

namespace {

model::Term termOf(const parse::ParsedFile& file, TSNode node) {
  const std::string text = parse::textOf(file, node);
  // a bare name is a variable; an entity is always written qualified (the form decides, nothing is guessed)
  return {text, text.find('@') == std::string::npos && text.find("::") == std::string::npos};
}

/// The text of a clause after its keyword, for the card and the export.
std::string clauseText(const parse::ParsedFile& file, TSNode clause) {
  const std::string text = parse::textOf(file, clause);
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
        l.form = fieldText(file, atom, "operator") == "=" ? model::LiteralForm::Equal : model::LiteralForm::NotEqual;
        l.terms = {termOf(file, child(atom, "left")), termOf(file, child(atom, "right"))};
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
    } else if (is(c, "definition_case")) {
      formula.cases.push_back(literals(c));
      append("if", clauseText(file, c));
    } else if (is(c, "name") && !ts_node_field_name_for_child(node, i)) {
      formula.parameters.push_back(parse::textOf(file, c));  // a definition's parameters
    }
  }
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
    std::string relation = "about";
    if (inRequirement && e.kind == model::Kind::Verification) relation = "verifies";
    if (inRequirement && e.kind == model::Kind::Requirement) relation = "refines";
    model_.relate({relation, e.id, owner, loc, ""});
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
  if (!base.empty()) model_.relate({"inherits", e.id, trim(base), e.location, ""});
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
  use.text = trim(parse::textOf(file, type));
  const std::string name = fieldText(file, type, "name");
  std::vector<TSNode> args;
  TSNode argsNode = child(type, "args");
  if (!ts_node_is_null(argsNode)) {
    const uint32_t n = ts_node_named_child_count(argsNode);
    for (uint32_t i = 0; i < n; ++i) args.push_back(ts_node_named_child(argsNode, i));
  }

  // built-in generics decide the multiplicity and pass the relation to their element (R16);
  // a qualified container (std::vector, std::map) is the same container
  const std::string text = use.text;
  const std::string base = name.substr(name.rfind("::") == std::string::npos ? 0 : name.rfind("::") + 2);
  if (base == "list" || base == "set" || base == "vector") {
    if (!args.empty()) use = bindType(file, args[0], scope, from, relation, "*", params, location);
    use.text = text;
    return use;
  }
  if (base == "optional") {
    if (!args.empty()) use = bindType(file, args[0], scope, from, relation, "0..1", params, location);
    use.text = text;
    return use;
  }
  if (base == "map") {
    if (args.size() > 0) bindType(file, args[0], scope, from, "uses", "", params, location);
    if (args.size() > 1) use = bindType(file, args[1], scope, from, relation, "*", params, location);
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
  ref.target = args.empty() ? name : name + "<" + std::to_string(args.size()) + ">";  // identity = name + arity (R15)
  ref.scope = scope;
  ref.relation = relation;
  ref.expectedKind = model::Kind::Class;
  ref.location = location;
  ref.multiplicity = isOwnership(relation) ? (multiplicity.empty() ? "1" : multiplicity) : "";
  for (TSNode a : args) {
    ref.args.push_back(trim(parse::textOf(file, a)));
    bindType(file, a, scope, from, "uses", "", params, location);  // the argument is used by the holder
  }
  references_.push_back(ref);
  return use;
}

// ---- classes (R14, R15, R17, R19) ----------------------------------------------

void Binder::declareOperator(model::Entity& cls, const std::string& symbol, const model::Location& location) {
  model::Entity op;
  op.kind = model::Kind::Operator;
  op.name = "operator" + symbol;
  op.id = cls.id + "::" + op.name;
  op.scope = cls.id;
  op.location = location;
  op.fields["symbol"] = symbol;
  model_.declare(op);
  model_.relate({"contains", cls.id, op.id, location, ""});
}

void Binder::bindMemberBody(const parse::ParsedFile& file, TSNode member, model::Entity& entity, const std::string& scope) {
  bindFields(file, member, entity);  // trailing contracts, if any
  TSNode body = child(member, "body");
  if (!ts_node_is_null(body)) bindFields(file, body, entity);
  model::Entity* declared = model_.find(entity.id);
  // only the declaration the model kept takes the fields: a duplicate is reported and leaves the first as it was
  const model::Location& at = declared ? declared->location : entity.location;
  if (declared && at.file == entity.location.file && at.line == entity.location.line && at.column == entity.location.column)
    declared->fields = entity.fields;
  if (ts_node_is_null(body)) return;
  bindAnnotations(file, body, entity.id, scope);
  visit(file, body, scope, entity.id);  // nested requirements and constraints are about the member (R12)
}

void Binder::bindMember(const parse::ParsedFile& file, TSNode m, model::Entity& cls, const std::vector<std::string>& params,
                        const std::set<std::string>& operators) {
  const model::Location loc = locationOf(file, m);
  const std::string scope = cls.scope;
  model::Entity member;
  member.scope = cls.id;
  member.location = loc;

  if (is(m, "operator")) {
    const std::string symbol = fieldText(file, m, "symbol");
    member.kind = model::Kind::Operator;
    member.name = "operator" + symbol;
    member.id = cls.id + "::" + member.name;
    member.fields["symbol"] = symbol;
    if (operators.count(symbol) == 0) {
      model_.report({"E-OPERATOR-UNDECLARED", "error",
                     "operator " + symbol + " is refined in the body of " + cls.id + " but is not in its header set", loc});
    }
  } else {
    member.kind = is(m, "method") ? model::Kind::Method : model::Kind::Attribute;
    member.name = fieldText(file, m, "name");
    member.id = cls.id + "::" + member.name;
    member.fields["visibility"] = fieldText(file, m, "visibility");
  }

  if (is(m, "method") || is(m, "operator")) {
    for (TSNode mod : childrenNamed(m, "modifier")) member.flags.push_back(parse::textOf(file, mod));
    if (is(m, "method") && member.name == cls.name) member.flags.push_back("constructor");
    if (cls.has("dataclass") && member.has("abstract")) {
      model_.report(
          {"E-DATACLASS-ABSTRACT", "error", cls.id + " is a dataclass: a value type has no abstract methods (" + member.name + ")", loc});
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
    model_.relate({"contains", cls.id, member.id, loc, ""});
  }
  bindMemberBody(file, m, member, scope);
}

void Binder::bindSignature(const parse::ParsedFile& file, TSNode node, model::Entity& entity, const std::string& scope,
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
      paramTexts.push_back(trim(parse::textOf(file, p)));
      TSNode type = child(p, "type");
      entity.params.push_back({fieldText(file, p, "name"), ts_node_is_null(type) ? "" : trim(parse::textOf(file, type))});
      bindType(file, child(p, "type"), scope, entity.id, "uses", "", params, locationOf(file, p));
    }
  }
  entity.fields["params"] = join(paramTexts, ", ");  // parameters and returns give use (R16)
  TypeUse returns = bindType(file, child(node, "returns"), scope, entity.id, "uses", "", params, locationOf(file, node));
  if (!returns.text.empty()) entity.fields["returns"] = returns.text;
}

void Binder::bindFunction(const parse::ParsedFile& file, TSNode f, const std::string& scope, const std::string& owner) {
  model::Entity e;
  e.kind = model::Kind::Function;
  e.name = fieldText(file, f, "name");
  e.id = qualify(scope, e.name);  // ds@ds::load: a function is named like a class of its module
  e.scope = scope;
  e.location = locationOf(file, f);
  const std::string visibility = fieldText(file, f, "visibility");
  e.fields["visibility"] = visibility.empty() ? "+" : visibility;
  const model::Entity* ownerEntity = owner.empty() ? nullptr : model_.find(owner);
  if (ownerEntity && ownerEntity->has("extern")) e.flags.push_back("extern");  // R11: a name, no obligations
  bindSignature(file, f, e, scope, {});
  model_.declare(e);
  if (!owner.empty()) model_.relate({"contains", owner, e.id, e.location, ""});
  bindMemberBody(file, f, e, scope);  // annotations, contracts and nested artefacts, as a method's
}

void Binder::bindClass(const parse::ParsedFile& file, TSNode c, const std::string& scope, const std::string& owner) {
  const model::Location loc = locationOf(file, c);
  if (fieldText(file, c, "visibility").empty() && ts_node_is_null(child(c, "params")) && childrenNamed(c, "base").empty() &&
      ts_node_is_null(child(c, "operators")) && annotationsOnly(child(c, "body"))) {
    const std::string name = fieldText(file, c, "name");
    extend(file, child(c, "body"), qualify(scope, name), model::Kind::Class, name, scope, loc);  // the annotations sit in the body
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
  e.id = qualify(scope, params.empty() ? e.name : e.name + "<" + std::to_string(params.size()) + ">");  // R15
  if (!params.empty()) e.fields["params"] = join(params, ", ");
  for (const auto& b : bounds) e.fields["bound." + b.first] = b.second;

  std::set<std::string> operators;
  std::vector<std::string> operatorTexts;
  TSNode opSet = child(c, "operators");
  if (!ts_node_is_null(opSet)) {
    const uint32_t n = ts_node_named_child_count(opSet);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode item = ts_node_named_child(opSet, i);
      const std::string text = trim(parse::textOf(file, item));
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
    references_.push_back({declared.id, parse::textOf(file, base), scope, "inherits", model::Kind::Class, loc, "", {}});
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

void Binder::bindSteps(const parse::ParsedFile& file, TSNode node, model::Entity& sequence, const std::string& frame, int& order,
                       int& frames) {
  const uint32_t n = ts_node_child_count(node);
  bool otherwise = false;
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_child(node, i);
    if (!ts_node_is_named(c)) {
      if (std::strcmp(ts_node_type(c), "else") == 0) otherwise = true;
      continue;
    }
    const model::Location loc = locationOf(file, c);
    if (is(c, "message") || is(c, "return")) {
      model::Entity m;
      m.kind = model::Kind::Message;
      m.id = sequence.id + "::m" + std::to_string(order);
      m.scope = sequence.id;
      m.location = loc;
      m.fields["order"] = std::to_string(order);
      const std::string from = fieldText(file, c, "from");
      const std::string to = fieldText(file, c, "to");
      m.fields["from"] = from.empty() ? "self" : from;
      m.fields["to"] = to.empty() ? "self" : to;
      m.fields["op"] = fieldText(file, c, "op");
      if (is(c, "return")) {
        m.fields["result"] = fieldText(file, c, "value");
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
          args.push_back(trim(parse::textOf(file, ts_node_named_child(a, x))));
          ++argc;
        }
      }
      m.fields["args"] = join(args, ", ");
      m.args = args;
      m.fields["argc"] = std::to_string(argc);
      if (!frame.empty()) m.fields["frame"] = frame;
      m.name = is(c, "return") ? "return" : m.fields["to"] + "." + m.fields["op"];
      model_.declare(m);
      model_.relate({"contains", sequence.id, m.id, loc, ""});
      messages_.push_back({sequence.id, m.id, m.fields["to"], m.fields["op"], argc, loc, is(c, "return")});
      if (!frame.empty()) {
        model::Entity* f = model_.find(frame);
        if (f->fields.find("first") == f->fields.end()) f->fields["first"] = m.id;
        f->fields["last"] = m.id;
        if (otherwise && f->fields.find("else") == f->fields.end()) f->fields["else"] = m.id;
      }
      ++order;
    } else if (is(c, "loop") || is(c, "alt") || is(c, "opt")) {
      model::Entity f;
      f.kind = model::Kind::Frame;
      f.id = sequence.id + "::f" + std::to_string(frames++);
      f.scope = sequence.id;
      f.location = loc;
      f.name = ts_node_type(c);
      f.fields["kind"] = ts_node_type(c);
      f.fields["at"] = std::to_string(order);  // where it opens: drawn there even when it holds no message
      f.fields["condition"] = trim(fieldText(file, c, "condition"));
      const std::string scenario = fieldText(file, c, "scenario");
      if (!scenario.empty()) f.fields["scenario"] = scenario;
      const std::string elseScenario = fieldText(file, c, "else_scenario");
      if (!elseScenario.empty()) f.fields["else-scenario"] = elseScenario;
      if (!frame.empty()) f.fields["parent"] = frame;
      model_.declare(f);
      model_.relate({"contains", sequence.id, f.id, loc, ""});
      bindSteps(file, c, sequence, f.id, order, frames);
      // a frame's span also covers its parent's
      model::Entity* child = model_.find(f.id);
      if (!frame.empty() && child->fields.count("first")) {
        model::Entity* parent = model_.find(frame);
        if (parent->fields.find("first") == parent->fields.end()) parent->fields["first"] = child->fields["first"];
        parent->fields["last"] = child->fields["last"];
        if (otherwise && parent->fields.find("else") == parent->fields.end()) parent->fields["else"] = child->fields["first"];
      }
    }
  }
}

void Binder::bindSequence(const parse::ParsedFile& file, TSNode c, const std::string& scope, const std::string& owner) {
  const model::Location loc = locationOf(file, c);
  const std::string subject = fieldText(file, c, "subject");
  const std::string scenario = fieldText(file, c, "scenario");
  model::Entity e;
  e.kind = model::Kind::Sequence;
  e.name = subject + (scenario.empty() ? "" : "/" + scenario);
  e.id = qualify(scope, subject) + "::sequence" + (scenario.empty() ? "" : "/" + scenario);  // identity = the subject (B1)
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
      if (is(s, "loop") || is(s, "alt") || is(s, "opt")) pending.push_back(s);
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
        if (is(part, "package_prefix")) prefix += parse::textOf(file, part);
      }
      model::Entity e;
      e.kind = model::Kind::Package;
      e.name = fieldText(file, c, "name");
      e.scope = prefix.empty() ? scope : prefix;
      e.id = e.scope + e.name;  // every package level adds its name with @: app, app@core, app@core@model::Entity (R4)
      if (fieldText(file, c, "visibility") == "-") e.fields["visibility"] = "-";
      e.location = loc;
      bindFields(file, c, e);
      model::Entity& declared = model_.declare(e);
      for (const auto& f : e.fields) declared.fields[f.first] = f.second;
      bindAnnotations(file, c, declared.id, declared.id + "@");  // an HLD box realises requirements like a class does
      if (!owner.empty())
        model_.relate({"contains", owner, declared.id, loc, ""});
      else if (!prefix.empty())  // the outer package must be declared somewhere; the checker turns part-of into contains
        references_.push_back({declared.id, prefix.substr(0, prefix.size() - 1), "", "part-of", model::Kind::Package, loc, "", {}});
      visit(file, c, declared.id + "@", declared.id);

    } else if (is(c, "module")) {
      model::Entity e;
      e.kind = model::Kind::Module;
      e.name = fieldText(file, c, "name");
      e.id = qualify(scope, e.name);
      e.scope = scope;
      e.location = loc;
      const model::Entity* ownerEntity = owner.empty() ? nullptr : model_.find(owner);
      if (!ts_node_is_null(child(c, "extern")) || (ownerEntity && ownerEntity->has("extern"))) e.flags.push_back("extern");  // R11
      const bool isPrivate = fieldText(file, c, "visibility") == "-";
      if (isPrivate) e.fields["visibility"] = "-";
      bindFields(file, c, e);
      model::Entity& declared = model_.declare(e);
      for (const auto& f : e.fields) {  // a module reopens: its fields add up, and two that differ are both kept
        auto& kept = declared.fields[f.first];
        if (kept.empty())
          kept = f.second;
        else if (kept != f.second && (", " + kept + ",").find(", " + f.second + ",") == std::string::npos)
          kept += ", " + f.second;
      }
      if (isPrivate) declared.fields["visibility"] = "-";  // a module reopens: any declaration may say it
      if (!owner.empty()) model_.relate({"contains", owner, declared.id, loc, ""});
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
      const bool inClass = in && in->kind == model::Kind::Class;  // a rule of a class is named under it: p@m::Order::priced
      e.id = inClass ? owner + "::" + e.name : qualify(scope, e.name);
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
      e.id = qualify(scope, e.name);  // hola@shell: an external interface of the box
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
        if (!realised.empty()) model_.relate({"realises", o.id, trim(realised), o.location, ""});
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
      e.id = qualify(scope, e.name);
      e.scope = scope;
      e.location = loc;
      std::vector<std::string> values;
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode v = ts_node_named_child(c, j);
        if (is(v, "enum_value")) values.push_back(trim(parse::textOf(file, v)));
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
      e.id = qualify(scope, entityName) + "::state";
      e.scope = scope;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
      references_.push_back({e.id, entityName, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
      bindAnnotations(file, c, e.id, scope);
      std::vector<std::string> transitions;
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode t = ts_node_named_child(c, j);
        if (is(t, "initial")) model_.find(e.id)->fields["initial"] = fieldText(file, t, "name");
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
      e.id = "diagram:" + e.name;
      e.location = loc;
      bindFields(file, c, e);
      model_.declare(e);
    }
  }
}

}  // namespace bind
}  // namespace ds
