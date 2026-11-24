#ifndef DRIFTSPEC_EMIT_SINK_HPP
#define DRIFTSPEC_EMIT_SINK_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace ds {
namespace emit {

/// What a document is written into (DS-394, DS-397): records of named fields, lists and values, in the order
/// the writer gives them. A sink knows no document and a document knows no technology: whoever asks for the
/// output builds the sink and lends it, the writer writes into it, and the one who built it takes the result.
/// Each call returns the sink, so a record reads as it is written.
class ISink {
 public:
  virtual ~ISink() = default;
  /// A record opens, as a value where the sink stands; each of its fields is named once, in byte order.
  virtual ISink& beginRecord() = 0;
  virtual ISink& endRecord() = 0;
  /// The name of the next field of the open record; its value follows.
  virtual ISink& field(std::string_view name) = 0;
  /// A list opens, as a value where the sink stands.
  virtual ISink& beginList() = 0;
  virtual ISink& endList() = 0;
  /// A text.
  virtual ISink& value(std::string_view text) = 0;
  virtual ISink& integer(std::int64_t value) = 0;
  virtual ISink& number(double value) = 0;
  virtual ISink& boolean(bool value) = 0;
  /// No value.
  virtual ISink& null() = 0;
  /// The document written, handed over: the sink is empty after, ready for another.
  virtual std::string result() = 0;
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_SINK_HPP
