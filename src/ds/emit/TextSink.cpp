#include "ds/emit/TextSink.hpp"

#include <cstddef>

#include "ds/emit/NotImplemented.hpp"

namespace ds {
namespace emit {

ISink& TextSink::beginRecord() { return *this; }

ISink& TextSink::endRecord() { return *this; }

ISink& TextSink::field(std::string_view) { return *this; }

ISink& TextSink::beginList() { return *this; }

ISink& TextSink::endList() { return *this; }

ISink& TextSink::value(std::string_view) { return *this; }

ISink& TextSink::integer(std::int64_t) { return *this; }

ISink& TextSink::number(double) { return *this; }

ISink& TextSink::boolean(bool) { return *this; }

ISink& TextSink::null() { return *this; }

ISink& TextSink::paragraph(std::string_view text) {
  accepted_ = true;
  out_.append(text);
  out_ += '\n';
  return *this;
}

ISink& TextSink::table(const std::vector<std::string_view>&) {
  accepted_ = true;
  return *this;
}

ISink& TextSink::row(const std::vector<std::string_view>& cells) {
  accepted_ = true;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    if (i) out_.append("  ", 2);
    out_.append(cells[i]);
  }
  out_ += '\n';
  return *this;
}

ISink& TextSink::endTable() {
  accepted_ = true;
  return *this;
}

ISink& TextSink::described(std::string_view document) {
  if (!accepted_) throw NotImplemented(std::string(document), "text", {});
  return *this;
}

std::string TextSink::result() {
  std::string written;
  written.swap(out_);
  accepted_ = false;
  return written;
}

}  // namespace emit
}  // namespace ds
