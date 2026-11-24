#ifndef DRIFTSPEC_CHECK_CONTEXT_HPP
#define DRIFTSPEC_CHECK_CONTEXT_HPP

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "ds/bind/Notes.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// What every step of one check is given (DS-138): the model it checks, the metamodel it is written in, what the
/// binder collected, the resolver, the renames linking made and the identities already reported as undeclared.
class Context {
 public:
  /// Made once, for every run of one checker: it resolves names in `model`, the model written in `metamodel` (DS-425).
  Context(model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel), resolver_(model) {}
  /// A run starts over: what binding left to resolve and check, and nothing of the run before (DS-425).
  void reset(const bind::Notes& notes) {
    notes_ = &notes;
    renamed_.clear();
    reported_.clear();
    found_.clear();
  }

  model::Model& model() { return model_; }
  /// What a step that only checks is given: the model, to read.
  const model::Model& model() const { return model_; }
  /// The language the model is written in, handed to the checker (DS-351): the kinds by name, the relations, the fields.
  const model::Metamodel& metamodel() const { return metamodel_; }
  const Resolver& resolver() const { return resolver_; }
  /// What binding left to resolve and check: the references, the messages and the events (DS-354).
  const bind::Notes& notes() const {
    if (!notes_) throw std::logic_error("the context is read before a run reset it with the notes");
    return *notes_;
  }
  /// An identity after the renames linking made.
  std::string current(const std::string& id) const;
  std::map<std::string, std::string>& renamed() { return renamed_; }
  std::set<std::string>& reported() { return reported_; }
  const std::set<std::string>& reported() const { return reported_; }
  /// A finding of the step that is running; take() hands them back when it ends.
  void report(const model::Finding& finding) { found_.push_back(finding); }
  std::vector<model::Finding> take();
  /// A reach from `from` down to `target` that crosses something private (W-NOT-VISIBLE).
  void checkVisibility(const std::string& from, const std::string& target, const model::Location& location);
  /// A public member that shows a private type (W-PRIVATE-LEAK).
  void checkLeak(const std::string& from, const std::string& target, const model::Location& location);

 private:
  model::Model& model_;
  const model::Metamodel& metamodel_;
  Resolver resolver_;
  const bind::Notes* notes_ = nullptr;  ///< the run's, set by reset
  std::map<std::string, std::string> renamed_;
  std::set<std::string> reported_;
  std::vector<model::Finding> found_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CONTEXT_HPP
