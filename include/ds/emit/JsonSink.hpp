#ifndef DRIFTSPEC_EMIT_JSONSINK_HPP
#define DRIFTSPEC_EMIT_JSONSINK_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace emit {

/// The sink that writes JSON (DS-397), as ds has always written it: a record is an object, a list an array; the
/// escapes, the two layouts and the numbers are those of nlohmann::json 3.11.3's dump(). It builds no tree: what
/// is written goes into one text, kept whole until it is taken, so a write that fails leaves nothing written.
/// The fields of a record are written in the order given, which is byte order (keptBy before kind). A text that
/// is not UTF-8 is refused, as nlohmann refuses it. It writes the structure and skips the presentation (DS-399): a
/// paragraph or a table is for a person, and JSON is for a program.
class JsonSink final : public ISink {
 public:
  /// Indented by two spaces when pretty, else compact. Checked, it refuses with std::logic_error a field out of
  /// byte order or repeated within a record, a field outside a record, a value where none goes and a result
  /// taken before the document is closed; it is checked unless ds is built with NDEBUG.
  explicit JsonSink(bool pretty, bool checked = checkedByDefault());

  ISink& beginRecord() override;
  ISink& endRecord() override;
  ISink& field(std::string_view name) override;
  ISink& beginList() override;
  ISink& endList() override;
  ISink& value(std::string_view text) override;
  ISink& integer(std::int64_t value) override;
  /// The shortest digits that read back (std::to_chars), in nlohmann's layout: 2.0, 0.001, 1e+15, -0.0;
  /// null when it is not finite.
  ISink& number(double value) override;
  ISink& boolean(bool value) override;
  ISink& null() override;
  /// The presentation is skipped: nothing is written.
  ISink& paragraph(std::string_view text) override;
  ISink& table(const std::vector<std::string_view>& columns) override;
  ISink& row(const std::vector<std::string_view>& cells) override;
  ISink& endTable() override;
  /// NotImplemented when nothing is written since the last result: JSON writes a byte for every part of the
  /// structure it accepts, so what it holds says whether it accepted any.
  ISink& described(std::string_view document) override;
  std::string result() override;

 private:
  /// Whether a sink is checked when its builder does not say: as DS_JSON_CHECK says (1 checks, 0 does not), whatever
  /// the build (DS-461); without it, in every build without NDEBUG.
  static bool checkedByDefault();
  /// What goes before a value: in a list, the separator; in a record, nothing (its field wrote it).
  void beforeValue();
  /// A container opens or closes: '{' or '[', '}' or ']'.
  void open(char kind);
  void close(char kind);
  /// The comma, line break and indentation before the next item of the innermost container.
  void separator();
  /// A text between quotes, escaped; refused when it is not UTF-8.
  void quoted(std::string_view text);
  /// Where the UTF-8 sequence that starts at `at` (a byte of 0x80 or more) ends; it throws at the first byte
  /// that cannot belong to it, with nlohmann's message.
  static std::size_t sequenceEnd(std::string_view text, std::size_t at);
  [[noreturn]] static void invalidByte(std::size_t at, unsigned char byte);
  [[noreturn]] static void incomplete(unsigned char last);
  static std::string hexByte(unsigned char byte);
  /// A misuse of the sink, refused when it is checked.
  [[noreturn]] static void misuse(const std::string& what);

  std::string out_;                ///< the document written so far
  bool pretty_;                    ///< indented by two spaces, else compact
  bool checked_;                   ///< the order of the fields and the shape of the document are checked
  std::string open_;               ///< the containers open, innermost last: r a record, l a list, upper case once one holds an item
  std::vector<std::string> keys_;  ///< checked: the last field of each open record
  bool awaiting_ = false;          ///< checked: a field is named and its value not yet written
  bool done_ = false;              ///< checked: the document's one value is written
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_JSONSINK_HPP
