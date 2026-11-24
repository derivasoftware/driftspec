#ifndef DRIFTSPEC_MACHINE_MACHINE_HPP
#define DRIFTSPEC_MACHINE_MACHINE_HPP

#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// One call of an event of a machine, found in a behaviour written (DS-250): who makes it, on what, and the
/// states its receiver may be in when it is made.
struct Call {
  std::string behaviour;          ///< the behaviour that makes it, as a reader calls it: Session::restart
  std::string message;            ///< the message that makes it
  std::string receiver;           ///< the name it is made on, in that behaviour
  std::string event;              ///< the operation called: an event of the machine
  std::vector<std::string> from;  ///< the states the receiver may be in when it is made
  bool fits = true;               ///< whether a jump of the machine lets the event happen from one of them
};

/// A state machine as its author wrote it (DS-249), read once for whoever needs it: the check that holds its
/// rules to each other and to the structure, and the drawing. A state is computed by a rule over the context
/// of the machine's subject: what its attributes are worth. The rules of a machine exclude each other, so
/// that whatever the subject holds, it is in one state.
class Machine {
 public:
  Machine(const model::Model& model, const model::Entity& machine);

  /// The states, in the order written: those a rule computes, then those only a jump or the start names.
  std::vector<std::string> states() const;
  /// The rules that compute a state, as written; a state may have several, each one way of being in it.
  std::vector<std::string> rules(const std::string& state) const;
  /// The state the subject starts in: the one written, else the one its defaults are in; empty when the
  /// defaults are in no state or do not tell.
  std::string initial() const;
  /// The states the machine ends in, as its `final` lines name them (DS-442); none when it names none.
  std::vector<std::string> finals() const;
  /// What the rules say that cannot be: an attribute the subject does not hold, a value its type does not
  /// have, a rule that excludes itself, and two states that can hold at once, with a context in which they do.
  /// Each is made by `metamodel` from its code (DS-379).
  std::vector<model::Finding> faults(const model::Metamodel& metamodel) const;
  /// The events its jumps are made on, in the order written.
  std::vector<std::string> events() const;
  /// What the machine is of: the identity of its subject.
  const std::string& subject() const { return subject_; }
  /// Where the jumps on an event lead from a state, in the order written, each with its guard when it has
  /// one: Paid, or Paid [enough]; none when the machine does not let the event happen there.
  std::vector<std::string> targets(const std::string& event, const std::string& from) const;
  /// The states the jumps let an event happen from, in the order written.
  std::vector<std::string> sources(const std::string& event) const;
  /// Every call of one of its events in the behaviours written (DS-250), each behaviour in the order it is
  /// written. Along a behaviour the states a receiver may be in are kept: any, to begin with; a call of an
  /// event moves them by the jumps on it; a jump with a guard, and a call inside a frame, may leave them
  /// where they were; an operation that is no event of the machine moves nothing. A call no jump lets
  /// happen from any of them does not fit.
  std::vector<Call> calls() const;

 private:
  /// Where a line of the machine is written: the machine's place, on that line when it is known.
  static model::Location at(const model::Entity& machine, int line);
  /// A name added once, in the order met; an empty one adds nothing.
  static void note(std::vector<std::string>& all, const std::string& name);

  const model::Model& model_;
  const model::Entity& machine_;
  std::string subject_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_MACHINE_HPP
