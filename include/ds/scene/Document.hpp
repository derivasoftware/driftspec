#ifndef DRIFTSPEC_SCENE_DOCUMENT_HPP
#define DRIFTSPEC_SCENE_DOCUMENT_HPP

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/layout/Box.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace scene {

// The scene's document (DS-126): the data the layout, the map, the layers and the formats share.

/// One thing drawn in the scene: a package, a module, an interface, a unit (class,
/// function, enum) or a row (a member, an operation, a function's signature).
struct Node {
  std::string id;          ///< the model identity; a function's signature row is the function's id + "#sig"
  std::string kind;        ///< package, module, interface, unit or row
  std::string parent;      ///< the node it is drawn inside; empty at the top
  std::string label;       ///< what is written in it
  std::string stereotype;  ///< the model kind it stands for: class, dataclass, function, enum, method, attribute, operation...
  std::string visibility;  ///< + or -
  std::vector<std::string> requirements;  ///< the requirements and constraints it is linked to
  layout::Box full;                       ///< its place with every member drawn
  bool lean = false;                      ///< whether it keeps a place when private members are hidden
  layout::Box leanBox;                    ///< its place with private members hidden
};

/// A relation between two nodes of the scene, of one kind, with what produces it: the
/// members that call (calls), the attribute that owns or refers (owns, shares, refs), the
/// sequences where a call is written. A viewer lifts it to the boxes it can see.
struct Link {
  std::string from;                    ///< a node: the member row or the unit the relation starts at
  std::string to;                      ///< a node: the member row, unit or interface it reaches
  std::string kind;                    ///< inherits, realises, owns, shares, refs, uses or calls
  int count = 0;                       ///< how many times the model states it
  std::string multiplicity;            ///< owns, shares, refs: 1, 0..1 or *
  std::vector<std::string> sequences;  ///< calls: the sequences that write the call
};

/// A diagram of the workspace and the nodes it shows, so a viewer can say where a box or a relation is drawn.
struct DiagramView {
  std::string name;
  std::string kind;
  std::vector<std::string> nodes;
};

/// One step of a walkthrough as the scene places it (DS-471): what it shows, by the name diagrams gives, and what it
/// lights, landed on the nodes drawn.
struct WalkStepView {
  std::string title;
  std::string diagram;                 ///< the shown diagram's name, as DiagramView names it
  std::string behaviour;               ///< the behaviour a sequence step replays, X::sequence[/scenario]; "" for none
  std::vector<std::string> focus;      ///< the nodes it focuses, landed
  std::vector<std::string> highlight;  ///< the nodes it highlights, landed
  std::string at;                      ///< the node its callout points at; "" for none
  std::string text;
};

/// A walkthrough as the scene carries it (DS-471): from the one record model::Walkthroughs builds.
struct WalkView {
  std::string name;
  std::string title;
  std::vector<WalkStepView> steps;
};

/// One call of a behaviour, as written: whom it calls and which behaviour it goes on into.
struct Call {
  std::string to;      ///< the node it calls
  std::string label;   ///< the call as written, op(args)
  int expands = -1;    ///< the behaviour of the callee it goes on into, or -1
  std::string result;  ///< what comes back, when the sequence names it
};

/// A sequence walked along one scenario, its calls in order: written once, however many tours
/// pass through it. A tour is a behaviour expanded through the behaviours its calls go on into.
struct Behaviour {
  std::string id;        ///< the subject's node, and / scenario for a named one
  std::string subject;   ///< the node whose behaviour it is
  std::string scenario;  ///< empty for the default path
  std::vector<Call> calls;
};

/// A behaviour a viewer offers to replay: an interface operation or a public method or function.
struct Tour {
  int behaviour = -1;
  std::string group;  ///< interface or method
};

/// One way a use case may end: default when it goes well, a named scenario when it does not.
struct Outcome {
  std::string name;
  std::string returns;  ///< what it gives back when not what the use case returns
  std::string text;
};

/// A requirement, a constraint or a use case as a reader of the viewer lists it: what it says, the
/// one it refines (or the use case it specialises), where it is written and the nodes that answer
/// to it (for a use case, the interface operations that realise it).
struct Requirement {
  std::string id;
  std::string kind;  ///< requirement, constraint or usecase
  std::string title;
  std::string status;  ///< draft, approved, obsolete, or empty when not written
  std::string text;
  std::string parent;              ///< the requirement it refines; empty at the top
  std::string scope;               ///< the node it is written in; empty at the top of the workspace
  std::vector<std::string> nodes;  ///< the nodes whose entities answer to it
  std::string signature;           ///< a use case: its contract, name(params) -> returns
  std::vector<Outcome> outcomes;   ///< a use case: its own outcomes
  std::vector<std::string> binds;  ///< a use case: the requirements it is bound by
};

/// A verification and what it proves: the requirements and constraints, the method, the tests.
struct Verification {
  std::string id;
  std::string title;
  std::string status;
  std::string method;  ///< test, review, analysis or inspection
  std::vector<std::string> tests;
  std::vector<std::string> requirements;
};

/// One binding that breaks a rule, placed: the node it lands on and, for a never, the node of the
/// answer that breaks it (DS-108).
struct BreachView {
  std::string node;  ///< the node of what breaks it, the first thing the rule ranges over
  std::string to;    ///< the node of the answer that breaks a never; empty when none is drawn
  std::vector<std::pair<std::string, std::string>> bindings;
  std::vector<std::pair<std::string, std::string>> witness;
  std::string file;
  int line = 0;
};

/// A rule or a definition as the logic perspective shows it: where it is written, what it says, what it
/// answers to and what breaks it.
struct RuleView {
  std::string id;
  std::string kind;  ///< rule or definition
  std::string name;
  std::string title;
  std::string scope;                                       ///< the node it is written in
  std::vector<std::pair<std::string, std::string>> lines;  ///< each clause: its keyword and its text, as written
  std::vector<std::string> requirements;
  std::vector<BreachView> breaches;
  std::string file;
  int line = 0;
};

/// The whole model laid out once, nested, in two variants: every member, public members only.
struct SceneDocument {
  std::vector<std::pair<std::string, std::string>> categories;  ///< each kind's category, for the viewer to group by (DS-427)
  std::vector<Node> nodes;
  layout::Box world;
  layout::Box leanWorld;
  std::vector<Link> links;
  std::vector<DiagramView> diagrams;
  std::map<std::string, std::string> titles;  ///< the title of every requirement and constraint a node links
  std::vector<Behaviour> behaviours;
  std::vector<Tour> tours;
  std::vector<Requirement> requirements;  ///< every requirement and constraint, by id
  std::vector<Verification> verifications;
  std::vector<RuleView> rules;         ///< every rule and definition, with what breaks each
  std::vector<WalkView> walkthroughs;  ///< every walkthrough, its steps in order (DS-471)
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_DOCUMENT_HPP
