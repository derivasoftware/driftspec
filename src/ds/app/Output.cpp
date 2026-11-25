#include "ds/app/Output.hpp"

#include <fstream>
#include <iostream>
#include <utility>

namespace ds {
namespace app {

Output::Output(std::string file) : file_(std::move(file)) {}

bool Output::write(std::string_view text, std::string_view end) const {
  if (file_.empty()) {
    std::cout << text << end;
    return true;
  }
  std::ofstream out(file_, std::ios::binary);  // the same bytes on every system (DS-283)
  if (!out) return false;
  out << text << end;
  return true;
}

}  // namespace app
}  // namespace ds
