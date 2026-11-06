#ifndef DRIFTSPEC_SCENE_SCENE_HPP
#define DRIFTSPEC_SCENE_SCENE_HPP

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

/// The whole model laid out once, nested, in two variants: every member, public members only.
struct SceneDocument {
  std::vector<Node> nodes;
  Box world;
  Box leanWorld;
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
  std::string page(const std::string& title, const std::string& css = "") const;
  /// A page as an iframe that carries it whole (srcdoc), to paste into any HTML document.
  static std::string frame(const std::string& page, const std::string& title, int height);

 private:
  const model::Model& model_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_SCENE_HPP
