#ifndef DRIFTSPEC_MACHINE_TABLE_HPP
#define DRIFTSPEC_MACHINE_TABLE_HPP

#include <string>
#include <vector>

namespace ds {
namespace model {
class Model;
struct Entity;
}  // namespace model

namespace machine {

/// One cell of the table of a machine (DS-251): what an event does from a state.
struct Cell {
  std::string to;                    ///< where the jumps on the event lead from the state: Paid, or Paid / Cancelled; "" when none
  std::vector<std::string> against;  ///< the behaviours that call the event when its receiver can only be where no jump is, here among them
};

/// One row of the table: an event, the behaviours that call it, and its cell under each state.
struct Row {
  std::string event;
  std::vector<std::string> callers;
  std::vector<Cell> cells;
};

/// A machine as a table (DS-251): its events down, its states across, and in each cell where the event takes
/// the state. Over it, what the behaviours written do: who calls each event, and the cells in which a
/// behaviour calls one though the machine has no jump there.
class Table {
 public:
  Table(const model::Model& model, const model::Entity& machine);
  /// The name of what the machine is of.
  const std::string& subject() const { return subject_; }
  /// Its states, across, in the order written.
  const std::vector<std::string>& states() const { return states_; }
  /// Its events, down, in the order written, each with a cell under every state.
  const std::vector<Row>& rows() const { return rows_; }

 private:
  /// A name added once, in the order met.
  static void note(std::vector<std::string>& all, const std::string& name);
  std::string subject_;
  std::vector<std::string> states_;
  std::vector<Row> rows_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_TABLE_HPP
