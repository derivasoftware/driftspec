#include "ds/render/TableProjector.hpp"

#include "ds/machine/Table.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

IrDocument TableProjector::project(const model::Entity& diagram) const {
  IrDocument out;
  const auto listed = [](const std::vector<std::string>& names) {
    std::string all;
    for (const std::string& name : names) all += (all.empty() ? "" : ", ") + name;
    return all;
  };
  const auto written = model_.tables().find(diagram.id);
  if (written != model_.tables().end() && !written->second.columns.empty()) {
    // written with its rows and columns (DS-268): the first column heads each row
    const model::Tabulated& t = written->second;
    IrTable table{t.columns[0], std::vector<std::string>(t.columns.begin() + 1, t.columns.end()), {}};
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
      IrRow row{t.rows[r][0], "", {}};
      for (std::size_t c = 1; c < t.rows[r].size(); ++c)
        row.cells.push_back({t.rows[r][c], t.kinds[r][c].empty() ? "jump" : t.kinds[r][c], ""});
      table.rows.push_back(row);
    }
    out.table = table;
    return out;
  }
  for (const auto& id : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {
    const machine::Table table(model_, *model_.find(id));
    IrTable drawn{table.subject(), table.states(), {}};
    for (const machine::Row& row : table.rows()) {
      std::vector<IrCell> cells;
      for (std::size_t i = 0; i < row.cells.size(); ++i) {
        const machine::Cell& cell = row.cells[i];
        const std::string& state = table.states()[i];
        if (!cell.against.empty())
          cells.push_back({listed(cell.against), "against",
                           listed(cell.against) + " calls " + row.event + " when it can be " + state + ": the machine has no jump here"});
        else if (cell.to.empty())
          cells.push_back({"", "none", ""});
        else
          cells.push_back({cell.to, "jump", row.event + " takes " + state + " to " + cell.to});
      }
      drawn.rows.push_back({row.event, row.callers.empty() ? "" : row.event + " is called in " + listed(row.callers), cells});
    }
    out.table = drawn;
    break;  // one machine is one table
  }
  if (!out.table) out.notice = "No state machine selected: include names what it is of (shop@order::Order)";
  return out;
}

}  // namespace render
}  // namespace ds
