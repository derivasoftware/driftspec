#include "ds/bind/LegacyBinder.hpp"

namespace ds {
namespace bind {

std::vector<std::string> LegacyBinder::kinds() const { return {}; }

void LegacyBinder::bind(TSNode, const BindContext&) const {}

}  // namespace bind
}  // namespace ds
