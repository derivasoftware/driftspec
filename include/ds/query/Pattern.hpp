#ifndef DRIFTSPEC_QUERY_PATTERN_HPP
#define DRIFTSPEC_QUERY_PATTERN_HPP

#include <string>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// One pattern of a diagram's include or exclude, read once (DS-212, DS-385): one reading for every kind of diagram.
/// It is an identity; a prefix with * over the identity or the scope (`p@m::*` is the module p@m and everything in
/// it); a module named as such; or a template written without its arity. What it is written as is decided when it is
/// built, so taking an entity compares the entity's identity and scope with what was read, and builds no text.
class Pattern {
 public:
  /// `text` read against `model`, which says whether it names a module.
  Pattern(const model::Model& model, std::string text);
  /// Whether `entity` falls under the pattern.
  bool takes(const model::Entity& entity) const;

 private:
  std::string text_;     // as written: the identity it names, a module, or a template without its arity
  bool prefix_ = false;  // written with a * at the end
  bool module_ = false;  // names a module the model declares
  std::string start_;    // a prefix: what comes before the *; a module: the module and the :: after it
  std::string whole_;    // a prefix ending in ::, the module before it, whose own artefacts it takes; else empty
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_PATTERN_HPP
