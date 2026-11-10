#ifndef DRIFTSPEC_SCENE_SHARED_HPP
#define DRIFTSPEC_SCENE_SHARED_HPP

#include <string>

#include "ds/model/Model.hpp"

namespace ds {
namespace scene {

/// A field of an entity, or "" when it has none.
inline std::string fieldOf(const model::Entity& e, const std::string& key) {
  const auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_SHARED_HPP
