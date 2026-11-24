#ifndef DRIFTSPEC_QUERY_CLASSBOXES_HPP
#define DRIFTSPEC_QUERY_CLASSBOXES_HPP

#include <set>
#include <string>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// What a class diagram draws, read of the model (DS-423): which entities are boxes and which hold others, the relations
/// it draws as edges, the box a relation is drawn from and the boxes drawn around what is taken. The class filter and
/// the exporter both read it; neither reaches the other for it.
class ClassBoxes {
 public:
  /// A class, an enum, an interface or a function of a module.
  static bool isBox(const model::Entity& entity);
  /// A module, or a package below the system: the outermost package is the drawing.
  static bool isContainer(const model::Entity& entity);
  /// A box, or a box that holds others.
  static bool drawable(const model::Entity& entity);
  /// The relations drawn as edges, the ones the neighbours follow.
  static bool drawnAsEdge(const std::string& relation);
  /// The box a class diagram draws a relation of `entity` from (DS-219): a box, a module or a package is itself; a
  /// member is its class; a behaviour is its subject's box (a method's class, a function, an operation's interface),
  /// not the module it is written in; anything else is its scope.
  static std::string drawnFrom(const model::Model& model, const model::Entity& entity);
  /// The modules and nested packages that hold what is taken: drawn around it.
  static std::set<std::string> containers(const model::Model& model, const std::set<std::string>& taken);
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_CLASSBOXES_HPP
