#include "ds/machine/Table.hpp"

#include <algorithm>

#include "ds/machine/Machine.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

namespace {

void note(std::vector<std::string>& all, const std::string& name) {
  if (std::find(all.begin(), all.end(), name) == all.end()) all.push_back(name);
}

}  // namespace

Table::Table(const model::Model& model, const model::Entity& machine) {
  const Machine written(model, machine);
  const model::Entity* subject = model.find(written.subject());
  subject_ = subject ? subject->name : machine.name;
  states_ = written.states();
  for (const std::string& event : written.events()) {
    Row row{event, {}, {}};
    for (const std::string& state : states_) {
      Cell cell;
      for (const std::string& to : written.targets(event, state)) cell.to += (cell.to.empty() ? "" : " / ") + to;
      row.cells.push_back(cell);
    }
    rows_.push_back(row);
  }
  for (const Call& call : written.calls()) {
    for (Row& row : rows_) {
      if (row.event != call.event) continue;
      note(row.callers, call.behaviour);
      if (call.fits) continue;
      for (std::size_t i = 0; i < states_.size(); ++i)  // every state its receiver can be in then: none has a jump on it
        if (std::find(call.from.begin(), call.from.end(), states_[i]) != call.from.end()) note(row.cells[i].against, call.behaviour);
    }
  }
}

}  // namespace machine
}  // namespace ds
