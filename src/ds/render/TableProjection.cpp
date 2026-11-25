#include "ds/render/TableProjection.hpp"

#include <sstream>

#include "ds/text/Text.hpp"

namespace ds {
namespace render {

layout::TableText TableProjection::measured(const std::string& text) {
  layout::TableText t;
  std::vector<std::string> words;
  std::istringstream in(text);
  std::string word;
  while (in >> word) {
    t.words.push_back(text::characters(word));  // in characters: ≠ is one, though UTF-8 writes it in three bytes (DS-466)
    words.push_back(word);
  }
  words_.push_back(std::move(words));
  return t;
}

TableProjection::TableProjection(const IrDocument& doc, bool marked) {
  const IrTable& t = *doc.table;
  const auto shown = [marked](const IrCell& cell) {
    if (!marked) return cell.text;
    return cell.kind == "against" ? "! " + cell.text : cell.kind == "none" ? std::string("\xC2\xB7") : cell.text;
  };
  graph_.corner = measured(t.corner);
  for (const std::string& heading : t.columns) graph_.columns.push_back(measured(heading));
  for (const IrRow& row : t.rows) {
    layout::TableRow r;
    r.label = measured(row.label);
    for (const IrCell& cell : row.cells) r.cells.push_back(measured(shown(cell)));
    graph_.rows.push_back(std::move(r));
  }
}

}  // namespace render
}  // namespace ds
