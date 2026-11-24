#ifndef DRIFTSPEC_BIND_FUNCTIONBINDER_HPP
#define DRIFTSPEC_BIND_FUNCTIONBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"

namespace ds {
namespace bind {

/// A function of a module (DS-356): a method's signature and body with the module as its scope.
class FunctionBinder : public INodeBinder {
 public:
  FunctionBinder(const SignatureBinder& signatures, const MemberBodyBinder& bodies) : signatures_(signatures), bodies_(bodies) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const SignatureBinder& signatures_;
  const MemberBodyBinder& bodies_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_FUNCTIONBINDER_HPP
