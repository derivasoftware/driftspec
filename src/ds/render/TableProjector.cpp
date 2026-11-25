#include "ds/render/TableProjector.hpp"

#include <nlohmann/json.hpp>

#include "ds/machine/Table.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

nlohmann::json TableProjector::project(const model::Entity& diagram) const {
  nlohmann::json out = {{"nodes", nlohmann::json::array()}, {"edges", nlohmann::json::array()}};
  const auto listed = [](const std::vector<std::string>& names) {
    std::string all;
    for (const std::string& name : names) all += (all.empty() ? "" : ", ") + name;
    return all;
  };
  const auto written = model_.tables().find(diagram.id);
  if (written != model_.tables().end() && !written->second.columns.empty()) {
    // written with its rows and columns (DS-268): the first column heads each row
    const model::Tabulated& t = written->second;
    nlohmann::json rows = nlohmann::json::array();
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
      nlohmann::json cells = nlohmann::json::array();
      for (std::size_t c = 1; c < t.rows[r].size(); ++c)
        cells.push_back({{"text", t.rows[r][c]}, {"kind", t.kinds[r][c].empty() ? "jump" : t.kinds[r][c]}, {"title", ""}});
      rows.push_back({{"label", t.rows[r][0]}, {"title", ""}, {"cells", cells}});
    }
    out["table"] = {
        {"corner", t.columns[0]}, {"columns", std::vector<std::string>(t.columns.begin() + 1, t.columns.end())}, {"rows", rows}};
    return out;
  }
  for (const auto& id : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {
    const machine::Table table(model_, *model_.find(id));
    nlohmann::json rows = nlohmann::json::array();
    for (const machine::Row& row : table.rows()) {
      nlohmann::json cells = nlohmann::json::array();
      for (std::size_t i = 0; i < row.cells.size(); ++i) {
        const machine::Cell& cell = row.cells[i];
        const std::string& state = table.states()[i];
        if (!cell.against.empty())
          cells.push_back(
              {{"text", listed(cell.against)},
               {"kind", "against"},
               {"title", listed(cell.against) + " calls " + row.event + " when it can be " + state + ": the machine has no jump here"}});
        else if (cell.to.empty())
          cells.push_back({{"text", ""}, {"kind", "none"}, {"title", ""}});
        else
          cells.push_back({{"text", cell.to}, {"kind", "jump"}, {"title", row.event + " takes " + state + " to " + cell.to}});
      }
      rows.push_back({{"label", row.event},
                      {"title", row.callers.empty() ? "" : row.event + " is called in " + listed(row.callers)},
                      {"cells", cells}});
    }
    out["table"] = {{"corner", table.subject()}, {"columns", table.states()}, {"rows", rows}};
    break;  // one machine is one table
  }
  if (!out.contains("table")) out["notice"] = "No state machine selected: include names what it is of (shop@order::Order)";
  return out;
}

}  // namespace render
}  // namespace ds
