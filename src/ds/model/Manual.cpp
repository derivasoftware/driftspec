#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Words.hpp"

// The manual ds embeds (DS-79): how each kind is written with a minimal example that checks
// clean, every finding code with what it means and what to do, the annotations that mean
// something and the commands with their exit codes. One source: `ds manual` prints it and the
// tests hold it to the language (every example checks clean, every code reported is here).

namespace ds {
namespace model {

namespace {

const std::string kShopClass =
    "package shop {\n"
    "  module store {\n"
    "    dataclass Item {\n"
    "      + name: string\n"
    "      + cents: int\n"
    "    }\n"
    "    class Cart {\n"
    "      - owns items: list<Item>\n"
    "      + add(item: Item) -> none\n"
    "      + total() -> int\n"
    "    }\n"
    "  }\n"
    "}\n";

const std::string kShopInterface =
    "package shop {\n"
    "  interface shell {\n"
    "    kind: cli\n"
    "    operation buy(item: string) -> int {\n"
    "      command: shop buy <item>\n"
    "      exit 0: default\n"
    "    }\n"
    "  }\n"
    "\n"
    "  sequence shell::buy {\n"
    "    return 0\n"
    "  }\n"
    "}\n";

const std::string kShopSequence =
    "package shop {\n"
    "  module store {\n"
    "    class Stock {\n"
    "      + has(item: string) -> bool\n"
    "    }\n"
    "    class Cart {\n"
    "      - stock: Stock\n"
    "      + add(item: string) -> bool\n"
    "    }\n"
    "\n"
    "    sequence Cart::add {\n"
    "      self -> stock.has(item) -> ok\n"
    "      opt the item is out of stock / out {\n"
    "        return false\n"
    "      }\n"
    "      return ok\n"
    "    }\n"
    "  }\n"
    "}\n";

const std::string kShopUseCase =
    "package shop {\n"
    "  usecase UC-00001-1 buy(item: string) -> int {\n"
    "    title: Buy an item\n"
    "    status: draft\n"
    "    outcome default: the number of the order\n"
    "    outcome sold-out: nothing is bought\n"
    "  }\n"
    "\n"
    "  interface shell {\n"
    "    kind: cli\n"
    "    operation buy(item: string) -> int : UC-00001-1 {\n"
    "      command: shop buy <item>\n"
    "      exit 0: default\n"
    "      exit 1: sold-out\n"
    "    }\n"
    "  }\n"
    "\n"
    "  sequence shell::buy {\n"
    "    return 0\n"
    "  }\n"
    "\n"
    "  sequence shell::buy / sold-out {\n"
    "    return 1\n"
    "  }\n"
    "}\n";

const std::map<Kind, std::pair<std::string, std::string>>& written() {
  static const std::map<Kind, std::pair<std::string, std::string>> all = {
      {Kind::Package,
       {"package <name> { ... }\npackage <outer>@<inner> { ... }      reopens the nested package <inner>\nlanguage: cpp | c | "
        "python | typescript | lua     what its code is written in; what it holds says the same unless it says otherwise",
        "package shop {\n  module store { }\n}\n"}},
      {Kind::Module,
       {"[-] [extern] module <name> { ... }\nlanguage: <language>     only when it differs from the box around it",
        "package shop {\n  language: cpp\n  module store {\n    module pricing { }\n  }\n  module web {\n    language: python\n  }\n}\n"}},
      {Kind::Requirement,
       {"requirement <ID> {\n  title: <one sentence: The <subject> shall <verb> <object>>\n  status: draft | approved | obsolete\n  text:\n"
        "    <what it says, indented>\n}\nrequirement <ID>: <title>             the one-line form\nnested in a requirement, it refines "
        "it; elsewhere write @refines <ID>",
        "package shop {\n  requirement SREQ-00001-1 {\n    title: The shop shall take an order\n    status: draft\n"
        "    requirement REQ-00001-1: The cart shall total its items\n  }\n}\n"}},
      {Kind::Verification,
       {"verification <ID> {\n  title: <how it is proved>\n  method: test | review | analysis | inspection\n  tests:\n"
        "    <file>::<Fixture>.<Test>\n}\nnested in what it verifies, or with @requirement <ID>",
        "package shop {\n  requirement REQ-00001-1 {\n    title: The cart shall total its items\n    status: draft\n"
        "    verification VER-00001-1 {\n      title: Unit tests confirm the cart totals its items\n      method: test\n"
        "      tests:\n        tests/test_cart.cpp::TestCart.Totals\n    }\n  }\n}\n"}},
      {Kind::Constraint,
       {"constraint <ID>: <title>\nconstraint <ID> {\n  title: <a rule every instance of a category obeys>\n  category: <category>\n"
        "  bok: <guides>\n}",
        "package shop {\n  constraint CST-00001-1: Every price shall be kept in cents\n}\n"}},
      {Kind::Interface,
       {"interface <name> {          in a package: an external interface of what the package builds\n  kind: cli | gui | http | ...\n"
        "  operation ...\n}",
        kShopInterface}},
      {Kind::Operation,
       {"operation <name>(<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] {\n  command: <how it is invoked>\n  "
        "exit <code>: <scenario>     default, "
        "or a scenario of its sequence\n}\nsequence <interface>::<operation> [/ <scenario>] { ... } says how it is handled",
        kShopInterface}},
      {Kind::View,
       {"view <Name> {               in the state of an interface: its navigation\n  text: <what it shows>\n}",
        "package shop {\n  interface web {\n    kind: gui\n    operation buy(item: string) -> none\n  }\n\n  sequence web::buy {\n  }\n\n"
        "  state web {\n    initial Home\n    view Home {\n      text: The items, and a button to buy one.\n    }\n"
        "    view Done {\n      text: What was bought.\n    }\n    Home -> Done on buy\n    Done -> Home on back\n  }\n}\n"}},
      {Kind::Class,
       {"[+|-] class | dataclass <Name>[<T: Bound>] [: <Base>, ...] [<<operators>>] { members }\nextern in an extern module: a "
        "name with no obligations\ninterface and abstract are derived from abstract methods, never written",
        kShopClass}},
      {Kind::Enum,
       {"enum <Name> { <Value>, <Value> = 2, ... }",
        "package shop {\n  module store {\n    enum Status { Open, Paid, Shipped }\n  }\n}\n"}},
      {Kind::Function,
       {"[-] function <name>(<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] [{ @requirement <ID> }]",
        "package shop {\n  module store {\n    function total(cents: list<int>) -> int\n  }\n}\n"}},
      {Kind::Method,
       {"+|-|# [abstract | static | const] <name>(<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] [{ "
        "body }]\nin a class; named like the class, "
        "it is a constructor; named ~ and the class, its destructor, which no interface counts against",
        kShopClass}},
      {Kind::Attribute,
       {"+|- [owns | refs | shares] <name>: <type> [= <default>]\nowns (the default) is composition, refs and shares aggregation; "
        "list<T>, set<T>, map<K,T> give *, optional<T> gives 0..1",
        kShopClass}},
      {Kind::Operator,
       {"in the header:  class <Name> <<==, ordered, hash, print, +, -, *, []>>\nin the body:    operator <symbol>(<param>: <type>) [-> "
        "<type>]     refines one the header names",
        "package shop {\n  module money {\n    dataclass Cents <<==, ordered, +>> {\n      + value: int\n"
        "      operator +(other: Cents) -> Cents\n    }\n  }\n}\n"}},
      {Kind::Sequence,
       {"sequence <Class>::<method> [/ <scenario>] { ... }\nsequence <function> | <interface>::<operation> [/ <scenario>] { ... }\n"
        "a scenario is a path: written apart, or named on a branch\n"
        "fail <Failure> ends a path failing; attempt { ... } on <Failure> [/ <scenario>] { ... } handles what its steps fail "
        "with",
        kShopSequence}},
      {Kind::Message,
       {"[<from>] -> [<target>.]<op>(<args>) [/ <scenario>] [-> <result>]\nself -> <attribute>.<op>(<args>) -> <result>\nreturn "
        "<value>\nfail <Failure>\nlocal <name>: <type>",
        kShopSequence}},
      {Kind::Frame,
       {"loop <prose> { ... }\nopt <prose> [/ <scenario>] { ... }\nalt <prose> [/ <scenario>] { ... } else [/ <scenario>] { ... }\n"
        "attempt { ... } on <Failure> [/ <scenario>] { ... } [on ...]",
        kShopSequence}},
      {Kind::State,
       {"state <Class | interface> {\n  initial <State>\n  <A> -> <B> on <operation> [<guard, as prose>] / <action>\n}",
        "package shop {\n  module store {\n    class Order {\n      + pay() -> none\n      + ship() -> none\n    }\n\n"
        "    state Order {\n      initial Open\n      Open -> Paid on pay\n      Paid -> Shipped on ship\n    }\n  }\n}\n"}},
      {Kind::UseCase,
       {"usecase <ID> [<name>(<param>: <type>, ...) [-> <type>]] [: <generic ID>] {\n  title: <what it is for>\n  status: draft | "
        "approved | obsolete\n  @requirement <ID>\n  outcome default: <what is obtained>\n  outcome <scenario> [-> <type>]: <what is "
        "obtained instead>\n}\nan interface operation realises it: operation <name>(...) -> <type> : <UC ID>\nit names no "
        "interface: whoever drives the interface, the contract is the same",
        kShopUseCase}},
      {Kind::Rule,
       {"rule <name> {\n  title: <what it states>\n  @requirement <ID>\n  for <kind> <variable> [in <box>], ...\n  where <literals>\n"
        "  holds <literals>               or never <literals>\n}\nliterals joined by commas, each one of:\n  <a> <relation> <b>    a "
        "relation "
        "of the metamodel: c uses x, m requires r\n  <a> is <kind>         x is class\n  <a> in <b>            containment, at any depth\n "
        " "
        "<a> = <b>, !=, <, <=, >, >=   entities or values: numbers compare as numbers\n  <definition>(<a>, ...)\n  arg(<call>, <param>, "
        "<v>)"
        "   the call passes v, a literal written in its sequence, to param\n  not <literal>         once its variables are bound\na bare "
        "name "
        "is a variable; an entity is written qualified: shop@store, shop@store::Order; a value is a number or a \"string\"\nin a package, "
        "a module, a class or a member: a member's rule speaks about it and its calls, what a precondition says\nevaluated over the "
        "model: holds is broken by each binding with no answer, never by each binding with one",
        "package shop {\n  requirement REQ-00001-1: The store shall not depend on the web\n  module web {\n    class Page { }\n  }\n"
        "  module store {\n    class Order {\n      + total() -> int\n    }\n    rule store-blind-to-web {\n"
        "      title: No member of the store names a type of the web\n      @requirement REQ-00001-1\n      for class c in shop@store\n"
        "      never m in c, m uses x, x in shop@web\n    }\n  }\n}\n"}},
      {Kind::Definition,
       {"definition <name>(<parameter>, ...) {\n  title: <what it names>\n  if <literals>             one line per case in which it "
        "holds\n}\na rule or another definition applies it: <name>(<a>, ...); it may recur, and negate only what does not rest on it",
        "package shop {\n  module store {\n    class Order {\n      + total() -> int\n    }\n    definition member-of(m, c) {\n"
        "      title: A member and the class that holds it\n      if m in c, c is class\n    }\n  }\n}\n"}},
      {Kind::Outcome,
       {"outcome default: <what is obtained>           the ending that goes well\noutcome <scenario> [-> <type>]: <what is "
        "obtained>   any other: every operation that realises the use case handles it as a scenario",
        kShopUseCase}},
      {Kind::Diagram,
       {"diagram <Name> {            at the top of a file, outside any package\n  kind: class | sequence | state | requirement | "
        "interface\n  include: <patterns>      shop@*, shop@store::*, shop@store::Cart\n  exclude, open, depth, show, hide: ...\n}",
        "package shop {\n  module store {\n    class Cart { }\n  }\n}\n\ndiagram CL_Store {\n  kind: class\n  include: "
        "shop@store::*\n}\n"}},
  };
  return all;
}

std::vector<CodeSpec> buildCodes() {
  return {
      {"E-SYNTAX", "error",
       "A file does not parse at the place shown. What parses around it still reaches the model, so other findings in that file "
       "may come from the cut rather than from the design.",
       "Fix this one first and check again. The message says what the place takes; ds manual grammar says what goes where."},
      {"E-DUPLICATE-IDENTITY", "error", "Two declarations name the same identity: one model has one entity per identity.",
       "Rename one, or reopen the scope instead of declaring it again."},
      {"E-MSG-ARITY", "error", "A message passes more or fewer arguments than the operation it calls takes.",
       "Match the arguments to the operation's parameters."},
      {"E-OPERATOR-UNDECLARED", "error", "A class refines an operator in its body that its header does not name.",
       "Add the symbol to the header set: class Vector <<==, +>>."},
      {"E-OWNERSHIP-CYCLE", "error", "Classes own each other in a cycle: a composition must be a tree.",
       "Make one of the attributes refs or shares instead of owns."},
      {"E-REF-AMBIGUOUS", "error", "A name is declared by more than one extern module.", "Qualify it: module::Name."},
      {"E-RELATION-KIND", "error",
       "A reference would join two kinds the language does not relate that way, such as a class that names an interface as its base.",
       "Realise an interface with @interface <name>; otherwise name something of the kind the relation takes."},
      {"E-REF-UNPINNED", "error", "A link to a requirement or a constraint leaves out its revision.",
       "Write the full id, with its revision: REQ-00001-2."},
      {"E-REFINES-CYCLE", "error", "Requirements refine each other in a cycle: a requirement cannot detail itself.",
       "Break the chain: one of them is the parent."},
      {"E-INHERITANCE-CYCLE", "error", "Classes or use cases derive from each other in a cycle.",
       "Break the chain: one of them is the base."},
      {"E-TEMPLATE-BOUND", "error", "A template argument does not satisfy the bound of its parameter.",
       "Pass a type that derives from the bound, or change the bound."},
      {"E-DATACLASS-ABSTRACT", "error", "A dataclass declares an abstract method: a value type has no abstract behaviour.",
       "Make it a class, or give the method a body."},
      {"W-REF-UNDECLARED", "warning", "Something is named that the workspace declares nowhere; it stays in the model as expected.",
       "Declare it, qualify the name, or declare it extern."},
      {"W-REVISION-DRIFT", "warning", "A link names an older revision of an artefact than the one the workspace declares.",
       "Point the link at the current revision."},
      {"W-REVISION-STALE", "warning", "ds diff: an artefact changed between two versions but kept its revision.",
       "Write it as the next revision: <ID>-(N+1)."},
      {"W-PARENT-IGNORED", "warning", "A requirement carries a parent: field, which the language does not read.",
       "Nest it in its parent, or write @refines <ID>."},
      {"W-FIELD-UNKNOWN", "warning",
       "A field that its kind does not declare in the metamodel: a typo, or a field the language does not know.",
       "ds manual <kind> lists the fields of each kind."},
      {"W-ANNOTATION-UNKNOWN", "warning",
       "An annotation word the language gives no meaning to: a typo, or a word no check reads. It is kept as a link.",
       "ds manual annotations lists the words that mean something."},
      {"W-DIAGRAM-UNDECLARED", "warning",
       "A diagram's include, exclude or open names something the workspace declares nowhere: it draws less and says nothing.",
       "Correct the pattern, or declare what it names."},
      {"W-RULE-NEVER-MATCHES", "warning",
       "A relation literal binds an end to a kind the relation never joins there, so it matches nothing and the rule holds by vacuity.",
       "Name the members: m in c, m uses x. ds manual <kind> lists the relations of each kind."},
      {"W-REFINES-UPWARD", "warning", "A requirement refines one written inside its own box: the detail stands above what it details.",
       "Turn the link round, or move one of them to the box it belongs to."},
      {"W-INTERFACE-SIGNATURE", "warning",
       "A class declares an operation of an interface it realises with another signature than the interface's.",
       "Make the two take and give the same. What the class does not declare is no finding."},
      {"W-HANDLER-UNREACHED", "warning",
       "A handler names a failure that no step of its attempt can fail with: it handles nothing, often a misspelt name.",
       "Correct the failure's name, or remove the handler."},
      {"W-FIELD-VALUE", "warning",
       "A closed field (status, method, a verification's level, a diagram's kind or level) carries a value it does not take.",
       "ds manual <kind> lists the values of each closed field."},
      {"W-MODIFIER-ON-VALUE", "warning", "owns, refs or shares on a value type (int, string...) means nothing.", "Drop the modifier."},
      {"W-MSG-UNDECLARED", "warning", "A message calls an operation its target does not declare.",
       "Declare the operation, or correct the name or the target."},
      {"W-MSG-NOT-STATIC", "warning", "A message calls a class's operation on the class itself, and the operation is not static.",
       "Make the operation static, or call it through an instance: a local, an attribute or a parameter."},
      {"W-PARTICIPANT-UNDECLARED", "warning",
       "A message is sent to something that is neither the subject, an attribute, a local nor an earlier result.",
       "Declare it with local <name>: <type>, or name an attribute."},
      {"W-ARG-TYPE", "warning", "An argument's type does not match the parameter it is passed to.", "Pass a value of the declared type."},
      {"W-RESULT-NONE", "warning", "A message names a result of an operation that returns nothing.",
       "Drop the result, or declare what the operation returns."},
      {"W-RETURN-TYPE", "warning", "A sequence returns a value of another type than its subject declares.",
       "Return the declared type, or correct the declaration."},
      {"W-SCENARIO-UNDECLARED", "warning", "A message or an exit names a scenario its target's behaviour does not have.",
       "Write that scenario (sequence X / name, or a branch / name), or correct the name."},
      {"W-SCENARIO-TWICE", "warning", "One behaviour names the same scenario on two branches: a scenario is one path.",
       "Give each path its own name."},
      {"W-OPERATION-NO-BEHAVIOUR", "warning", "An interface operation has no sequence that says how it is handled.",
       "Write sequence <interface>::<operation> { ... }."},
      {"W-OUTCOME-MISMATCH", "warning", "An operation promises an exit code on a scenario whose trace ends in another.",
       "Make the exit and the scenario's return agree."},
      {"W-EVENT-UNDECLARED", "warning", "A transition of a class's state machine fires on something that is not an operation of the class.",
       "Name an operation of the class, or declare it."},
      {"W-ACTION-UNDECLARED", "warning", "A transition runs an action that is not an operation of the class.",
       "Name an operation of the class, or declare it."},
      {"W-VIEW-UNDECLARED", "warning", "The navigation of an interface names a view it does not declare.",
       "Declare the view, or correct the name."},
      {"W-VIEW-UNREACHABLE", "warning", "Nothing leads to a view of an interface's navigation.",
       "Add a transition into it, or make it the initial one."},
      {"W-STATE-UNREACHABLE", "warning", "Nothing leads to a state of a class's machine from its initial state.",
       "Add a transition into it, or remove the state."},
      {"W-FAILURE-UNDECLARED", "warning",
       "An operation lets a failure escape that its signature does not say it may fail with; an interface operation is a "
       "boundary and must say it.",
       "Handle it in an attempt, or add it to the signature: fails <Failure>."},
      {"W-FAILURE-UNREACHED", "warning", "An operation says it may fail with something no path of its behaviour fails with.",
       "Remove it from the signature, or write the path that fails with it."},
      {"W-VIEW-NO-WAY-OUT", "warning", "Nothing leaves a view of an interface's navigation.", "Add a transition out of it."},
      {"W-NOT-VISIBLE", "warning", "Something reaches a private member or box from outside it.",
       "Go through a public member, or make it public."},
      {"W-PRIVATE-LEAK", "warning", "Something public shows a private type in its signature.", "Make the type public, or hide the member."},
      {"W-DIAGRAM-DEPTH", "warning", "A diagram asks for a depth that is neither a number nor *.", "Write a number, or * for every level."},
      {"W-REQ-NO-VERIFICATION", "warning", "An approved requirement that nothing verifies.", "Nest a verification in it, or link one."},
      {"W-VER-NO-TEST", "warning", "A verification by test names no tests.", "List them under tests: as <file>::<Fixture>.<Test>."},
      {"W-RULE-NO-REQUIREMENT", "warning", "A rule answers to no requirement: nothing says what it makes real.",
       "Link it: @requirement <ID> in its body."},
      {"W-RULE-NO-CLAIM", "warning", "A rule says neither what holds nor what must never happen, or says both.",
       "Write one of holds: <condition> or never: <condition>."},
      {"W-DEFINITION-NO-CASE", "warning", "A definition says no case in which it holds: nothing ever belongs to it.",
       "Write if <literals>."},
      {"E-RULE-BROKEN", "error", "The model breaks a rule: a binding of its ranges with no answer to holds, or with one to never.",
       "Change the design so the rule holds, or the rule if it says what nobody meant."},
      {"E-RULE-UNKNOWN", "error",
       "A rule or a definition names a kind, a relation, an entity or a definition that the language or the model does not declare.",
       "Write a kind or a relation of the metamodel, a qualified entity that exists, a definition in scope."},
      {"E-RULE-ARITY", "error", "A definition is applied to a number of arguments it does not take.", "Pass one per parameter."},
      {"E-RULE-UNSAFE", "error",
       "A variable is tested by a negation or a comparison, or is a parameter of a definition, and no positive literal binds it.",
       "Name it in a relation, a kind, a containment or a definition first."},
      {"E-RULE-UNSTRATIFIED", "error", "A definition negates itself through recursion: no order of evaluation decides it.",
       "Break the cycle: negate only definitions that do not rest on the one being defined."},
      {"W-USES-UNDECLARED", "warning",
       "A behaviour calls an operation of an interface another box offers, and neither it nor any box it lives in declares @uses of it.",
       "Declare @uses <interface> on the caller, its module or its package (or in a wiring file), or call something you depend on."},
      {"W-USES-UNUSED", "warning", "A box declares @uses of an interface and none of its behaviours calls it.",
       "Remove the @uses, or write the behaviour that calls the interface."},
      {"W-USES-BYPASS", "warning",
       "A behaviour calls a method of a class that realises an interface, from outside its module, instead of the interface.",
       "Type the local or the parameter by the interface and call its operation, and declare @uses of it."},
      {"W-LANGUAGE-UNKNOWN", "warning",
       "A package or a module says a language ds does not know, or two at once (two declarations of a box it reopens).",
       "Write one of cpp, c, python, typescript or lua, once."},
      {"W-USECASE-UNDECLARED", "warning", "An operation realises, or a use case specialises, an id that is no use case of the workspace.",
       "Declare the use case, or correct the id after the colon."},
      {"W-USECASE-SIGNATURE", "warning",
       "An operation that realises a use case, or a use case that specialises another, takes or gives something else than the contract.",
       "Make the parameters and the return agree with the use case."},
      {"W-USECASE-OUTCOME-UNHANDLED", "warning", "An operation realises a use case but has no scenario for one of its outcomes.",
       "Write the scenario (sequence <interface>::<operation> / <outcome>, or a branch named so) and an exit for it."},
      {"W-SCENARIO-NO-OUTCOME", "warning", "An operation that realises a use case handles a scenario none of its outcomes names.",
       "Add the outcome to the use case, or rename the scenario to the outcome it is."},
      {"W-USECASE-UNREALISED", "warning", "No interface operation realises a use case, nor any use case that specialises it.",
       "Realise it from an operation (operation <name>(...) : <UC ID>), or drop it."},
  };
}

}  // namespace

std::string syntaxOf(Kind kind) {
  const auto it = written().find(kind);
  return it == written().end() ? "" : it->second.first;
}

std::string exampleOf(Kind kind) {
  const auto it = written().find(kind);
  return it == written().end() ? "" : it->second.second;
}

const std::vector<CodeSpec>& codes() {
  static const std::vector<CodeSpec> all = buildCodes();
  return all;
}

const CodeSpec* codeOf(const std::string& code) {
  for (const auto& c : codes())
    if (c.code == code) return &c;
  return nullptr;
}

const std::vector<AnnotationSpec>& annotations() {
  static const std::vector<AnnotationSpec> all = {
      {"@requirement", "requires", "the entity answers to the requirement; on a verification, verifies it", "verifies", Kind::Requirement},
      {"@constraint", "constrained-by", "the entity obeys the constraint; on a verification, verifies it", "verifies", Kind::Constraint},
      {"@refines", "refines", "the requirement details the one named (nesting says the same)", "", Kind::Requirement},
      {"@interface", "realises", "the class or verification carries out the interface", "", Kind::Interface},
      {"@uses", "consumes", "the package, module or class depends on an interface another box offers", "", Kind::Interface},
      {"@verification", "exercised-by", "the sequence is run by the verification", "", Kind::Verification},
      {"@<any other word>", "refers", "kept as a link the language gives no meaning to, and reported (W-ANNOTATION-UNKNOWN)"},
      {": <UC ID> after an operation", "realises", "not an annotation: an interface operation realises the use case it names"},
  };
  return all;
}

const AnnotationSpec& annotationOf(const std::string& word) {
  for (const auto& a : annotations())
    if (a.name == "@" + word) return a;
  static const AnnotationSpec& other =
      *std::find_if(annotations().begin(), annotations().end(), [](const AnnotationSpec& a) { return a.name == "@<any other word>"; });
  return other;
}

const std::vector<CommandSpec>& commands() {
  static const std::vector<CommandSpec> all = {
      {"check",
       "ds check <workspace> [--json]",
       "the findings of a workspace, then the counts of its model",
       {{0, "no error among the findings (warnings may be)"},
        {1, "an error is among the findings"},
        {2, "bad usage, or no workspace at the path"}}},
      {"export",
       "ds export <workspace>",
       "the model as JSON: entities, relations, findings and its metamodel",
       {{0, "the model, even with findings"}, {2, "bad usage, or no workspace at the path"}}},
      {"render",
       "ds render <workspace|ir.json> [diagram] [-f html|json|svg|text|cells|ir|layout] [-w N] [-o file] [--embed [--height N]] [--css "
       "file]",
       "the whole model in the viewer (html, the default), its scene (json), or one diagram drawn (svg, text, cells) or as its data "
       "(ir, layout)",
       {{0, "drawn"},
        {1, "no such diagram"},
        {2, "bad usage (a format that needs a diagram without one), or nothing to read at the path"}}},
      {"diff",
       "ds diff <A> <B> [path] [diagram] [-f text|svg|cells] [-w N]",
       "the changes between two versions in the model's terms; A and B are directories, or git revisions with the path of the "
       "workspace inside the repository",
       {{0, "compared"},
        {1, "a version does not parse, or the diff raises findings, drawn or not"},
        {2, "bad usage (-f without a diagram), or a version that is no workspace"}}},
      {"query",
       "ds query <workspace> <question> [subject] [--json]",
       "ask the model: diagrams · entities <scope> · relations <id> · artefacts <id> · behaviours <id> · shows <diagram> · findings "
       "[file] · coupling · failures <operation>",
       {{0, "answered"}, {1, "not a question, or an identity the model does not declare"}, {2, "bad usage, or no workspace at the path"}}},
      {"manual",
       "ds manual [topic] [--json]",
       "this manual: the index, a kind (class, sequence...), a finding code (W-REF-UNDECLARED...), annotations, commands or codes",
       {{0, "the page"}, {1, "no such topic"}, {2, "bad usage"}}},
  };
  return all;
}

const CommandSpec* commandOf(const std::string& name) {
  for (const auto& c : commands())
    if (c.name == name) return &c;
  return nullptr;
}

}  // namespace model
}  // namespace ds

