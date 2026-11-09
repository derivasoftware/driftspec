#ifndef DRIFTSPEC_CHECK_CONTEXT_HPP
#define DRIFTSPEC_CHECK_CONTEXT_HPP

#include <map>
#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// What every step of one check is given (DS-138): the model it checks, what the binder collected,
/// the resolver, the renames linking made and the identities already reported as undeclared.
class Context {
 public:
  Context(model::Model& model, const Resolver& resolver, const bind::Binder& binder)
      : model_(model), resolver_(resolver), binder_(binder) {}

  model::Model& model() const { return model_; }
  const Resolver& resolver() const { return resolver_; }
  const bind::Binder& binder() const { return binder_; }
  /// An identity after the renames linking made.
  std::string current(const std::string& id) const;
  std::map<std::string, std::string>& renamed() { return renamed_; }
  std::set<std::string>& reported() { return reported_; }
  /// A reach from `from` down to `target` that crosses something private (W-NOT-VISIBLE).
  void checkVisibility(const std::string& from, const std::string& target, const model::Location& location);
  /// A public member that shows a private type (W-PRIVATE-LEAK).
  void checkLeak(const std::string& from, const std::string& target, const model::Location& location);

 private:
  model::Model& model_;
  const Resolver& resolver_;
  const bind::Binder& binder_;
  std::map<std::string, std::string> renamed_;
  std::set<std::string> reported_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CONTEXT_HPP
