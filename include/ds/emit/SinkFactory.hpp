#ifndef DRIFTSPEC_EMIT_SINKFACTORY_HPP
#define DRIFTSPEC_EMIT_SINKFACTORY_HPP

#include <memory>
#include <string>
#include <vector>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace emit {

/// The technology axis (DS-397, DS-398): the one place that names the sinks. A door that asks for an output is
/// handed the sink of its technology as an ISink, lends it to the description of the document and takes the
/// result. A new technology is one sink and one line here; no document changes.
class SinkFactory {
 public:
  /// The sink of the technology named (json, text), JSON indented when pretty; NotImplemented for a technology ds
  /// has no sink of.
  static std::unique_ptr<ISink> of(const std::string& technology, bool pretty);
  /// The sink that writes JSON, the technology every answer of the engine's interfaces is written in: indented by
  /// two spaces when pretty, else compact; checked unless ds is built with NDEBUG.
  static std::unique_ptr<ISink> json(bool pretty);
  /// The sink that writes text for a terminal: the report and the answers as ds prints them (DS-399).
  static std::unique_ptr<ISink> text();
  /// The technologies there is a sink of, for a message.
  static std::vector<std::string> names();
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_SINKFACTORY_HPP
