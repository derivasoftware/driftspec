#ifndef DRIFTSPEC_MACHINE_CONTEXT_HPP
#define DRIFTSPEC_MACHINE_CONTEXT_HPP

#include <map>
#include <set>
#include <string>

#include "ds/machine/StateRule.hpp"
#include "ds/machine/Subject.hpp"

namespace ds {
namespace machine {

/// What each name of a context may be worth (DS-249). A name that is absent may be worth anything.
class Context {
 public:
  /// What a rule lets each name of its subject be, into `out`. False when it asks two things that exclude each other.
  static bool allowed(const Subject& subject, const StateRule& rule, Context* out);
  /// Whether this context and `b` can hold at once, and one in which both do, into `out`.
  bool together(const Context& b, Context* out) const;
  /// The context as a reader is told it: one value for each name.
  std::string told() const;
  /// Whether this context, known in full for what a rule reads, is in the state the rule computes.
  bool within(const Context& rule) const;
  /// A name worth these values.
  void set(const std::string& name, const std::set<std::string>& values) { values_[name] = values; }

 private:
  std::map<std::string, std::set<std::string>> values_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_CONTEXT_HPP
