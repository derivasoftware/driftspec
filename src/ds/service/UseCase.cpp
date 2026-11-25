#include "ds/service/UseCase.hpp"

namespace ds {
namespace service {

const std::string& Request::get(const std::string& name) const {
  const auto it = arguments.find(name);
  if (it == arguments.end()) throw BadRequest("the request needs " + name);
  return it->second;
}

std::string Request::getOr(const std::string& name, const std::string& fallback) const {
  const auto it = arguments.find(name);
  return it == arguments.end() ? fallback : it->second;
}

}  // namespace service
}  // namespace ds
