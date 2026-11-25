#ifndef DRIFTSPEC_APP_OUTPUT_HPP
#define DRIFTSPEC_APP_OUTPUT_HPP

#include <string>
#include <string_view>

namespace ds {
namespace app {

/// Where a command writes what it was asked for (DS-283, DS-409): the file -o names, or stdout when it names none.
class Output {
 public:
  /// To the file `file`; "" is stdout.
  explicit Output(std::string file = "");
  /// The text, then what ends it (the line a JSON document ends with), the same bytes on every system; false when
  /// the file cannot be written, and nothing is.
  bool write(std::string_view text, std::string_view end = "") const;
  /// The file it writes to; "" for stdout.
  const std::string& file() const { return file_; }

 private:
  const std::string file_;
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_OUTPUT_HPP
