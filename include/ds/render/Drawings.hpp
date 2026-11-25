#ifndef DRIFTSPEC_RENDER_DRAWINGS_HPP
#define DRIFTSPEC_RENDER_DRAWINGS_HPP

#include <map>
#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace render {

/// A diagram drawn in svg (DS-470): the one path from a diagram to its drawing, that `ds render <ws> <D> -f svg` and
/// a walkthrough's cards both take, so a card is exactly what render draws. It is render's (DS-474): the diagram's IR
/// exported from the model and drawn by Svg, below every door, so see-design draws through it.
class Drawings {
 public:
  /// One diagram, by name, drawn in svg and wrapped to `width` pixels (0: none); "" when there is no such diagram.
  static std::string svg(const model::Model& model, const model::Metamodel& metamodel, const theme::Theme& theme,
                         const std::string& diagram, int width);
  /// Every diagram of the workspace but a walkthrough, by name, each drawn as svg draws it alone.
  static std::map<std::string, std::string> of(const model::Model& model, const model::Metamodel& metamodel, const theme::Theme& theme);
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_DRAWINGS_HPP
