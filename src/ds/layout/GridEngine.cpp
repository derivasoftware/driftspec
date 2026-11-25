#include "ds/layout/GridEngine.hpp"

#include <algorithm>

namespace ds {
namespace layout {

std::vector<std::size_t> GridEngine::breaks(const TableText& text) const {
  std::vector<std::size_t> out;
  std::size_t line = 0;  // the characters of the line being filled
  for (std::size_t w = 0; w < text.words.size(); ++w) {
    if (line > 0 && line + 1 + text.words[w] > spacing_.wrap) {
      out.push_back(w);
      line = 0;
    }
    line += (line > 0 ? 1 : 0) + text.words[w];
  }
  return out;
}

std::size_t GridEngine::widest(const TableText& text, const std::vector<std::size_t>& breaks) const {
  std::size_t most = 0, line = 0, next = 0;
  for (std::size_t w = 0; w < text.words.size(); ++w) {
    if (next < breaks.size() && breaks[next] == w) {
      most = std::max(most, line);
      line = 0;
      ++next;
    }
    line += (line > 0 ? 1 : 0) + text.words[w];
  }
  return std::max(most, line);
}

TablePlacement GridEngine::place(const TableGraph& graph) const {
  const TableSpacing& S = spacing_;
  TablePlacement out;
  const auto broken = [&](const TableText& text) {
    out.breaks.push_back(breaks(text));
    return out.breaks.back();
  };
  // the corner, the headings, then each row's label and cells, in reading order
  std::vector<std::size_t> widths{widest(graph.corner, broken(graph.corner))};
  for (const TableText& heading : graph.columns) widths.push_back(widest(heading, broken(heading)));
  std::vector<std::size_t> lines;
  for (const TableRow& row : graph.rows) {
    widths[0] = std::max(widths[0], widest(row.label, broken(row.label)));
    std::size_t most = out.breaks.back().size() + 1;
    for (std::size_t c = 0; c < row.cells.size(); ++c) {  // every cell counts for the row's height, those under a heading for its width
      const std::vector<std::size_t> b = broken(row.cells[c]);
      if (c < graph.columns.size()) widths[c + 1] = std::max(widths[c + 1], widest(row.cells[c], b));
      most = std::max(most, b.size() + 1);
    }
    lines.push_back(most);
  }
  int x = 0;
  for (const std::size_t w : widths) {
    out.columnX.push_back(x);
    out.columnWidth.push_back(static_cast<int>(w) * S.advance + 2 * S.padX);
    x += out.columnWidth.back();
  }
  out.width = x;
  out.headHeight = S.headHeight;
  int y = S.headHeight;
  for (const std::size_t most : lines) {
    out.rowY.push_back(y);
    out.rowHeight.push_back(std::max(S.rowHeight, static_cast<int>(most) * S.lineHeight + S.linePad));
    y += out.rowHeight.back();
  }
  out.height = y;
  return out;
}

}  // namespace layout
}  // namespace ds
