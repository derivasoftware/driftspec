#include "ds/scene/Map.hpp"

#include "ds/model/Identity.hpp"

namespace ds {
namespace scene {

Map::Map(const std::vector<Node>& nodes) {
  for (const auto& n : nodes) {
    ids_.insert(n.id);
    parent_[n.id] = n.parent;
  }
}

// the node that stands for a model identity: itself, a function's signature row, or the nearest
// enclosing node; a message stands for the member whose behaviour it is written in
std::string Map::land(std::string id) const {
  if (const std::string subject = model::Identity(id).subject().text(); !subject.empty()) id = subject;  // a behaviour: its subject
  for (;;) {
    if (ids_.count(id)) return id;
    const auto at = id.rfind("::");
    if (at == std::string::npos) return "";
    id = id.substr(0, at);
  }
}

bool Map::has(const std::string& id) const { return ids_.count(id) > 0; }

std::string Map::parentOf(const std::string& id) const {
  const auto it = parent_.find(id);
  return it == parent_.end() ? "" : it->second;
}

}  // namespace scene
}  // namespace ds
