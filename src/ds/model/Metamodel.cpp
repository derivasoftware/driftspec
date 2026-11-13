#include "ds/model/Metamodel.hpp"

#include <algorithm>
#include <stdexcept>

namespace ds {
namespace model {

namespace {

FieldSpec written(const char* name, FieldType type, const char* meaning) { return {name, type, FieldSource::Written, meaning}; }
FieldSpec syntax(const char* name, FieldType type, const char* meaning) { return {name, type, FieldSource::Syntax, meaning}; }
FieldSpec derived(const char* name, FieldType type, const char* meaning) { return {name, type, FieldSource::Derived, meaning}; }

// The fields every artefact carries (REQ-00045-1).
std::vector<FieldSpec> artefact() {
  return {
      written("title", FieldType::Text, "one sentence that names it"), written("text", FieldType::Text, "what it says, in full"),
      written("rationale", FieldType::Text, "why it is so"),           written("status", FieldType::Status, "draft, approved or obsolete"),
      written("tags", FieldType::Names, "words to find it by"),        written("owner", FieldType::Name, "who answers for it")};
}

std::vector<FieldSpec> plus(std::vector<FieldSpec> base, const std::vector<FieldSpec>& more) {
  base.insert(base.end(), more.begin(), more.end());
  return base;
}

const FieldSpec kVisibility = syntax("visibility", FieldType::Name, "- when private to its parent, + otherwise");
const FieldSpec kParams = syntax("params", FieldType::Params, "the parameters, name: type");
const FieldSpec kReturns = syntax("returns", FieldType::Type, "what it gives back");
const FieldSpec kNote = written("note", FieldType::Text, "a remark kept with the declaration");
const FieldSpec kEscapes =
    derived("escapes", FieldType::Names, "the failures that escape it, from where they start and where they are handled (DS-188)");
const FieldSpec kEscapePath =
    derived("escape.*", FieldType::Text, "how a failure escapes it: the operations it comes up through, and the behaviour that starts it");
const FieldSpec kLanguage = written("language", FieldType::Name,
                                    "what its code is written in: cpp, c, python, typescript or lua; held by what it holds unless "
                                    "that says otherwise");

std::vector<KindSpec> describe(std::vector<KindSpec> kinds) {  // how each is written, from the manual (Manual.cpp)
  for (auto& k : kinds) {
    k.syntax = syntaxOf(k.kind);
    k.example = exampleOf(k.kind);
  }
  return kinds;
}

std::vector<KindSpec> build() {
  const Kind P = Kind::Package, M = Kind::Module, Cl = Kind::Class, Rq = Kind::Requirement;
  // an artefact is written in a box, in the requirement it details, or in the class or member it is about (the grammar's
  // _package_item, _module_item, requirement, class_body and member_body; DS-121)
  const std::vector<Kind> aboutAnything = {P, M, Rq, Cl, Kind::Method, Kind::Attribute, Kind::Function, Kind::Operator};
  std::vector<KindSpec> k;
  k.push_back(
      {P,
       "a namespace of the system; packages nest and reopen",
       {written("title", FieldType::Text, "what the package is"), written("owner", FieldType::Name, "who answers for it"), kLanguage},
       {},
       {P},
       true});
  k.push_back({M, "a unit of the design inside a package; modules nest and reopen", {kVisibility, kLanguage}, {"extern"}, {P, M}});
  k.push_back({Rq,
               "what the system or a module promises",
               plus(artefact(), {written("parent", FieldType::Name, "ignored: nesting or @refines say it (W-PARENT-IGNORED)")}),
               {"extension"},
               aboutAnything});
  k.push_back(
      {Kind::Verification,
       "how a requirement or a constraint is proved",
       plus(artefact(),
            {written("method", FieldType::Name, "test, review, analysis or inspection"),
             written("tests", FieldType::Names, "the tests that prove it, file::test"),
             written("test", FieldType::Text, "one test that proves it"), written("evidence", FieldType::Text, "what was observed"),
             written("level", FieldType::Name, "unit, integration or system"), written("precondition", FieldType::Text, "the state before"),
             written("postcondition", FieldType::Text, "the state after")}),
       {"extension"},
       {P, M, Rq}});
  k.push_back({Kind::Constraint,
               "a rule every instance of a category obeys",
               plus(artefact(), {written("category", FieldType::Name, "the category it applies to"),
                                 written("bok", FieldType::Names, "the guides that ground it")}),
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
               {written("text", FieldType::Text, "what is obtained, in a line"),
                syntax("returns", FieldType::Type, "what it gives back when not what the use case returns")},
               {},
               {Kind::UseCase}});
  k.push_back({Kind::Interface,
               "an external interface of a process or a library",
               {written("kind", FieldType::Name, "cli, gui, http, ..."), written("transport", FieldType::Name, "how it travels"),
                written("encoding", FieldType::Text, "how it is serialised")},
               {},
               {P, M}});
  k.push_back({Kind::Operation,
               "an operation an interface offers",
               {kParams, kReturns, kEscapes, kEscapePath, written("command", FieldType::Text, "how it is invoked"),
                written("stdout", FieldType::Text, "what it writes out"), written("stderr", FieldType::Text, "what it writes as errors"),
                written("exit *", FieldType::Name, "the scenario each exit code stands for")},
               {},
               {Kind::Interface}});
  k.push_back({Kind::View, "a state of an interface's navigation", {written("text", FieldType::Text, "what it shows")}, {}, {Kind::State}});
  k.push_back(
      {Cl,
       "a class, a dataclass or an extern type",
       {kVisibility, kNote,
        derived("stereotypes", FieldType::Names,
                "what the check reads from the declaration: interface, abstract, value, equatable, ordered, hashable, printable, extern"),
        syntax("params", FieldType::Params, "the template parameters"), syntax("operators", FieldType::Names, "the operators it declares"),
        syntax("bound.*", FieldType::Type, "the bound of a template parameter")},
       {"dataclass", "extern", "extension"},  // abstract is derived into stereotypes, never a flag
       {M}});
  k.push_back({Kind::Enum, "a closed set of values", {kVisibility, syntax("values", FieldType::Names, "its values")}, {}, {M}});
  k.push_back(
      {Kind::Function, "a free function of a module", {kParams, kReturns, kVisibility, kNote, kEscapes, kEscapePath}, {"extern"}, {M}});
  k.push_back({Kind::Method,
               "an operation of a class",
               {kParams, kReturns, kVisibility, kNote, kEscapes, kEscapePath},
               {"abstract", "static", "const", "constructor", "destructor"},
               {Cl}});
  k.push_back({Kind::Attribute,
               "a field of a class",
               {syntax("type", FieldType::Type, "its type"), kVisibility, syntax("default", FieldType::Text, "its initial value")},
               {"owns", "refs", "shares"},
               {Cl}});
  k.push_back({Kind::Operator, "an operator of a class", {syntax("symbol", FieldType::Name, "the operator"), kParams, kReturns}, {}, {Cl}});
  k.push_back({Kind::Sequence,
               "the behaviour of an operation, a function or a class, as messages in order",
               {syntax("subject", FieldType::Name, "whose behaviour it is"),
                syntax("scenario", FieldType::Name, "the scenario, when written apart"),
                syntax("local.*", FieldType::Type, "a local and its type")},
               {},
               {P, M}});
  k.push_back(
      {Kind::Message,
       "one call of a sequence",
       {syntax("from", FieldType::Name, "who sends it"), syntax("to", FieldType::Name, "who receives it"),
        syntax("op", FieldType::Name, "the operation called"), syntax("args", FieldType::Names, "the arguments"),
        syntax("argc", FieldType::Number, "how many arguments"), syntax("result", FieldType::Name, "the value it produces"),
        syntax("return", FieldType::Text, "what a return gives back"),
        syntax("fails", FieldType::Name, "the failure a path ends with: fail <Failure>"),
        syntax("scenario", FieldType::Name, "the scenario of the callee it follows"),
        derived("type", FieldType::Type, "the type of its target"), derived("result_type", FieldType::Type, "the type of its result"),
        derived("result_scope", FieldType::Name, "where that type resolves"),
        derived("expands", FieldType::Name, "the sequence it expands to"),
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
               {syntax("initial", FieldType::Name, "the initial state"), syntax("transitions", FieldType::Text, "its transitions")},
               {},
               {P, M}});
  k.push_back(
      {Kind::Diagram,
       "a view asked of the model",
       {written("kind", FieldType::Name, "class, sequence, state, requirement or interface"),
        written("include", FieldType::Patterns, "what it shows"), written("exclude", FieldType::Patterns, "what it leaves out"),
        written("open", FieldType::Patterns, "the boxes it closes, lifting their edges"),
        written("depth", FieldType::Number, "how deep a sequence expands, or *"), written("show", FieldType::Names, "the extras it draws"),
        written("hide", FieldType::Names, "the compartments it leaves out"),
        written("neighbours", FieldType::Number, "how many steps of neighbours it adds"),
        written("level", FieldType::Name, "the level of the verifications it shows")},
       {},
       {},
       true});
  k.push_back({Kind::Rule,
               "a design statement that makes a requirement real: what it speaks about, which of them, what holds or must never happen",
               {written("title", FieldType::Text, "what it states, in one sentence"),
                syntax("for", FieldType::Text, "what it speaks about: for <kind> <variable> [in <box>], ..."),
                syntax("where", FieldType::Text, "which of them: literals over the model"),
                syntax("holds", FieldType::Text, "what holds for each of them: literals over the model"),
                syntax("never", FieldType::Text, "what must never happen, instead of holds: literals over the model"),
                written("verified-by", FieldType::Names, "the tests that prove a rule the model cannot evaluate"),
                written("rationale", FieldType::Text, "why it is so")},
               {},
               {P, M, Kind::Class, Kind::Method, Kind::Attribute, Kind::Function, Kind::Operator}});
  k.push_back({Kind::Definition,
               "a relation derived from others: its parameters, and each case in which it holds for them",
               {written("title", FieldType::Text, "what it names, in one sentence"),
                syntax("if", FieldType::Text, "the cases in which it holds: literals over the model, one line each"),
                written("rationale", FieldType::Text, "why it is so")},
               {},
               {P, M, Kind::Class}});
  return k;
}

RelationSpec rel(const char* kind, const char* meaning, std::vector<Kind> from, std::vector<Kind> to) {
  return {kind, meaning, std::move(from), std::move(to), {}};
}

std::vector<RelationSpec> buildRelations() {
  const Kind P = Kind::Package, M = Kind::Module, Rq = Kind::Requirement, V = Kind::Verification, Cs = Kind::Constraint;
  const Kind I = Kind::Interface, Op = Kind::Operation, Vw = Kind::View, Cl = Kind::Class, E = Kind::Enum, F = Kind::Function;
  const Kind Me = Kind::Method, A = Kind::Attribute, O = Kind::Operator, S = Kind::Sequence, Ms = Kind::Message;
  const Kind St = Kind::State, Uc = Kind::UseCase;
  return {
      rel("exposes", "a package or a module offers an interface", {P, M}, {I}),
      rel("requires", "the entity answers to a requirement", {P, M, Cl, E, F, Me, I, Op, Vw, S, Uc, Kind::Rule, Kind::Definition}, {Rq}),
      rel("constrained-by", "the entity obeys a constraint", {P, M, Cl, F, Me, I, Op, Vw}, {Cs}),
      rel("refines", "a requirement details another", {Rq}, {Rq}),
      rel("verifies", "a verification proves a requirement or a constraint", {V}, {Rq, Cs}),
      rel("about", "an artefact concerns the scope it is declared in", {Rq, V, Cs, Uc}, {}),
      rel("realises", "the entity carries out an interface, or an interface operation a use case", {Cl, V, Op}, {I, Uc}),
      rel("exercised-by", "a behaviour is run by a verification", {S}, {V}),
      rel("inherits", "a class derives from another; a use case specialises another", {Cl, Uc}, {Cl, Uc}),
      rel("bounded-by", "a template parameter is bounded by a class", {Cl}, {Cl}),
      rel("owns", "an attribute owns its value", {A}, {Cl, E}),
      rel("refs", "an attribute refers to a value it does not own", {A}, {Cl, E, I}),
      rel("shares", "an attribute shares ownership of its value", {A}, {Cl, E}),
      rel("uses", "the entity names a type", {A, F, Me, Op, O, S, Uc, Kind::Outcome}, {Cl, E, I}),
      rel("behaviour-of", "a sequence or a state machine is the behaviour of its subject", {S, St}, {Cl, F, Me, Op, I}),
      rel("calls", "a message calls an operation", {Ms}, {F, Me, Op}),
      rel("expands-to", "a message expands to the sequence of what it calls", {Ms}, {S}),
      rel("couples-transient", "a behaviour touches a type for the length of a call", {S}, {Cl, E, F, I}),
      rel("couples-structural", "a behaviour reaches a type through an attribute", {S}, {Cl, E}),
      rel("on", "a transition fires on an operation", {St}, {Me, Op}),
      rel("does", "a transition runs an operation", {St}, {Me, Op}),
      rel("consumes", "a box depends on an interface another box offers (@uses)", {P, M, Cl}, {I}),
      rel("refers", "an annotation the language gives no meaning to", {}, {}),
  };
}

bool among(const std::vector<Kind>& kinds, Kind kind) {
  if (kinds.empty()) return true;
  for (Kind k : kinds) {
    if (k == kind) return true;
  }
  return false;
}

bool matches(const std::string& name, const std::string& key) {
  if (!name.empty() && name.back() == '*')
    return key.compare(0, name.size() - 1, name, 0, name.size() - 1) == 0 && key.size() >= name.size();
  return name == key;
}

}  // namespace

const char* fieldTypeName(FieldType type) {
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

const char* fieldSourceName(FieldSource source) {
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

const std::vector<KindSpec>& kinds() {
  static const std::vector<KindSpec> all = describe(build());
  return all;
}

namespace {

// contains goes from a parent to a child exactly where the child may be declared: one pair per parent of each kind
RelationSpec containment() {
  RelationSpec contains{"contains", "the parent holds the child", {}, {}, {}};
  for (const auto& k : kinds()) {
    for (Kind p : k.parents) {
      contains.pairs.push_back({p, k.kind});
      if (std::find(contains.from.begin(), contains.from.end(), p) == contains.from.end()) contains.from.push_back(p);
      if (std::find(contains.to.begin(), contains.to.end(), k.kind) == contains.to.end()) contains.to.push_back(k.kind);
    }
  }
  return contains;
}

}  // namespace

const std::vector<RelationSpec>& relations() {
  static const std::vector<RelationSpec> all = [] {
    std::vector<RelationSpec> r{containment()};
    for (auto& spec : buildRelations()) r.push_back(spec);
    return r;
  }();
  return all;
}

const KindSpec& specOf(Kind kind) {
  for (const auto& k : kinds()) {
    if (k.kind == kind) return k;
  }
  throw std::logic_error(std::string("the metamodel does not declare ") + kindName(kind));
}

const FieldSpec* fieldOf(Kind kind, const std::string& key) {
  for (const auto& f : specOf(kind).fields) {
    if (matches(f.name, key)) return &f;
  }
  return nullptr;
}

const std::vector<ContainerSpec>& containers() {
  static const std::vector<ContainerSpec> all = {
      {"list", "*", 0}, {"set", "*", 0}, {"vector", "*", 0}, {"optional", "0..1", 0}, {"map", "*", 1}};
  return all;
}

const ContainerSpec* containerOf(const std::string& name) {
  const auto at = name.rfind("::");
  const std::string base = at == std::string::npos ? name : name.substr(at + 2);
  for (const auto& c : containers())
    if (c.name == base) return &c;
  return nullptr;
}

const RelationSpec* relationOf(const std::string& kind) {
  for (const auto& r : relations()) {
    if (r.kind == kind) return &r;
  }
  return nullptr;
}

bool joins(const RelationSpec& relation, Kind from, Kind to) {
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