namespace ds {
namespace model {

namespace {

std::string indent(const std::string& text, const std::string& by) {
  std::string out;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    out += by + text.substr(start, end - start) + "\n";
    start = end + 1;
  }
  return out;
}

std::string pad(const std::string& text, size_t width) {
  return text.size() >= width ? text + " " : text + std::string(width - text.size(), ' ');
}

std::string names(const std::vector<Kind>& kinds) {
  std::string out;
  for (size_t i = 0; i < kinds.size(); ++i) out += (i ? ", " : "") + std::string(kindName(kinds[i]));
  return out;
}

const KindSpec* kindNamed(const std::string& name) {
  for (const auto& k : kinds())
    if (name == kindName(k.kind)) return &k;
  return nullptr;
}

std::string kindPage(const KindSpec& k) {
  std::string out = std::string(kindName(k.kind)) + " — " + k.meaning + "\n";
  out += "declared in: " + (k.parents.empty() ? std::string("the top of a file") : names(k.parents)) +
         (k.top && !k.parents.empty() ? ", or at the top" : "") + "\n\n";
  out += "How it is written\n" + indent(k.syntax, "  ") + "\n";
  if (!k.fields.empty()) {
    out += "Fields\n";
    for (const auto& f : k.fields)
      out += "  " + pad(f.name, 14) + pad(fieldTypeName(f.type), 9) + pad(fieldSourceName(f.source), 9) + f.meaning +
             (f.values.empty() ? "" : " (one of: " + model::join(f.values, ", ") + ")") + "\n";
    out += "  (written: you write it as key: value; syntax: read from how it is declared; derived: computed by the check)\n\n";
  }
  if (!k.flags.empty()) {
    out += "Flags: ";
    for (size_t i = 0; i < k.flags.size(); ++i) out += (i ? ", " : "") + k.flags[i];
    out += "\n\n";
  }
  std::string from, to;
  for (const auto& r : relations()) {
    if (r.kind == "contains" || r.kind == "refers") continue;
    for (Kind f : r.from)
      if (f == k.kind)
        from += "  " + pad(r.kind, 20) + "to " + (r.to.empty() ? std::string("anything") : names(r.to)) + "   " + r.meaning + "\n";
    for (Kind t : r.to)
      if (t == k.kind) to += "  " + pad(r.kind, 20) + "from " + (r.from.empty() ? std::string("anything") : names(r.from)) + "\n";
  }
  if (!from.empty()) out += "Relations from it\n" + from + "\n";
  if (!to.empty()) out += "Relations to it\n" + to + "\n";
  out += "Example (a workspace that checks clean)\n" + indent(k.example, "  ");
  return out;
}

std::string indexPage() {
  std::string out =
      "ds — a software design as text: checked, drawn, compared and asked.\n\n"
      "A workspace is a directory of .ds files, read as one model in which every entity has one identity.\n"
      "Write its high-level design (packages, interfaces), its low-level design (modules, classes, functions,\n"
      "sequences, state machines) and the artefacts about it (requirements, constraints, verifications) in it;\n"
      "then check it, look at it, compare two versions of it, ask it questions or hand it to another tool.\n\n"
      "Commands (ds manual commands for their exit codes)\n";
  for (const auto& c : commands()) out += "  " + pad(c.name, 8) + c.does + "\n";
  out += "\nKinds you write (ds manual <kind>: how it is written, its fields and an example)\n  ";
  size_t column = 2;
  for (const auto& k : kinds()) {
    const std::string name = kindName(k.kind);
    if (column + name.size() > 100) {
      out += "\n  ";
      column = 2;
    }
    out += name + "  ";
    column += name.size() + 2;
  }
  out +=
      "\n\nAlso: ds manual grammar (what goes where, identities, annotations) · ds manual annotations · ds manual codes ·\n"
      "ds manual <CODE> (E-SYNTAX, W-REF-UNDECLARED...)\n";
  return out;
}

std::string grammarPage() {
  std::string out =
      "How a workspace is written\n\n"
      "Files\n"
      "  A workspace is every .ds file under a directory, read as one model; files may split a package or a module any way.\n"
      "  One thing per line. // starts a comment. A field is key: value to the end of its line; a longer value goes on\n"
      "  the lines after key:, indented.\n\n"
      "Identities\n"
      "  Packages join with @, everything inside a package with ::  —  shop@store::Cart::add\n"
      "  A package reopens (package shop { } twice, or package shop@store { }); a name is resolved from the innermost\n"
      "  scope outwards, or qualified: store::Cart. Artefacts (requirements, verifications, constraints) are named by\n"
      "  their id, which ends in its revision: REQ-00001-1. The prefix (SREQ, REQ, VER, CST...) is a convention; the\n"
      "  level comes from where it is written.\n\n"
      "What goes inside what\n";
  for (const auto& k : kinds()) {
    std::string inside;
    for (const auto& other : kinds())
      for (Kind parent : other.parents)
        if (parent == k.kind) inside += (inside.empty() ? "" : ", ") + std::string(kindName(other.kind));
    if (!inside.empty()) out += "  " + pad(kindName(k.kind), 14) + inside + "\n";
  }
  out +=
      "  (and at the top of a file: package, diagram)\n\n"
      "Annotations\n"
      "  @<word> <ID> on a line of its own, inside the body of what it is about: a package, a module, a class, a member, a\n"
      "  function, an enum, an interface, an operation, a sequence, a requirement, a verification, a constraint.\n"
      "  ds manual annotations says which words mean something.\n\n"
      "Definitions and extensions\n"
      "  A body that holds only annotations does not define the entity again: it extends one defined elsewhere\n"
      "  (W-REF-UNDECLARED when nothing defines it). A class that nothing else defines is the class, empty: an\n"
      "  exception that only carries its requirements. Give it a member, a field or a value and it is a definition.\n\n"
      "Behaviour\n"
      "  A sequence is the behaviour of an operation, a method or a function: sequence Cart::add { ... }. A message\n"
      "  goes to self, an attribute, a local (local x: Type) or an earlier result: self -> stock.has(item) -> ok.\n"
      "  A scenario is one path through it, named: written apart (sequence Cart::add / out { ... }) or on a branch\n"
      "  (opt the item is out / out { ... }). A call names the scenario it enters: -> add(item) / out -> r.\n"
      "  A path ends failing where a return would go: fail ParseError, the failure's name as text, whatever the\n"
      "  mechanism. An attempt runs its steps and handles what they fail with, one on each: attempt { ... } on IoError\n"
      "  { fail LoadError } translates it, a handler that returns resolves it, a failure no handler names goes on.\n"
      "  The default path does not fail; a handler that names a scenario is the path of that failure.\n"
      "  What escapes each operation is computed: ds query <workspace> failures <operation>. A signature promises it:\n"
      "  + load(path: string) -> Book fails LoadError, ParseError, or fails nothing; an interface operation must say it.\n"
      "  An operation with no behaviour fails as its signature says, so a library call can be said to fail.\n\n"
      "The counts ds check prints\n"
      "  entities · expected (named but declared nowhere: W-REF-UNDECLARED) · relations · findings\n";
  return out;
}

}  // namespace

std::vector<std::string> manualTopics() {
  std::vector<std::string> out{"grammar", "commands", "annotations", "codes"};
  for (const auto& k : kinds()) out.push_back(kindName(k.kind));
  for (const auto& c : codes()) out.push_back(c.code);
  return out;
}

std::string manualPage(const std::string& topic) {
  if (topic.empty()) return indexPage();
  if (topic == "grammar") return grammarPage();
  if (const KindSpec* k = kindNamed(topic)) return kindPage(*k);
  if (const CodeSpec* c = codeOf(topic)) return c->code + " (" + c->severity + ")\n\n" + c->meaning + "\n\nWhat to do: " + c->fix + "\n";
  if (topic == "commands") {
    std::string out;
    for (const auto& c : commands()) {
      out += c.synopsis + "\n  " + c.does + "\n";
      for (const auto& e : c.exits) out += "  exit " + std::to_string(e.first) + "  " + e.second + "\n";
      out += "\n";
    }
    return out;
  }
  if (topic == "annotations") {
    std::string out = "An annotation links the entity it is written in to an artefact: @<word> <ID>.\n\n";
    for (const auto& a : annotations()) out += "  " + pad(a.name, 20) + pad(a.relation, 16) + a.meaning + "\n";
    return out;
  }
  if (topic == "codes") {
    std::string out;
    for (const auto& c : codes()) out += "  " + pad(c.code, 26) + pad(c.severity, 9) + c.meaning + "\n";
    return out + "\nds manual <CODE> says what to do about one.\n";
  }
  return "";
}

}  // namespace model
}  // namespace ds
