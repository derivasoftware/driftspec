#ifndef DRIFTSPEC_RENDER_IR_HPP
#define DRIFTSPEC_RENDER_IR_HPP

#include <string>
#include <vector>

namespace driftspec {
namespace render {

/// One node of a render-IR document (plantuml-render contract v1): what this engine draws.
struct IrNode {
  std::string id;
  std::string kind;  ///< box, container, note (the class-diagram vocabulary)
  std::string label;
  std::string classifier;
  std::string stereotype;
  std::string parent;
  std::string title;
  std::string href;
  bool abstract = false;
  std::vector<std::vector<std::string>> sections;
  std::vector<std::string> sectionKinds;
};

/// One relation of a render-IR document.
struct IrEdge {
  std::string from;
  std::string to;
  std::string kind;  ///< inheritance, realization, composition, aggregation, dependency, association, attachment
  std::string label;
  std::string title;
  std::string href;
  bool dashed = false;
};

/// A render-IR document, read from JSON; what `ds render` emits and plantuml-render consumes.
struct IrDocument {
  std::string title;
  std::string notice;
  std::vector<IrNode> nodes;
  std::vector<IrEdge> edges;

  static IrDocument parse(const std::string& json);

  /// Relations that differ only in their text, drawn as one: same ends, kind,
  /// dash and link; the labels are joined one per line (as plantuml-render does).
  IrDocument bundled() const;
};

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_IR_HPP
