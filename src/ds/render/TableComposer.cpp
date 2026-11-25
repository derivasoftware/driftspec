#include "ds/render/TableComposer.hpp"

#include <utility>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/TablePlacement.hpp"
#include "ds/layout/TableSpacing.hpp"
#include "ds/render/TableLayout.hpp"
#include "ds/render/TableProjection.hpp"

namespace ds {
namespace render {

// The table projected into the layout's graph and placed by its table engine with the sink's measures (DS-464); each
// text is read back in the lines the engine broke it into.
Figure TableComposer::compose(const IrDocument& input, int) const {
  Figure figure;
  figure.source = &input;  // drawn as it is
  const TableProjection projection(input, metrics_.TABLE_MARKS != 0);
  const layout::TablePlacement placed =
      layout::EngineFactory::table(layout::TableSpacing::of(metrics_.CHAR_W, metrics_.LINE_H))->place(projection.graph());
  std::size_t next = 0;  // the text being read back, in the graph's reading order
  const auto lines = [&]() {
    const std::vector<std::string>& words = projection.words()[next];
    const std::vector<std::size_t>& breaks = placed.breaks[next];
    ++next;
    std::vector<std::string> out{""};
    std::size_t b = 0;
    for (std::size_t w = 0; w < words.size(); ++w) {
      if (b < breaks.size() && breaks[b] == w) {
        out.push_back("");
        ++b;
      }
      out.back() += (out.back().empty() ? "" : " ") + words[w];
    }
    return out;
  };
  TableLayout t;
  t.columnX = placed.columnX;
  t.columnWidth = placed.columnWidth;
  t.headHeight = placed.headHeight;
  t.rowY = placed.rowY;
  t.rowHeight = placed.rowHeight;
  t.width = placed.width;
  t.height = placed.height;
  const layout::TableSpacing spacing = layout::TableSpacing::of(metrics_.CHAR_W, metrics_.LINE_H);
  t.padX = spacing.padX;
  t.lineHeight = spacing.lineHeight;
  t.corner = lines();
  for (std::size_t c = 0; c < projection.graph().columns.size(); ++c) t.headings.push_back(lines());
  for (const layout::TableRow& row : projection.graph().rows) {
    t.labels.push_back(lines());
    std::vector<std::vector<std::string>> cells;
    for (std::size_t c = 0; c < row.cells.size(); ++c) cells.push_back(lines());
    t.cells.push_back(std::move(cells));
  }
  figure.table = std::move(t);
  return figure;
}

}  // namespace render
}  // namespace ds
