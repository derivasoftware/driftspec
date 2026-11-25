#include "ds/machine/Jump.hpp"

namespace ds {
namespace machine {

std::vector<Jump> Jump::of(const model::Entity& machine) {
  std::vector<Jump> out;  // the records the binder read (DS-384)
  for (const model::WrittenTransition& t : machine.transitions) out.push_back({t.from, t.to, t.event, t.guard});
  return out;
}

}  // namespace machine
}  // namespace ds
