#include <stdexcept>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"

// The catalogue of finding codes (DS-79): every code the engine reports, with what it means and what to do
// about it. The metamodel's: the checker reads it for coverage and the manual prints it (DS-340).

namespace ds {
namespace model {

std::vector<SeveritySpec> Metamodel::buildSeverities() {
  std::vector<SeveritySpec> out{
      {Severity::error, "error", true, false, "error"},
      {Severity::warning, "warning", false, false, "warning"},
      {Severity::coverage, "coverage", false, true, "information"},
  };
  for (std::size_t i = 0; i < out.size(); ++i)  // a row out of its place would describe another severity
    if (static_cast<std::size_t>(out[i].id) != i)
      throw std::logic_error("the severity table is out of the order of Severity at " + out[i].name);
  return out;
}

std::vector<CodeSpec> Metamodel::buildCodes() {
  std::vector<CodeSpec> out{
      {Code::E_SYNTAX, "E-SYNTAX", Severity::error,
       "A file does not parse at the place shown; a block left open is shown where it opens. What parses around it still reaches "
       "the model, and until the file parses the findings that only echo the cut are left out: the rest of that file's, and "
       "every name declared nowhere.",
       "Fix this one first and check again. The message says what the place takes; ds manual grammar says what goes where."},
      {Code::E_ENCODING, "E-ENCODING", Severity::error,
       "A file is not UTF-8: the byte at the place shown is not part of a character. It is a continuation byte nothing leads, "
       "a byte that leads nothing (0xC0, 0xC1, 0xF5 to 0xFF), or the lead of a character cut short, written in an overlong "
       "form, a surrogate or above U+10FFFF. The place is the line and the column of that byte, both counted from 1 and the "
       "column in bytes, as E-SYNTAX counts them. Nothing of the file is read, and until it is UTF-8 the findings that only "
       "echo its absence are left out: every name declared nowhere. The other files are read and checked as usual.",
       "Save the file as UTF-8 and check again. A byte from 0x80 to 0xFF on its own is most often a letter written in Latin-1 or "
       "Windows-1252, such as 0xE9 for \u00e9."},
      {Code::E_NAME_RESERVED, "E-NAME-RESERVED", Severity::error,
       "A member is named state or sequence and its holder has that behaviour: a state machine is X::state and a sequence "
       "X::name::sequence, so the member takes the identity of the behaviour.",
       "Rename the member: status, phase, steps, order. A member with that name and no such behaviour is fine."},
      {Code::E_DUPLICATE_IDENTITY, "E-DUPLICATE-IDENTITY", Severity::error,
       "Two declarations name the same identity: one model has one entity per identity.",
       "Rename one, or reopen the scope instead of declaring it again."},
      {Code::E_MSG_ARITY, "E-MSG-ARITY", Severity::error, "A message passes more or fewer arguments than the operation it calls takes.",
       "Match the arguments to the operation's parameters."},
      {Code::E_MODIFIER_CONFLICT, "E-MODIFIER-CONFLICT", Severity::error,
       "An attribute is written with two ownerships (owns, refs, shares) or two multiplicities (many, optional), or one of them twice.",
       "Keep one of each: - refs many watchers: std::set<Watcher*>."},
      {Code::E_OPERATOR_UNDECLARED, "E-OPERATOR-UNDECLARED", Severity::error,
       "A class refines an operator in its body that its header does not name.",
       "Add the symbol to the header set: class Vector <<==, +>>."},
      {Code::E_OWNERSHIP_CYCLE, "E-OWNERSHIP-CYCLE", Severity::error, "Classes own each other in a cycle: a composition must be a tree.",
       "Make one of the attributes refs or shares instead of owns."},
      {Code::E_RELATION_KIND, "E-RELATION-KIND", Severity::error,
       "A reference would join two kinds the language does not relate that way, such as a class that names an interface as its base.",
       "Realise an interface with @interface <name>; otherwise name something of the kind the relation takes."},
      {Code::E_REF_UNPINNED, "E-REF-UNPINNED", Severity::error, "A link to a requirement or a constraint leaves out its revision.",
       "Write the full id, with its revision: REQ-00001-2."},
      {Code::E_REFINES_CYCLE, "E-REFINES-CYCLE", Severity::error,
       "Requirements refine each other in a cycle: a requirement cannot detail itself.", "Break the chain: one of them is the parent."},
      {Code::E_INHERITANCE_CYCLE, "E-INHERITANCE-CYCLE", Severity::error, "Classes or use cases derive from each other in a cycle.",
       "Break the chain: one of them is the base."},
      {Code::E_TEMPLATE_BOUND, "E-TEMPLATE-BOUND", Severity::error, "A template argument does not satisfy the bound of its parameter.",
       "Pass a type that derives from the bound, or change the bound."},
      {Code::E_DATACLASS_ABSTRACT, "E-DATACLASS-ABSTRACT", Severity::error,
       "A dataclass declares an abstract method: a value type has no abstract behaviour.", "Make it a class, or give the method a body."},
      {Code::W_REF_UNDECLARED, "W-REF-UNDECLARED", Severity::warning,
       "An annotation, a sequence's subject or a link names something the workspace declares nowhere; it stays in the model as "
       "expected. A name in a type, a base, a bound or a call is never this: it is text.",
       "Declare it, or qualify the name."},
      {Code::W_NAME_AMBIGUOUS, "W-NAME-AMBIGUOUS", Severity::warning,
       "A bare name that no scope declares is the one class of that name in its package, and two or more classes of the package "
       "share it: Card in billing, when store::Card and payment::Card are both designed. It relates nothing; the message names "
       "every candidate.",
       "Qualify the name with its module (store::Card), or rename one of the classes."},
      {Code::W_REVISION_DRIFT, "W-REVISION-DRIFT", Severity::warning,
       "A link names an older revision of an artefact than the one the workspace declares.", "Point the link at the current revision."},
      {Code::W_PARENT_IGNORED, "W-PARENT-IGNORED", Severity::warning,
       "A requirement carries a parent: field, which the language does not read.", "Nest it in its parent, or write @refines <ID>."},
      {Code::W_FIELD_UNKNOWN, "W-FIELD-UNKNOWN", Severity::warning,
       "A field that its kind does not declare in the metamodel: a typo, or a field the language does not know.",
       "ds manual <kind> lists the fields of each kind."},
      {Code::W_ANNOTATION_UNKNOWN, "W-ANNOTATION-UNKNOWN", Severity::warning,
       "An annotation word the language gives no meaning to: a typo, or a word no check reads. It is kept as a link.",
       "ds manual annotations lists the words that mean something."},
      {Code::W_DIAGRAM_UNDECLARED, "W-DIAGRAM-UNDECLARED", Severity::warning,
       "A diagram's include, exclude, open, close, cut, unfold or fold names something the workspace declares nowhere: it draws less and "
       "says nothing.",
       "Correct the pattern, or declare what it names."},
      {Code::W_RULE_NEVER_MATCHES, "W-RULE-NEVER-MATCHES", Severity::warning,
       "A relation literal binds an end to a kind the relation never joins there, so it matches nothing and the rule holds by vacuity.",
       "Name the members: m in c, m uses x. ds manual <kind> lists the relations of each kind."},
      {Code::W_REFINES_UPWARD, "W-REFINES-UPWARD", Severity::warning,
       "A requirement refines one written inside its own box: the detail stands above what it details.",
       "Turn the link round, or move one of them to the box it belongs to."},
      {Code::W_SATISFIES_OUTSIDE_HOME, "W-SATISFIES-OUTSIDE-HOME", Severity::warning,
       "A piece answers to a requirement written in a box that does not hold it: the requirement is written at one place and "
       "realised outside it. The finding names the box that holds the home and every piece that realises the requirement.",
       "Move the requirement to the box the finding names, or take the annotation off a piece that only passes the work on."},
      {Code::W_INTERFACE_SIGNATURE, "W-INTERFACE-SIGNATURE", Severity::warning,
       "A class declares an operation of an interface it realises with another signature than the interface's.",
       "Make the two take and give the same. What the class does not declare is no finding."},
      {Code::W_HANDLER_UNREACHED, "W-HANDLER-UNREACHED", Severity::warning,
       "A handler names a failure that no step of its attempt can fail with: it handles nothing, often a misspelt name.",
       "Correct the failure's name, or remove the handler."},
      {Code::W_FIELD_VALUE, "W-FIELD-VALUE", Severity::warning,
       "A closed field (status, method, a verification's level, a diagram's kind or level) carries a value it does not take.",
       "ds manual <kind> lists the values of each closed field."},
      {Code::W_MSG_UNDECLARED, "W-MSG-UNDECLARED", Severity::warning, "A message calls an operation its target does not declare.",
       "Declare the operation, or correct the name or the target."},
      {Code::W_MSG_NOT_STATIC, "W-MSG-NOT-STATIC", Severity::warning,
       "A message calls a class's operation on the class itself, and the operation is not static.",
       "Make the operation static, or call it through an instance: a local, an attribute or a parameter."},
      {Code::W_ARG_TYPE, "W-ARG-TYPE", Severity::warning,
       "An argument does not fit the class its parameter expects: a value of a class that does not derive from it, or a literal. "
       "A type the design does not declare (int, str, std::string) is never compared.",
       "Pass a value of the declared class."},
      {Code::W_RETURN_TYPE, "W-RETURN-TYPE", Severity::warning,
       "A sequence returns a value that does not fit the class its subject declares: a value of another class, or a literal.",
       "Return the declared type, or correct the declaration."},
      {Code::W_SCENARIO_UNDECLARED, "W-SCENARIO-UNDECLARED", Severity::warning,
       "A message or an exit names a scenario its target's behaviour does not have.",
       "Write that scenario (sequence X / name, or a branch / name), or correct the name."},
      {Code::W_SCENARIO_TWICE, "W-SCENARIO-TWICE", Severity::warning,
       "One behaviour names the same scenario on two branches: a scenario is one path.", "Give each path its own name."},
      {Code::W_LEVEL_NO_CROSSING, "W-LEVEL-NO-CROSSING", Severity::warning,
       "A scenario of a use case has one participant send to another, and the behaviour below it, all written for that sender, never "
       "crosses between the two.",
       "Make the two levels agree: correct the scenario, or the behaviour of the operation it is asked through."},
      {Code::W_LEVEL_ORDER, "W-LEVEL-ORDER", Severity::warning,
       "A scenario of a use case and the behaviour below it show the same crossings between its participants in another order.",
       "Make the two levels agree on the order."},
      {Code::W_LEVEL_NOT_BELOW, "W-LEVEL-NOT-BELOW", Severity::coverage,
       "A scenario of a use case has one participant send to another, and the behaviour below it does not show it yet: what the "
       "sender does is not all written.",
       "Write the behaviours the sender still lacks; an empty sequence says an operation calls nothing."},
      {Code::W_STATE_OVERLAP, "W-STATE-OVERLAP", Severity::warning,
       "Two states of a machine can hold at once: there is a context in which the rules of both hold, so the subject would be in two "
       "states.",
       "Say in one of the two rules what tells them apart: the rules of a machine exclude each other."},
      {Code::W_STATE_VALUE, "W-STATE-VALUE", Severity::warning, "The rule of a state asks an attribute for a value its type does not have.",
       "Write one of the values of the type, or add the value to it."},
      {Code::W_STATE_UNDECLARED, "W-STATE-UNDECLARED", Severity::warning,
       "The rule of a state compares something its subject does not hold.", "Name an attribute of the subject of the machine."},
      {Code::W_STATE_IMPOSSIBLE, "W-STATE-IMPOSSIBLE", Severity::warning,
       "The rule of a state asks two things that exclude each other: the subject can never be in it.", "Correct the rule."},
      {Code::W_STATE_ORDER, "W-STATE-ORDER", Severity::warning,
       "A behaviour calls an event of a machine when what it calls it on can be in no state from which the machine lets that event "
       "happen.",
       "Correct the order of the calls, or the jumps of the machine."},
      {Code::W_MESSAGE_NO_OPERATION, "W-MESSAGE-NO-OPERATION", Severity::coverage,
       "A scenario sends a message to a box that offers no operation of that name: the message is a label, and the level below is not "
       "written yet.",
       "Declare the operation in an interface of the box (a function, in a module) when that level is designed."},
      {Code::W_OPERATION_NO_BEHAVIOUR, "W-OPERATION-NO-BEHAVIOUR", Severity::coverage,
       "An interface operation has no sequence that says how it is handled.", "Write sequence <interface>::<operation> { ... }."},
      {Code::W_OUTCOME_MISMATCH, "W-OUTCOME-MISMATCH", Severity::warning,
       "An operation promises an exit code on a scenario whose trace ends in another.", "Make the exit and the scenario's return agree."},
      {Code::W_EVENT_UNDECLARED, "W-EVENT-UNDECLARED", Severity::warning,
       "A transition of a class's state machine fires on something that is not an operation of the class.",
       "Name an operation of the class, or declare it."},
      {Code::W_ACTION_UNDECLARED, "W-ACTION-UNDECLARED", Severity::warning,
       "A transition runs an action that is not an operation of the class.", "Name an operation of the class, or declare it."},
      {Code::W_VIEW_UNDECLARED, "W-VIEW-UNDECLARED", Severity::warning, "The navigation of an interface names a view it does not declare.",
       "Declare the view, or correct the name."},
      {Code::W_VIEW_UNREACHABLE, "W-VIEW-UNREACHABLE", Severity::warning, "Nothing leads to a view of an interface's navigation.",
       "Add a transition into it, or make it the initial one."},
      {Code::W_STATE_UNREACHABLE, "W-STATE-UNREACHABLE", Severity::warning,
       "Nothing leads to a state of a class's machine from its initial state.", "Add a transition into it, or remove the state."},
      {Code::W_STATE_LEAVES_FINAL, "W-STATE-LEAVES-FINAL", Severity::warning,
       "A transition leaves a state the machine says is final: a final state is one the machine ends in.",
       "Remove the transition, or take the state out of final."},
      {Code::W_FAILURE_UNDECLARED, "W-FAILURE-UNDECLARED", Severity::warning,
       "An operation lets a failure escape that its signature does not say it may fail with; an interface operation is a "
       "boundary and must say it. A use case has no signature: a failure that escapes its scenario is said there.",
       "Handle it in an attempt, or add it to the signature: fails <Failure>."},
      {Code::W_FAILURE_UNREACHED, "W-FAILURE-UNREACHED", Severity::warning,
       "An operation says it may fail with something no path of its behaviour fails with.",
       "Remove it from the signature, or write the path that fails with it."},
      {Code::W_VIEW_NO_WAY_OUT, "W-VIEW-NO-WAY-OUT", Severity::warning, "Nothing leaves a view of an interface's navigation.",
       "Add a transition out of it."},
      {Code::W_NOT_VISIBLE, "W-NOT-VISIBLE", Severity::warning, "Something reaches a private member or box from outside it.",
       "Go through a public member, or make it public."},
      {Code::W_PRIVATE_LEAK, "W-PRIVATE-LEAK", Severity::warning, "Something public shows a private type in its signature.",
       "Make the type public, or hide the member."},
      {Code::W_DIAGRAM_DEPTH, "W-DIAGRAM-DEPTH", Severity::warning, "A diagram asks for a depth that is neither a number nor *.",
       "Write a number, or * for every level."},
      {Code::W_DIAGRAM_NEIGHBOURS, "W-DIAGRAM-NEIGHBOURS", Severity::warning, "A diagram asks for neighbours that are not a number.",
       "Write a number: how many steps of neighbours it adds."},
      {Code::W_REQ_NO_VERIFICATION, "W-REQ-NO-VERIFICATION", Severity::coverage, "An approved requirement that nothing verifies.",
       "Nest a verification in it, or link one."},
      {Code::W_VER_NO_TEST, "W-VER-NO-TEST", Severity::coverage, "A verification by test names no tests.",
       "List them under tests: as <file>::<Fixture>.<Test>."},
      {Code::W_RULE_NO_REQUIREMENT, "W-RULE-NO-REQUIREMENT", Severity::coverage,
       "A rule answers to no requirement: nothing says what it makes real.", "Link it: @requirement <ID> in its body."},
      {Code::W_RULE_NO_CLAIM, "W-RULE-NO-CLAIM", Severity::warning,
       "A rule says neither what holds nor what must never happen, or says both.",
       "Write one of holds: <condition> or never: <condition>."},
      {Code::W_DEFINITION_NO_CASE, "W-DEFINITION-NO-CASE", Severity::warning,
       "A definition says no case in which it holds: nothing ever belongs to it.", "Write if <literals>."},
      {Code::E_RULE_BROKEN, "E-RULE-BROKEN", Severity::error,
       "The model breaks a rule: a binding of its ranges with no answer to holds, or with one to never.",
       "Change the design so the rule holds, or the rule if it says what nobody meant."},
      {Code::E_RULE_UNKNOWN, "E-RULE-UNKNOWN", Severity::error,
       "A rule or a definition names a kind, a relation, an entity or a definition that the language or the model does not declare.",
       "Write a kind or a relation of the metamodel, a qualified entity that exists, a definition in scope."},
      {Code::E_RULE_ARITY, "E-RULE-ARITY", Severity::error, "A definition is applied to a number of arguments it does not take.",
       "Pass one per parameter."},
      {Code::E_RULE_UNSAFE, "E-RULE-UNSAFE", Severity::error,
       "A variable is tested by a negation or a comparison, or is a parameter of a definition, and no positive literal binds it.",
       "Name it in a relation, a kind, a containment or a definition first."},
      {Code::E_RULE_UNSTRATIFIED, "E-RULE-UNSTRATIFIED", Severity::error,
       "A definition negates itself through recursion: no order of evaluation decides it.",
       "Break the cycle: negate only definitions that do not rest on the one being defined."},
      {Code::W_CONSUMES_UNDECLARED, "W-CONSUMES-UNDECLARED", Severity::warning,
       "A behaviour calls an operation of an interface another box offers, and neither it nor any box it lives in declares @consumes of "
       "it.",
       "Declare @consumes <interface> on the caller, its module or its package (or in a wiring file), or call something you depend on."},
      {Code::W_CONSUMES_UNUSED, "W-CONSUMES-UNUSED", Severity::warning,
       "A box declares @consumes of an interface and none of its behaviours calls it.",
       "Remove the @consumes, or write the behaviour that calls the interface."},
      {Code::W_CONSUMES_BYPASS, "W-CONSUMES-BYPASS", Severity::warning,
       "A behaviour calls a method of a class that realises an interface, from outside its module, instead of the interface.",
       "Type the local or the parameter by the interface and call its operation, and declare @consumes of it."},
      {Code::W_LANGUAGE_UNKNOWN, "W-LANGUAGE-UNKNOWN", Severity::warning,
       "A package or a module says a language ds does not know, or two at once (two declarations of a box it reopens).",
       "Write one of cpp, c, python, typescript or lua, once."},
      {Code::W_USECASE_UNDECLARED, "W-USECASE-UNDECLARED", Severity::warning,
       "An operation realises, or a use case specialises, an id that is no use case of the workspace.",
       "Declare the use case, or correct the id after the colon."},
      {Code::W_USECASE_SIGNATURE, "W-USECASE-SIGNATURE", Severity::warning,
       "An operation that realises a use case, or a use case that specialises another, takes or gives something else than the contract.",
       "Make the parameters and the return agree with the use case."},
      {Code::W_USECASE_OUTCOME_UNHANDLED, "W-USECASE-OUTCOME-UNHANDLED", Severity::coverage,
       "An operation realises a use case but has no scenario for one of its outcomes.",
       "Write the scenario (sequence <interface>::<operation> / <outcome>, or a branch named so) and an exit for it."},
      {Code::W_SCENARIO_NO_OUTCOME, "W-SCENARIO-NO-OUTCOME", Severity::warning,
       "An operation that realises a use case handles a scenario none of its outcomes names.",
       "Add the outcome to the use case, or rename the scenario to the outcome it is."},
      {Code::W_USECASE_UNREALISED, "W-USECASE-UNREALISED", Severity::coverage,
       "No interface operation realises a use case, nor any use case that specialises it.",
       "Realise it from an operation (operation <name>(...) : <UC ID>), or drop it."},
      {Code::E_RULE_PATTERN, "E-RULE-PATTERN", Severity::error,
       "A rule, a definition or a table matches a value against a pattern that is no string, or that does not compile as a regular "
       "expression.",
       "Write the pattern as a string of ECMAScript regular expression syntax: name(c) matches \"^I[A-Z]\"."},
      {Code::E_RULE_RESERVED, "E-RULE-RESERVED", Severity::error,
       "A definition takes the name of what an entity is, name, id or kind, which a rule reads as name(x), id(x) and kind(x).",
       "Name the definition otherwise."},
      {Code::W_REQ_UNREALISED, "W-REQ-UNREALISED", Severity::coverage,
       "An approved requirement that nothing satisfies: no class, function, interface or box names it with @requirement, "
       "and not every requirement that refines it is satisfied.",
       "Name it with @requirement in what realises it, or refine it into requirements that are."},
      {Code::W_CST_NO_VERIFICATION, "W-CST-NO-VERIFICATION", Severity::coverage, "An approved constraint that nothing verifies.",
       "Name it with @constraint in a verification that shows it is kept."},
  };
  // a finding is made by its slot: a row out of the order of Code, or a Code with no row, would give another code
  for (std::size_t i = 0; i < out.size(); ++i)
    if (static_cast<std::size_t>(out[i].id) != i) throw std::logic_error("the catalogue is out of the order of Code at " + out[i].code);
  if (out.size() != static_cast<std::size_t>(Code::W_CST_NO_VERIFICATION) + 1)
    throw std::logic_error("the catalogue does not hold one row per Code");
  return out;
}

const CodeSpec* Metamodel::codeOf(const std::string& code) const {
  for (const auto& c : codes_)
    if (c.code == code) return &c;
  return nullptr;
}

}  // namespace model
}  // namespace ds
