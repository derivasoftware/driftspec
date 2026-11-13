#ifndef DRIFTSPEC_CHECK_SHARED_HPP
#define DRIFTSPEC_CHECK_SHARED_HPP

#include <sstream>
#include <string>
#include <vector>

#include "ds/model/Words.hpp"

// small text helpers the steps of the check share; not part of any interface
namespace ds {
namespace check {

using model::join;
using model::split;

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

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SHARED_HPP
