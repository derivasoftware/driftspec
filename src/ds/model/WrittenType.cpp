#include "ds/model/WrittenType.hpp"

#include <utility>

namespace ds {
namespace model {

WrittenType::WrittenType(std::string text) : text_(std::move(text)) {}

// Word by word: a word that is a name followed by :: is dropped with it, every other word and every other character
// is kept. Each word is read from its start, so a name is never found inside a longer word.
std::string WrittenType::unqualified() const {
  std::string out;
  out.reserve(text_.size());
  const std::size_t n = text_.size();
  std::size_t i = 0;
  while (i < n) {
    if (!inWord(text_[i])) {
      out += text_[i++];
      continue;
    }
    std::size_t end = i;
    while (end < n && inWord(text_[end])) ++end;
    if (startsName(text_[i]) && text_.compare(end, 2, "::") == 0) {
      i = end + 2;
      continue;
    }
    out.append(text_, i, end - i);
    i = end;
  }
  return out;
}

bool WrittenType::inWord(char c) { return startsName(c) || (c >= '0' && c <= '9'); }

bool WrittenType::startsName(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

}  // namespace model
}  // namespace ds
