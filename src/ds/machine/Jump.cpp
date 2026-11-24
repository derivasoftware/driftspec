#include "ds/machine/Jump.hpp"

#include "ds/machine/WrittenFields.hpp"

namespace ds {
namespace machine {

std::vector<Jump> Jump::of(const model::Entity& machine) {
  std::vector<Jump> out;
  for (const std::string& line : WrittenFields::pieces(machine.field("transitions"), '\n')) {
    const auto first = line.find('>');
    const auto second = line.find('>', first + 1);
    if (first == std::string::npos || second == std::string::npos) continue;
    const std::string label = line.substr(second + 1);  // event [guard] / action
    const auto open = label.find('[');
    const auto close = label.find(']', open == std::string::npos ? 0 : open);
    const bool guarded = open != std::string::npos && close != std::string::npos;
    out.push_back({line.substr(0, first), line.substr(first + 1, second - first - 1), label.substr(0, label.find_first_of(" [")),
                   guarded ? label.substr(open + 1, close - open - 1) : ""});
  }
  return out;
}

}  // namespace machine
}  // namespace ds
