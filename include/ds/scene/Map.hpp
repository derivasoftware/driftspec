#ifndef DRIFTSPEC_SCENE_MAP_HPP
#define DRIFTSPEC_SCENE_MAP_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/scene/Document.hpp"

namespace ds {
namespace scene {

/// The boxes placed and where each identity of the model lands among them (DS-126): built once from
/// what the layout placed, read by every layer.
class Map {
 public:
  explicit Map(const std::vector<Node>& nodes);
  /// The node that stands for a model identity: itself, a function's signature row, or the nearest
  /// enclosing node; a behaviour stands for its subject. "" when nothing on the map does.
  std::string land(std::string id) const;
  bool has(const std::string& id) const;
  /// The node a node is drawn inside; "" at the top.
  std::string parentOf(const std::string& id) const;

 private:
  std::set<std::string> ids_;
  std::map<std::string, std::string> parent_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_MAP_HPP
