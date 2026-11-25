#ifndef DRIFTSPEC_EMIT_SINK_HPP
#define DRIFTSPEC_EMIT_SINK_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ds {
namespace emit {

/// What a document is written into (DS-394, DS-397, DS-399), in two halves a description says once, in the order
/// it gives them: the structure, records of named fields, lists and values, for a program to read; and the
/// presentation, paragraphs and tables of rows, for a person to read. A sink writes the half its technology writes
/// and skips the other, so a description never asks which half it is wanted in. A sink knows no document and a
/// document knows no technology: whoever asks for the output builds the sink and lends it, the description writes
/// into it, and the one who built it takes the result. Each call returns the sink, so a record reads as it is
/// written.
class ISink {
 public:
  virtual ~ISink() = default;
  // ---- the structure ----
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
  // ---- the presentation ----
  /// A paragraph: one text, said as it is.
  virtual ISink& paragraph(std::string_view text) = 0;
  /// A table opens with the names of its columns; its rows follow, and endTable() closes it.
  virtual ISink& table(const std::vector<std::string_view>& columns) = 0;
  /// A row of the open table: its cells in the order of the columns. A row ends at its last cell, so it holds
  /// fewer cells than the table has columns when it has nothing for the last of them.
  virtual ISink& row(const std::vector<std::string_view>& cells) = 0;
  virtual ISink& endTable() = 0;
  // ---- the end of a description ----
  /// The description of `document` ends. A sink that has accepted nothing of its half since its last result fails
  /// with NotImplemented, naming the document and its technology, rather than hand over an empty page.
  virtual ISink& described(std::string_view document) = 0;
  /// The document written, handed over: the sink is empty after, ready for another.
  virtual std::string result() = 0;
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_SINK_HPP
