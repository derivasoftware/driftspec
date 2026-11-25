#ifndef DRIFTSPEC_RENDER_TABLEPROJECTION_HPP
#define DRIFTSPEC_RENDER_TABLEPROJECTION_HPP

#include <string>
#include <vector>

#include "ds/layout/TableGraph.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A table of the render-IR as the layout's table graph (DS-464): each text as the sink writes it, measured word by word
/// in characters (DS-466). A sink that marks a cell in its text (a terminal writes "! " before a cell against the
/// machine and "·" for an empty one) is measured with its marks.
class TableProjection {
 public:
  TableProjection(const IrDocument& doc, bool marked);

  const layout::TableGraph& graph() const { return graph_; }
  /// The words of each text, as written, in the graph's reading order: the corner, the headings, then each row's
  /// label and every cell of it.
  const std::vector<std::vector<std::string>>& words() const { return words_; }

 private:
  layout::TableText measured(const std::string& text);

  layout::TableGraph graph_;
  std::vector<std::vector<std::string>> words_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TABLEPROJECTION_HPP
