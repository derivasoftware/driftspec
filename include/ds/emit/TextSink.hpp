#ifndef DRIFTSPEC_EMIT_TEXTSINK_HPP
#define DRIFTSPEC_EMIT_TEXTSINK_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace emit {

/// The sink that writes text for a terminal (DS-399), as ds has always printed its report and its answers: a
/// paragraph is its text and a line break; a table is its rows, a line each, the cells joined by two spaces and not
/// aligned, under no header. It writes the presentation and skips the structure, which is for a program to read.
/// What is written is kept whole until it is taken.
class TextSink final : public ISink {
 public:
  /// The structure is skipped: nothing is written.
  ISink& beginRecord() override;
  ISink& endRecord() override;
  ISink& field(std::string_view name) override;
  ISink& beginList() override;
  ISink& endList() override;
  ISink& value(std::string_view text) override;
  ISink& integer(std::int64_t value) override;
  ISink& number(double value) override;
  ISink& boolean(bool value) override;
  ISink& null() override;
  ISink& paragraph(std::string_view text) override;
  /// The columns name what the cells are; a terminal's table shows no header.
  ISink& table(const std::vector<std::string_view>& columns) override;
  ISink& row(const std::vector<std::string_view>& cells) override;
  ISink& endTable() override;
  /// The text exactly as it is given, with no line break added (DS-474); a second one before the result fails with
  /// std::logic_error, since a text written whole is its document.
  ISink& verbatim(std::string_view text) override;
  /// NotImplemented when no paragraph, no table and no verbatim text was accepted since the last result. A table with no rows was
  /// accepted: what it writes is nothing, and that is the document.
  ISink& described(std::string_view document) override;
  std::string result() override;

 private:
  std::string out_;        ///< the text written so far
  bool accepted_ = false;  ///< a paragraph, a table or a verbatim text was accepted since the last result
  bool verbatim_ = false;  ///< a verbatim text was written since the last result
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_TEXTSINK_HPP
