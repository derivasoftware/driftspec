#ifndef DRIFTSPEC_CHECK_CHECKER_HPP
#define DRIFTSPEC_CHECK_CHECKER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/bind/Notes.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/Step.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// The check module's entry point (DS-138): what the rest of ds asks of a check. It resolves the
/// references the Binder collected (R5, R6), checks the static model against its own rules
/// (R14–R17) and the dynamic specifications against the static ones.
class IChecker {
 public:
  virtual ~IChecker() = default;
  virtual void run(const bind::Notes& notes) = 0;
};

/// A runner of steps: it orders the steps it is given by what each needs and runs them over one
/// context, knowing none of them. The steps, keyed by name, are built outside it, by steps() or by a
/// test; it holds the map by reference, so the map outlives it.
class Checker : public IChecker {
 public:
  /// Checks `model` with the steps given, in `metamodel`, the language the model is written in (DS-341); the model
  /// itself keeps apart what is coverage (DS-346).
  Checker(model::Model& model, const model::Metamodel& metamodel, const std::map<std::string, std::unique_ptr<IStep>>& steps);
  Checker(model::Model& model, const model::Metamodel& metamodel,
          std::map<std::string, std::unique_ptr<IStep>>&& steps) = delete;  // a temporary map would die before the run
  void run(const bind::Notes& notes) override;
  /// What a name means where it is written, and the other read-only questions the checks ask (DS-125).
  const Resolver& resolver() const { return resolver_; }
  /// The steps' keys in the order they run: each after every step it needs, ties by key.
  std::vector<std::string> order() const;

 private:
  model::Model& model_;
  const model::Metamodel& metamodel_;
  Resolver resolver_;
  const std::map<std::string, std::unique_ptr<IStep>>& steps_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CHECKER_HPP
