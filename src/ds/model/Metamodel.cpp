#include "ds/model/Metamodel.hpp"

#include <algorithm>
#include <stdexcept>

// The metamodel's kinds and relations, built once when it is made (DS-341); its catalogue of codes is in
// Codes.cpp and how each kind is written in WrittenForms.cpp.

namespace ds {
namespace model {

Metamodel::Metamodel()
    : names_(buildNames()),
      forms_(writtenForms()),
      kinds_(describe(build())),
      identityFields_(buildIdentityFields()),
      relations_(buildRelations()),
      codes_(buildCodes()),
      known_(buildKnownAnnotations()),
      other_(buildOtherAnnotation()),
      annotations_(buildAnnotations()) {}

FieldSpec Metamodel::writtenField(const std::string& name, FieldType type, const std::string& meaning) {
  return {name, type, FieldSource::Written, meaning};
}
FieldSpec Metamodel::syntax(const std::string& name, FieldType type, const std::string& meaning) {
  return {name, type, FieldSource::Syntax, meaning};
}
FieldSpec Metamodel::derived(const std::string& name, FieldType type, const std::string& meaning) {
  return {name, type, FieldSource::Derived, meaning};
}
// a field written with one of a closed set of values: any other is W-FIELD-VALUE (DS-211)
FieldSpec Metamodel::closed(const std::string& name, FieldType type, const std::string& meaning, std::vector<std::string> values) {
  return {name, type, FieldSource::Written, meaning, std::move(values)};
}

FieldSpec Metamodel::holdingIdentity(FieldSpec field) {
  field.identity = true;
  return field;
}

std::vector<std::string> Metamodel::buildIdentityFields() const {
  std::vector<std::string> out;
  for (const auto& k : kinds_)
    for (const auto& f : k.fields)
      if (f.identity && std::find(out.begin(), out.end(), f.name) == out.end()) out.push_back(f.name);
  return out;
}

// The fields every artefact carries (REQ-00045-1).
std::vector<FieldSpec> Metamodel::artefact() {
  return {writtenField("title", FieldType::Text, "one sentence that names it"),
          writtenField("text", FieldType::Text, "what it says, in full"),
          writtenField("rationale", FieldType::Text, "why it is so"),
          closed("status", FieldType::Status, "draft, approved or obsolete", {"draft", "approved", "obsolete"}),
          writtenField("tags", FieldType::Names, "words to find it by"),
          writtenField("owner", FieldType::Name, "who answers for it")};
}

std::vector<FieldSpec> Metamodel::plus(std::vector<FieldSpec> base, const std::vector<FieldSpec>& more) {
  base.insert(base.end(), more.begin(), more.end());
  return base;
}

std::vector<KindSpec> Metamodel::describe(std::vector<KindSpec> kinds) const {  // how each is written (WrittenForms.cpp)
  for (auto& k : kinds) {
    k.syntax = syntaxOf(k.kind);
    k.example = exampleOf(k.kind);
  }
  return kinds;
}

std::vector<KindSpec> Metamodel::build() {
  // the fields several kinds carry the same
  const FieldSpec kVisibility = syntax("visibility", FieldType::Name, "- when private to its parent, + otherwise");
  const FieldSpec kParams = syntax("params", FieldType::Params, "the parameters, name: type");
  const FieldSpec kReturns = syntax("returns", FieldType::Type, "what it gives back");
  const FieldSpec kNote = writtenField("note", FieldType::Text, "a remark kept with the declaration");
  const FieldSpec kFails =
      syntax("fails", FieldType::Names, "what it promises it may fail with: fails A, B; fails nothing promises none (DS-189)");
  const FieldSpec kEscapes =
      derived("escapes", FieldType::Names, "the failures that escape it, from where they start and where they are handled (DS-188)");
  const FieldSpec kEscapePath = derived("escape.*", FieldType::Text,
                                        "how a failure escapes it: the operations it comes up through, and the behaviour that starts it");
  const FieldSpec kLanguage = writtenField("language", FieldType::Name,
                                           "what its code is written in: cpp, c, python, typescript or lua; held by what it holds unless "
                                           "that says otherwise");
  const Kind P = Kind::Package, M = Kind::Module, Cl = Kind::Class, Rq = Kind::Requirement;
  // an artefact is written in a box, in the requirement it details, or in the class or member it is about (the grammar's
  // _package_item, _module_item, requirement, class_body and member_body; DS-121)
  const std::vector<Kind> aboutAnything = {P, M, Rq, Cl, Kind::Method, Kind::Attribute, Kind::Function, Kind::Operator};
  std::vector<KindSpec> k;
  k.push_back({P,
               "a box of the system: organisation above the code; packages nest and reopen",
               {writtenField("title", FieldType::Text, "what the package is"), writtenField("owner", FieldType::Name, "who answers for it"),
                kLanguage},
               {},
               {P},
               true});
  k.push_back({M, "a namespace of the code inside a package; modules nest and reopen", {kVisibility, kLanguage}, {}, {P, M}});
  k.push_back({Rq,
               "what the system or a module promises",
               plus(artefact(), {writtenField("parent", FieldType::Name, "ignored: nesting or @refines say it (W-PARENT-IGNORED)")}),
               {"extension"},
               aboutAnything});
  k.push_back({Kind::Verification,
               "how a requirement or a constraint is proved",
               plus(artefact(), {closed("method", FieldType::Name, "test, review, analysis or inspection",
                                        {"test", "review", "analysis", "inspection"}),
                                 writtenField("tests", FieldType::Names, "the tests that prove it, file::test"),
                                 writtenField("test", FieldType::Text, "one test that proves it"),
                                 writtenField("evidence", FieldType::Text, "what was observed"),
                                 closed("level", FieldType::Name, "unit, integration or system", {"unit", "integration", "system"}),
                                 writtenField("precondition", FieldType::Text, "the state before"),
                                 writtenField("postcondition", FieldType::Text, "the state after")}),
               {"extension"},
               {P, M, Rq}});
  k.push_back({Kind::Constraint,
               "a requirement every instance of what it binds obeys",
               plus(artefact(), {writtenField("category", FieldType::Name, "the category it applies to"),
                                 writtenField("bok", FieldType::Names, "the guides that ground it")}),
               {"extension"},
               aboutAnything});
  k.push_back(
      {Kind::UseCase,
       "a contract an interface operation realises: what goes in, what comes out, the outcomes it ends in",
       plus(artefact(), {syntax("operation", FieldType::Name, "the name of its contract, when it has a signature"), kParams, kReturns}),
       {},
       {P, M}});
  k.push_back({Kind::Outcome,
               "one way a use case ends: default when it goes well, a named scenario when it does not",
               {writtenField("text", FieldType::Text, "what is obtained, in a line"),
                syntax("returns", FieldType::Type, "what it gives back when not what the use case returns")},
               {},
               {Kind::UseCase}});
  k.push_back({Kind::Interface,
               "an external interface of a process or a library",
               {writtenField("kind", FieldType::Name, "cli, gui, http, ..."), writtenField("transport", FieldType::Name, "how it travels"),
                writtenField("encoding", FieldType::Text, "how it is serialised")},
               {},
               {P, M}});
  k.push_back(
      {Kind::Operation,
       "an operation an interface offers",
       {kParams, kReturns, kFails, kEscapes, kEscapePath, writtenField("command", FieldType::Text, "how it is invoked"),
        writtenField("stdout", FieldType::Text, "what it writes out"), writtenField("stderr", FieldType::Text, "what it writes as errors"),
        writtenField("exit *", FieldType::Name, "the scenario each exit code stands for")},
       {},
       {Kind::Interface}});
  k.push_back(
      {Kind::View, "a state of an interface's navigation", {writtenField("text", FieldType::Text, "what it shows")}, {}, {Kind::State}});
  k.push_back(
      {Cl,
       "a class or a dataclass",
       {kVisibility, kNote,
        derived("stereotypes", FieldType::Names,
                "what the check reads from the declaration: interface, abstract, value, equatable, ordered, hashable, printable"),
        syntax("params", FieldType::Params, "the template parameters"), syntax("operators", FieldType::Names, "the operators it declares"),
        syntax("bound.*", FieldType::Type, "the bound of a template parameter"),
        syntax("bases", FieldType::Text, "its bases as written, template arguments and all")},
       {"dataclass", "extension"},  // abstract is derived into stereotypes, never a flag
       {M}});
  k.push_back({Kind::Enum, "a closed set of values", {kVisibility, syntax("values", FieldType::Names, "its values")}, {}, {M}});
  k.push_back({Kind::Function,
               "a free function of a module",
               {kParams, kReturns, kVisibility, kNote, kFails, kEscapes, kEscapePath,
                syntax("template", FieldType::Names, "its own template parameters")},
               {},
               {M}});
  k.push_back({Kind::Method,
               "an operation of a class",
               {kParams, kReturns, kVisibility, kNote, kFails, kEscapes, kEscapePath,
                syntax("template", FieldType::Names, "its own template parameters")},
               {"abstract", "static", "const", "constructor", "destructor"},
               {Cl}});
  k.push_back({Kind::Attribute,
               "a field of a class",
               {syntax("type", FieldType::Type, "its type"), kVisibility, syntax("default", FieldType::Text, "its initial value")},
               {"owns", "refs", "shares", "many", "optional"},
               {Cl}});
  k.push_back({Kind::Operator, "an operator of a class", {syntax("symbol", FieldType::Name, "the operator"), kParams, kReturns}, {}, {Cl}});
  k.push_back({Kind::Sequence,
               "the behaviour of an operation, a function or a class, as messages in order",
               {syntax("subject", FieldType::Name, "whose behaviour it is"),
                syntax("scenario", FieldType::Name, "the scenario, when written apart"),
                syntax("local.*", FieldType::Type, "a local and its type")},
               {},
               {P, M}});
  k.push_back({Kind::Message,
               "one call of a sequence",
               {syntax("from", FieldType::Name, "who sends it"), syntax("to", FieldType::Name, "who receives it"),
                syntax("op", FieldType::Name, "the operation called"), syntax("args", FieldType::Names, "the arguments"),
                syntax("argc", FieldType::Number, "how many arguments"), syntax("result", FieldType::Name, "the value it produces"),
                syntax("return", FieldType::Text, "what a return gives back"),
                syntax("fails", FieldType::Name, "the failure a path ends with: fail <Failure>"),
                syntax("scenario", FieldType::Name, "the scenario of the callee it follows"),
                derived("type", FieldType::Type, "the type of its target"),
                derived("from-type", FieldType::Type, "who sends it, when the sender is written"),
                derived("result_type", FieldType::Type, "the type of its result"),
                derived("result_scope", FieldType::Name, "where that type resolves"),
                holdingIdentity(derived("expands", FieldType::Name, "the sequence it expands to")),
                derived("expands-path", FieldType::Names, "the path through that sequence")},
               {},
               {Kind::Sequence, Kind::Frame}});
  k.push_back({Kind::Frame,
               "a loop, alt, opt or attempt around the steps it holds, in the order written; an alt holds its sides as "
               "branches, an attempt its handlers",
               {syntax("kind", FieldType::Name, "loop, alt, opt, attempt, branch for a side of an alt, handler of an attempt"),
                syntax("condition", FieldType::Text, "its guard, as prose"),
                syntax("failure", FieldType::Name, "the failure a handler handles: on <Failure>"),
                syntax("scenario", FieldType::Name, "the scenario a loop, an opt or a branch names")},
               {},
               {Kind::Sequence, Kind::Frame}});
  k.push_back({Kind::State,
               "the state machine of a class or the navigation of an interface",
               {syntax("initial", FieldType::Name, "the initial state"), syntax("transitions", FieldType::Text, "its transitions"),
                syntax("rules", FieldType::Text, "its states as rules over the context of its subject: state|literals|line, one per line")},
               {},
               {P, M}});
  k.push_back(
      {Kind::Diagram,
       "a view asked of the model",
       {closed("kind", FieldType::Name, "class, sequence, state, table, requirement, interface or tour",
               {"class", "sequence", "state", "table", "requirement", "interface", "tour"}),
        writtenField("include", FieldType::Patterns, "what it shows"), writtenField("exclude", FieldType::Patterns, "what it leaves out"),
        writtenField("open", FieldType::Patterns, "the boxes it keeps open, and the way to each; alone, every other box closes"),
        writtenField("close", FieldType::Patterns,
                     "the boxes it closes, by name, prefix or kind (module, package); what a closed box holds is lifted to it"),
        writtenField("cut", FieldType::Name, "the diagram whose open and close it takes as well"),
        writtenField("depth", FieldType::Number, "how deep a sequence expands, or *"),
        writtenField("unfold", FieldType::Patterns,
                     "the calls a sequence follows: *, a box, an operation; alone, every other call is folded"),
        writtenField("fold", FieldType::Patterns, "the calls a sequence does not follow"),
        writtenField("show", FieldType::Names, "the extras it draws"),
        writtenField("hide", FieldType::Names, "the compartments it leaves out"),
        writtenField("neighbours", FieldType::Number, "how many steps of neighbours it adds"),
        syntax("for", FieldType::Text, "the rows of a table: what it ranges over, as a rule does"),
        syntax("where", FieldType::Text, "the rows of a table that stay: the literals each must answer")},
       {},
       {},
       true});
  k.push_back({Kind::Rule,
               "a design statement that makes a requirement real: what it speaks about, which of them, what holds or must never happen",
               {writtenField("title", FieldType::Text, "what it states, in one sentence"),
                syntax("for", FieldType::Text, "what it speaks about: for <kind> <variable> [in <box>], ..."),
                syntax("where", FieldType::Text, "which of them: literals over the model"),
                syntax("holds", FieldType::Text, "what holds for each of them: literals over the model"),
                syntax("never", FieldType::Text, "what must never happen, instead of holds: literals over the model"),
                writtenField("verified-by", FieldType::Names, "the tests that prove a rule the model cannot evaluate"),
                writtenField("rationale", FieldType::Text, "why it is so")},
               {},
               {P, M, Kind::Class, Kind::Method, Kind::Attribute, Kind::Function, Kind::Operator}});
  k.push_back({Kind::Definition,
               "a relation derived from others: its parameters, and each case in which it holds for them",
               {writtenField("title", FieldType::Text, "what it names, in one sentence"),
                syntax("if", FieldType::Text, "the cases in which it holds: literals over the model, one line each"),
                writtenField("rationale", FieldType::Text, "why it is so")},
               {},
               {P, M, Kind::Class}});
  return k;
}

RelationSpec Metamodel::rel(const std::string& kind, const std::string& meaning, std::vector<Kind> from, std::vector<Kind> to) {
  return {kind, meaning, std::move(from), std::move(to), {}};
}

// containment first, derived from where each kind may be declared, then every other relation as declared
std::vector<RelationSpec> Metamodel::buildRelations() const {
  const Kind P = Kind::Package, M = Kind::Module, Rq = Kind::Requirement, V = Kind::Verification, Cs = Kind::Constraint;
  const Kind I = Kind::Interface, Op = Kind::Operation, Vw = Kind::View, Cl = Kind::Class, E = Kind::Enum, F = Kind::Function;
  const Kind Me = Kind::Method, A = Kind::Attribute, O = Kind::Operator, S = Kind::Sequence, Ms = Kind::Message;
  const Kind St = Kind::State, Uc = Kind::UseCase;
  return {
      containment(),
      rel("exposes", "a package or a module offers an interface", {P, M}, {I}),
      // whatever may nest an artefact may annotate it, and one source set for both (DS-217)
      rel("satisfies", "the entity satisfies a requirement", {P, M, Cl, E, F, Me, A, O, I, Op, Vw, S, St, Uc, Kind::Rule, Kind::Definition},
          {Rq}),
      rel("constrained-by", "the entity obeys a constraint", {P, M, Cl, E, F, Me, A, O, I, Op, Vw, S, St, Uc, Kind::Rule, Kind::Definition},
          {Cs}),
      rel("refines", "a requirement details another", {Rq}, {Rq}),
      rel("verifies", "a verification proves a requirement or a constraint", {V}, {Rq, Cs}),
      rel("about", "an artefact concerns the box it is declared in, where it gives its level", {Rq, V, Cs, Uc}, {}),
      rel("realises", "the entity carries out an interface, or an interface operation a use case", {Cl, V, Op}, {I, Uc}),
      rel("exercised-by", "a behaviour is run by a verification", {S}, {V}),
      rel("inherits", "a class derives from another; a use case specialises another", {Cl, Uc}, {Cl, Uc}),
      rel("bounded-by", "a template parameter is bounded by a class", {Cl}, {Cl}),
      rel("owns", "an attribute owns its value", {A}, {Cl, E}),
      rel("refs", "an attribute refers to a value it does not own", {A}, {Cl, E, I}),
      rel("shares", "an attribute shares ownership of its value", {A}, {Cl, E}),
      rel("uses", "the entity names a type", {A, F, Me, Op, O, S, Uc, Kind::Outcome}, {Cl, E, I}),
      rel("behaviour-of", "a sequence or a state machine is the behaviour of its subject", {S, St}, {Cl, F, Me, Op, I, P, M, Uc}),
      rel("calls", "a message calls an operation", {Ms}, {F, Me, Op}),
      rel("expands-to", "a message expands to the sequence of what it calls", {Ms}, {S}),
      rel("couples-transient", "a behaviour touches a type for the length of a call", {S}, {Cl, E, F, I}),
      rel("couples-structural", "a behaviour reaches a type through an attribute", {S}, {Cl, E, I}),  // an interface held by refs (DS-209)
      rel("on", "a transition fires on an operation", {St}, {Me, Op}),
      rel("does", "a transition runs an operation", {St}, {Me, Op}),
      rel("consumes", "a box depends on an interface another box offers (@consumes)", {P, M, Cl}, {I}),
      rel("sends", "in a scenario written between them, one participant sends a message to another", {P, M, I, Cl}, {P, M, I, Cl, E, F}),
      rel("refers", "an annotation the language gives no meaning to", {}, {}),
  };
}

bool Metamodel::among(const std::vector<Kind>& kinds, Kind kind) {
  if (kinds.empty()) return true;
  for (Kind k : kinds) {
    if (k == kind) return true;
  }
  return false;
}

bool Metamodel::matches(const std::string& name, const std::string& key) {
  if (!name.empty() && name.back() == '*')
    return key.compare(0, name.size() - 1, name, 0, name.size() - 1) == 0 && key.size() >= name.size();
  return name == key;
}

std::string Metamodel::fieldTypeName(FieldType type) const {
  switch (type) {
    case FieldType::Text:
      return "text";
    case FieldType::Name:
      return "name";
    case FieldType::Names:
      return "names";
    case FieldType::Type:
      return "type";
    case FieldType::Params:
      return "params";
    case FieldType::Number:
      return "number";
    case FieldType::Status:
      return "status";
    case FieldType::Patterns:
      return "patterns";
  }
  return "text";
}

std::string Metamodel::fieldSourceName(FieldSource source) const {
  switch (source) {
    case FieldSource::Written:
      return "written";
    case FieldSource::Syntax:
      return "syntax";
    case FieldSource::Derived:
      return "derived";
  }
  return "written";
}

// each kind's name, by its place in Kind: the switch is read once, here, and nameOf answers from the table. The
// kinds are numbered from 0 in the order Kind declares them, and the first number that names no kind ends it.
std::vector<std::string> Metamodel::buildNames() {
  const auto written = [](Kind kind) -> const char* {
    switch (kind) {
      case Kind::Package:
        return "package";
      case Kind::Module:
        return "module";
      case Kind::Requirement:
        return "requirement";
      case Kind::Verification:
        return "verification";
      case Kind::Interface:
        return "interface";
      case Kind::View:
        return "view";
      case Kind::Operation:
        return "operation";
      case Kind::Constraint:
        return "constraint";
      case Kind::Class:
        return "class";
      case Kind::Enum:
        return "enum";
      case Kind::Function:
        return "function";
      case Kind::Method:
        return "method";
      case Kind::Attribute:
        return "attribute";
      case Kind::Operator:
        return "operator";
      case Kind::Sequence:
        return "sequence";
      case Kind::Message:
        return "message";
      case Kind::Frame:
        return "frame";
      case Kind::State:
        return "state";
      case Kind::Diagram:
        return "diagram";
      case Kind::UseCase:
        return "usecase";
      case Kind::Outcome:
        return "outcome";
      case Kind::Rule:
        return "rule";
      case Kind::Definition:
        return "definition";
    }
    return "?";
  };
  std::vector<std::string> names;
  for (int k = 0;; ++k) {
    names.emplace_back(written(static_cast<Kind>(k)));
    if (names.back() == "?") return names;
  }
}

const std::string& Metamodel::nameOf(Kind kind) const {
  const auto at = static_cast<std::size_t>(kind);
  return names_[at < names_.size() ? at : names_.size() - 1];
}

// contains goes from a parent to a child exactly where the child may be declared: one pair per parent of each kind
RelationSpec Metamodel::containment() const {
  RelationSpec contains{"contains", "the parent holds the child", {}, {}, {}};
  for (const auto& k : kinds_) {
    for (Kind p : k.parents) {
      contains.pairs.push_back({p, k.kind});
      if (std::find(contains.from.begin(), contains.from.end(), p) == contains.from.end()) contains.from.push_back(p);
      if (std::find(contains.to.begin(), contains.to.end(), k.kind) == contains.to.end()) contains.to.push_back(k.kind);
    }
  }
  return contains;
}

const KindSpec& Metamodel::specOf(Kind kind) const {
  for (const auto& k : kinds_) {
    if (k.kind == kind) return k;
  }
  throw std::logic_error(std::string("the metamodel does not declare ") + nameOf(kind));
}

const KindSpec* Metamodel::specNamed(const std::string& name) const {
  for (const auto& k : kinds_)
    if (name == nameOf(k.kind)) return &k;
  return nullptr;
}

const FieldSpec* Metamodel::fieldOf(Kind kind, const std::string& key) const {
  for (const auto& f : specOf(kind).fields) {
    if (matches(f.name, key)) return &f;
  }
  return nullptr;
}

const RelationSpec* Metamodel::relationOf(const std::string& kind) const {
  for (const auto& r : relations_) {
    if (r.kind == kind) return &r;
  }
  return nullptr;
}

// The annotations that mean something, the binder's table (DS-340: the metamodel's, not the manual's).
std::vector<AnnotationSpec> Metamodel::buildKnownAnnotations() {
  return {
      {"@requirement", "satisfies", "the entity answers to the requirement; on a verification, verifies it", "verifies", Kind::Requirement},
      {"@constraint", "constrained-by", "the entity obeys the constraint; on a verification, verifies it", "verifies", Kind::Constraint},
      {"@refines", "refines", "the requirement details the one named (nesting says the same)", "", Kind::Requirement},
      {"@interface", "realises", "the class or verification carries out the interface", "", Kind::Interface},
      {"@consumes", "consumes", "the package, module or class depends on an interface another box offers", "", Kind::Interface},
      {"@verification", "exercised-by", "the sequence is run by the verification", "", Kind::Verification},
  };
}

// any other word: kept as a link, and said (DS-210)
AnnotationSpec Metamodel::buildOtherAnnotation() {
  return {"@<any other word>", "refers", "kept as a link the language gives no meaning to, and reported (W-ANNOTATION-UNKNOWN)"};
}

// what the manual lists: the known annotations, any other word, and what an operation writes after a colon,
// which is no annotation but makes a relation the same way
std::vector<AnnotationSpec> Metamodel::buildAnnotations() const {
  std::vector<AnnotationSpec> all = known_;
  all.push_back(other_);
  all.push_back({": <UC ID> after an operation", "realises", "not an annotation: an interface operation realises the use case it names"});
  return all;
}

const AnnotationSpec& Metamodel::annotationOf(const std::string& word) const {
  for (const auto& a : known_)
    if (a.name == "@" + word) return a;
  return other_;
}

bool Metamodel::isArtefact(Kind kind) const {
  return kind == Kind::Requirement || kind == Kind::Verification || kind == Kind::Constraint || kind == Kind::UseCase;
}

bool Metamodel::joins(const RelationSpec& relation, Kind from, Kind to) const {
  if (!relation.pairs.empty()) {
    for (const auto& p : relation.pairs) {
      if (p.first == from && p.second == to) return true;
    }
    return false;
  }
  return among(relation.from, from) && among(relation.to, to);
}

}  // namespace model
}  // namespace ds
