#ifndef DRIFTSPEC_SCENE_SCENE_HPP
#define DRIFTSPEC_SCENE_SCENE_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace scene {

/// A rectangle in the coordinates of the scene: one unit is a pixel at zoom 1.
struct Box {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

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
  Box full;                               ///< its place with every member drawn
  bool lean = false;                      ///< whether it keeps a place when private members are hidden
  Box leanBox;                            ///< its place with private members hidden
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

/// One step of a tour: a call from one node to another, or its answer coming back.
struct Step {
  std::string from;
  std::string to;
  std::string label;  ///< the call as written, op(args), or what comes back
  int depth = 1;      ///< 1 for the subject's own messages, one more for each sequence expanded
  bool back = false;  ///< the answer of the call above it
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

/// A requirement or a constraint as a reader of the viewer lists it: what it says, the one it
/// refines, where it is written and the nodes that answer to it.
struct Requirement {
  std::string id;
  std::string kind;  ///< requirement or constraint
  std::string title;
  std::string status;  ///< draft, approved, obsolete, or empty when not written
  std::string text;
  std::string parent;              ///< the requirement it refines; empty at the top
  std::string scope;               ///< the node it is written in; empty at the top of the workspace
  std::vector<std::string> nodes;  ///< the nodes whose entities answer to it
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

/// The whole model laid out once, nested, in two variants: every member, public members only.
struct SceneDocument {
  std::vector<Node> nodes;
  Box world;
  Box leanWorld;
  std::vector<Link> links;
  std::vector<DiagramView> diagrams;
  std::map<std::string, std::string> titles;  ///< the title of every requirement and constraint a node links
  std::vector<Behaviour> behaviours;
  std::vector<Tour> tours;
  std::vector<Requirement> requirements;  ///< every requirement and constraint, by id
  std::vector<Verification> verifications;
};

/// Builds the structural scene of a model: every box placed inside its parent, once, so a
/// viewer can zoom from the system down to a member without anything moving.
class Scene {
 public:
  explicit Scene(const model::Model& model);
  SceneDocument build() const;
  std::string render(bool pretty = true) const;
  /// The viewer as one self-contained HTML page with this scene inside, titled after the workspace;
  /// css, when given, is the reader's stylesheet, applied after the viewer's.
  /// focus, when given, are the identities the page opens framed on (what a diagram shows).
  std::string page(const std::string& title, const std::string& css = "", const std::vector<std::string>& focus = {}) const;
  /// A page as an iframe that carries it whole (srcdoc), to paste into any HTML document.
  static std::string frame(const std::string& page, const std::string& title, int height);
  /// The steps of a behaviour expanded through the behaviours its calls go on into, down to a
  /// depth: each call, the steps of what it calls, then its answer. A cycle is not entered twice.
  static std::vector<Step> expand(const SceneDocument& document, int behaviour, int depth);
  /// Text deflated (zlib format) and written in base64: how the scene travels inside a page, which a
  /// browser inflates with its own DecompressionStream.
  static std::string pack(const std::string& text);
  /// The text a pack holds; empty when it holds none.
  static std::string unpack(const std::string& packed);

 private:
  void relate(SceneDocument& out_document) const;
  void tour(SceneDocument& out_document) const;
  void trace(SceneDocument& out_document)
      const;  // the requirements, what they refine and what answers to and proves them  // every public behaviour, expanded through what it
              // calls  // the links between nodes, the diagrams that show them, the titles of requirements

  const model::Model& model_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_SCENE_HPP
