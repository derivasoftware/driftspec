#include "ds/format/Description.hpp"

namespace ds {
namespace format {

void ADescription::describe(emit::ISink& out_sink) const {
  say(out_sink);
  out_sink.described(name());
}

}  // namespace format
}  // namespace ds
