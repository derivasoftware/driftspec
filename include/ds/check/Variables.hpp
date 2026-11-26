#ifndef DRIFTSPEC_CHECK_VARIABLES_HPP
#define DRIFTSPEC_CHECK_VARIABLES_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// The variables of one formula, each given a fixed slot once (DS-381): its ranges' variables, the variables its
/// literals write, its parameters and its columns', in the order of their names. The solver binds a variable by its
/// slot, so a candidate copies a vector of pointers instead of a map of strings; walking the slots in order walks
/// the variables by name, as the map did.
class Variables {
 public:
  explicit Variables(const model::Formula& formula);

  /// How many variables the formula has: the size of every binding of it.
  std::size_t size() const { return names_.size(); }
  /// The slot of a variable; -1 when the formula writes no variable of that name.
  int slotOf(const std::string& name) const;
  /// The name of the variable in a slot.
  const std::string& name(std::size_t slot) const { return names_[slot]; }
  /// Whether one binding comes before another as a map of name to text would sort them: their bound slots compared
  /// as (name, text) pairs in the order of the names. A set of bindings keeps the order the evaluator always had.
  bool before(const std::vector<const std::string*>& a, const std::vector<const std::string*>& b) const;
  /// A binding as (name, text) pairs, its bound slots in the order of the names.
  std::vector<std::pair<std::string, std::string>> pairs(const std::vector<const std::string*>& bindings) const;

  /// A body's terms as slots, read once (DS-507): for each literal, the slot of each of its terms, -1 for a term that is
  /// no variable or a variable the formula does not write. The solver reads a term's slot here at every step, never by
  /// its name.
  using Slots = std::vector<std::vector<int>>;
  /// The slots of a body: one of the formula's, compiled when the variables were made; any other the first time it
  /// is asked.
  const Slots& slotsOf(const std::vector<model::Literal>& body) const;

 private:
  void collect(const std::vector<model::Literal>& body);
  /// The slots of a body's terms, each found by its name once.
  Slots compile(const std::vector<model::Literal>& body) const;

  std::vector<std::string> names_;  ///< sorted, each once
  /// The compiled bodies, by the body's address alone, never read through: the formula's bodies do not move while it
  /// is evaluated
  mutable std::unordered_map<const void*, Slots> bodies_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_VARIABLES_HPP
