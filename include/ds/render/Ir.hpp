#ifndef DRIFTSPEC_RENDER_IR_HPP
#define DRIFTSPEC_RENDER_IR_HPP

#include <map>
#include <optional>
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
  std::string part;                         ///< the part of the drawing it belongs to, in a drawing made of several (DS-245)
  std::optional<bool> abstract;             ///< said of a class or an interface, either way; of anything else not said (DS-418)
  std::map<std::string, std::string> refs;  ///< named references (emitted as data-ref-<key>); not `refs`: a modifier keyword in DriftSpec
  std::vector<std::vector<std::string>> sections;
  std::vector<std::string> sectionKinds;
  std::pair<int, int> span{-1, -1};                   ///< frame: first and last sequence row
  std::vector<std::pair<int, std::string>> dividers;  ///< frame: else sections

  /// Whether a section of a box holds members, written as signatures: attributes or methods. Any other kind of
  /// section holds the names of what a closed box offers, all of that kind (DS-244): "class", "interface", ...
  static bool holdsMembers(const std::string& sectionKind);

  /// The letter a node's badge carries, the same in every drawing (DS-118): by its classifier, A for an
  /// abstract class with none, the classifier's first letter for one the table does not name.
  static std::string badgeOf(const std::string& classifier, bool abstract);
};

/// One relation of a render-IR document.
struct IrEdge {
  std::string from;
  std::string to;
  std::string kind;  ///< inheritance, realization, composition, aggregation, dependency, association, attachment
  std::string label;
  std::string title;
  std::string href;
  std::string part;  ///< the part of the drawing it belongs to, in a drawing made of several (DS-245)
  bool dashed = false;
  int order = -1;                           ///< sequence row of a message
  std::map<std::string, std::string> refs;  ///< named references: expands, span, location

  /// An edge from `from` to `to` of `kind`, saying nothing else yet (DS-418).
  static IrEdge between(const std::string& from, const std::string& to, const std::string& kind);
};

/// One cell of a table: its text, what it is (the renderers draw each kind its own way) and what it says
/// when pointed at.
struct IrCell {
  std::string text;
  std::string kind;
  std::string title;
};

/// One row of a table: its heading and a cell under each column.
struct IrRow {
  std::string label;
  std::string title;
  std::vector<IrCell> cells;
};

/// A drawing that is a table (DS-251): a corner, the headings of its columns, and its rows.
struct IrTable {
  std::string corner;
  std::vector<std::string> columns;
  std::vector<IrRow> rows;
};

/// A render-IR document, read from JSON; what `ds render` emits and plantuml-render consumes.
struct IrDocument {
  std::string kind;  ///< the diagram's kind, as it is written (class, state, sequence, ...): how it is drawn (DS-443)
  std::string title;
  std::string notice;
  std::optional<IrTable> table;  ///< when written and with columns, the document is drawn as this table
  std::vector<IrNode> nodes;
  std::vector<IrEdge> edges;
  /// A drawing made of several, each drawn as itself and laid out beside the last (DS-245): a tour holds the
  /// boxes a behaviour crosses and its sequence. The parts are named in the order they are drawn, and each
  /// node and edge says which one it belongs to.
  std::vector<std::string> parts;

  static IrDocument parse(const std::string& json);

  /// One part of a drawing made of several, as a document of its own: the nodes and edges that belong to it.
  IrDocument part(const std::string& name) const;

  /// Relations that differ only in their text, drawn as one: same ends, kind,
  /// dash and link; the labels are joined one per line (as plantuml-render does).
  IrDocument bundled() const;
};

/// A render-IR document as its contract writes it (JSON): what ds emits, plantuml-render reads, and
/// another tool may hand ds to draw. The document the formats of a diagram write (DS-126).
struct IrText {
  std::string json;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IR_HPP
