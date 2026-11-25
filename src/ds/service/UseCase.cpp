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

int Request::number(const std::string& name, const std::string& fallback) const {
  const std::string text = getOr(name, fallback);
  try {
    std::size_t end = 0;
    const int value = std::stoi(text, &end);
    if (end == text.size()) return value;
  } catch (const std::exception&) {
  }
  throw BadRequest(name + " is no whole number: " + text);
}

bool Request::flag(const std::string& name) const { return getOr(name, "false") == "true"; }

std::vector<std::string> Request::lines(const std::string& name) const {
  const std::string text = getOr(name, "");
  std::vector<std::string> out;
  for (std::size_t from = 0; from < text.size();) {
    const std::size_t end = text.find('\n', from);
    if (end == std::string::npos) {
      out.push_back(text.substr(from));
      break;
    }
    out.push_back(text.substr(from, end - from));
    from = end + 1;
  }
  return out;
}

}  // namespace service
}  // namespace ds
