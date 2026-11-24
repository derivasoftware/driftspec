#ifndef DRIFTSPEC_CHECK_CHECKER_HPP
#define DRIFTSPEC_CHECK_CHECKER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/bind/Notes.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/Selection.hpp"
#include "ds/check/Step.hpp"
#include "ds/check/Steps.hpp"
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
/// context, knowing none of them. The steps, keyed by name, are built outside it, by StepFactory::of or by a
/// test; it holds the map by reference, so the map outlives it.
class Checker : public IChecker {
 public:
  /// Checks the context's model with the steps given, each made with that same context (DS-425); the model itself
  /// keeps apart what is coverage (DS-346). It holds both by reference, so they outlive it.
  Checker(Context& context, const std::map<std::string, std::unique_ptr<IStep>>& steps) : context_(context), steps_(steps) {}
  Checker(Context& context, std::map<std::string, std::unique_ptr<IStep>>&& steps) = delete;  // a temporary map would die before the run
  /// Starts the context over with the run's notes, then runs the steps in order (DS-425).
  void run(const bind::Notes& notes) override;
  /// What a name means where it is written, and the other read-only questions the checks ask (DS-125).
  const Resolver& resolver() const { return context_.resolver(); }
  /// The steps' keys in the order they run: each after every step it needs, ties by key.
  std::vector<std::string> order() const;

 private:
  Context& context_;
  const std::map<std::string, std::unique_ptr<IStep>>& steps_;
};

/// A checker made for a selection of steps (DS-425): it holds the context, the steps made with it and the checker
/// that runs them, so what the factory hands back lives as long as it is held.
class SelectedChecker : public IChecker {
 public:
  SelectedChecker(model::Model& model, const model::Metamodel& metamodel, const Selection& selection)
      : context_(model, metamodel), steps_(StepFactory::of(context_, selection)), checker_(context_, steps_) {}
  void run(const bind::Notes& notes) override { checker_.run(notes); }

 private:
  Context context_;
  std::map<std::string, std::unique_ptr<IStep>> steps_;
  Checker checker_;
};

/// The one place that makes the checker (DS-404): what asks for a check holds it as the IChecker it is, and names no
/// concrete checker. It makes the context once, the steps with it and the checker that runs them (DS-425).
class CheckerFactory {
 public:
  /// The checker of a selection of steps, every step unless a selection says otherwise (DS-425).
  static std::unique_ptr<IChecker> make(model::Model& model, const model::Metamodel& metamodel,
                                        const Selection& selection = Selection::every());
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CHECKER_HPP
