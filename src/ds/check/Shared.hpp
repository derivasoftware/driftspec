#ifndef DRIFTSPEC_CHECK_SHARED_HPP
#define DRIFTSPEC_CHECK_SHARED_HPP

#include <sstream>
#include <string>
#include <vector>

// small text helpers the steps of the check share; not part of any interface
namespace ds {
namespace check {

/// Primitives one converts to the other without saying so; any other pair of primitives is a mismatch.
inline int primitiveFamily(const std::string& type) {
  for (const char* t : {"int", "float", "double", "size_t", "char"}) {
    if (type == t) return 1;
  }
  for (const char* t : {"string", "path"}) {
    if (type == t) return 2;
  }
  if (type == "bool") return 3;
  if (type == "none" || type == "void") return 4;
  return 0;
}

inline std::vector<std::string> split(const std::string& text, char sep) {
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

inline std::string join(const std::vector<std::string>& items, const char* sep) {
  std::string out;
  for (const auto& i : items) out += (out.empty() ? "" : sep) + i;
  return out;
}

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SHARED_HPP
