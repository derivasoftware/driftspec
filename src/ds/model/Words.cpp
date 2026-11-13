#include "ds/model/Words.hpp"

#include <sstream>

namespace ds {
namespace model {

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, sep)) {
    const auto b = item.find_first_not_of(' ');
    const auto e = item.find_last_not_of(' ');
    if (b != std::string::npos) out.push_back(item.substr(b, e - b + 1));
  }
  return out;
}

std::string join(const std::vector<std::string>& items, const std::string& sep) {
  std::string out;
  for (const auto& i : items) out += (out.empty() ? std::string() : sep) + i;
  return out;
}

}  // namespace model
}  // namespace ds
