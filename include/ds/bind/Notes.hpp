#ifndef DRIFTSPEC_BIND_NOTES_HPP
#define DRIFTSPEC_BIND_NOTES_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace bind {

/// A reference found while binding, resolved later by the Checker.
struct Reference {
  std::string from;          ///< identity of the referring entity
  std::string target;        ///< the name as written
  std::string scope;         ///< qualified scope where it was written (R4)
  std::string relation;      ///< inherits, uses, verifies, refines, realises, exercises, refers
  model::Kind expectedKind;  ///< what to create when nobody declares it (R6)
  model::Location location;
  std::string multiplicity;       ///< for owns/refs/shares (R16)
  std::vector<std::string> args;  ///< template arguments as written, for Repository<Finding> (R15)
  /// a name read out of a type, a base or a bound: it relates only to what the design declares, and is
  /// text otherwise, with no expected node and no finding (DS-18)
  bool soft = false;
};

/// A message of a behaviour: `from -> to.op(args) -> result`, checked against the static
/// context of the behaviour's subject (self, attributes, parameters, locals).
struct Message {
  std::string sequence;  ///< identity of the behaviour
  std::string message;   ///< identity of the message entity
  std::string to;        ///< participant name as written; "self" when omitted
  std::string op;
  int argc = 0;
  model::Location location;
  bool returns = false;  ///< a `return` of the behaviour, not a call
};

/// A transition event (or action) of a state machine, checked against the entity's operations.
struct Event {
  std::string state;
  std::string entity;  ///< as written
  std::string scope;
  std::string event;
  bool action = false;  ///< `/ action` rather than `on event`
  model::Location location;
};

/// What binding leaves for the checker to resolve and check (DS-354): the names written, the messages of the
/// behaviours and the events of the state machines, in the order they were bound. The checker reads only this.
struct Notes {
  std::vector<Reference> references;
  std::vector<Message> messages;
  std::vector<Event> events;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_NOTES_HPP
