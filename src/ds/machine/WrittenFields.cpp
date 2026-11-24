#include "ds/machine/WrittenFields.hpp"

#include <sstream>

namespace ds {
namespace machine {

std::vector<std::string> WrittenFields::pieces(const std::string& text, char separator) {
  std::vector<std::string> out;
  std::stringstream in(text);
  for (std::string piece; std::getline(in, piece, separator);) out.push_back(piece);
  return out;
}

std::string WrittenFields::joined(const std::vector<std::string>& parts, const std::string& with) {
  std::string out;
  for (const auto& p : parts) out += (out.empty() ? "" : with) + p;
  return out;
}

}  // namespace machine
}  // namespace ds
