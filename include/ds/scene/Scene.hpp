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

/// A behaviour replayed over the scene: a sequence and every sequence it expands to, in order,
/// along one scenario.
struct Tour {
  std::string id;        ///< the subject's node, and / scenario for a named one
  std::string subject;   ///< the node whose behaviour it is
  std::string scenario;  ///< empty for the default path
  std::string group;     ///< interface or method
  std::vector<Step> steps;
};

/// The whole model laid out once, nested, in two variants: every member, public members only.
struct SceneDocument {
  std::vector<Node> nodes;
  Box world;
  Box leanWorld;
  std::vector<Link> links;
  std::vector<DiagramView> diagrams;
  std::map<std::string, std::string> titles;  ///< the title of every requirement and constraint a node links
  std::vector<Tour> tours;
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

 private:
  void relate(SceneDocument& out_document) const;
  void tour(SceneDocument& out_document) const;  // every public behaviour, expanded through what it calls  // the links between nodes, the
                                                 // diagrams that show them, the titles of requirements

  const model::Model& model_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_SCENE_HPP
