#ifndef DRIFTSPEC_RENDER_IR_HPP
#define DRIFTSPEC_RENDER_IR_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ds {
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
  std::map<std::string, std::string> refs;  ///< named references (emitted as data-ref-<key>); not `refs`: a modifier keyword in DriftSpec
  std::vector<std::vector<std::string>> sections;
  std::vector<std::string> sectionKinds;
  std::pair<int, int> span{-1, -1};                   ///< frame: first and last sequence row
  std::vector<std::pair<int, std::string>> dividers;  ///< frame: else sections
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
  int order = -1;                           ///< sequence row of a message
  std::map<std::string, std::string> refs;  ///< named references: expands, span, location
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

/// A render-IR document as its contract writes it (JSON): what ds emits, plantuml-render reads, and
/// another tool may hand ds to draw. The document the formats of a diagram write (DS-126).
struct IrText {
  std::string json;
};

/// The letter a node's badge carries, the same in every drawing (DS-118): by its classifier, A for an
/// abstract class with none, the classifier's first letter for one the table does not name.
std::string badgeOf(const std::string& classifier, bool abstract);

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IR_HPP
