#ifndef DRIFTSPEC_EMIT_SINKFACTORY_HPP
#define DRIFTSPEC_EMIT_SINKFACTORY_HPP

#include <memory>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace emit {

/// The one place that names the sinks (DS-397): a door that asks for an output is handed the sink of its
/// technology as an ISink, lends it to the writer and takes the result.
class SinkFactory {
 public:
  /// The sink that writes JSON: indented by two spaces when pretty, else compact; checked unless ds is built
  /// with NDEBUG.
  static std::unique_ptr<ISink> json(bool pretty);
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_SINKFACTORY_HPP
