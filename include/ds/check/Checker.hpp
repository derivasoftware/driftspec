#ifndef DRIFTSPEC_CHECK_CHECKER_HPP
#define DRIFTSPEC_CHECK_CHECKER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/Step.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Resolves the references the Binder collected (R5, R6), checks the static model against its own
/// rules (R14–R17) and the dynamic specifications against the static ones. It is a runner of steps
/// (DS-138): it orders them by what each needs and runs them over one context, knowing none of them.
class Checker {
 public:
  explicit Checker(model::Model& model);
  void run(const bind::Binder& binder);
  /// What a name means where it is written, and the other read-only questions the checks ask (DS-125).
  const Resolver& resolver() const { return resolver_; }
  /// The steps' keys in the order they run: each after every step it needs, ties by key.
  std::vector<std::string> order() const;

 private:
  model::Model& model_;
  Resolver resolver_;
  std::map<std::string, std::unique_ptr<IStep>> steps_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CHECKER_HPP
