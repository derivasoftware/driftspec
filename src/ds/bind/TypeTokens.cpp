#include "ds/bind/TypeTokens.hpp"

#include <cctype>

#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

std::vector<TypeToken> TypeToken::scan(const std::string& text) {
  std::vector<TypeToken> out;
  const auto start = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
  const auto inWord = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
  std::size_t i = 0;
  const std::size_t n = text.size();
  while (i < n) {
    // the tail of a word, or the part after the dot of a number (Literal[1.e5]): no name starts there
    if (!start(text[i]) || (i > 0 && inWord(text[i - 1])) || (i > 1 && text[i - 1] == '.' && inWord(text[i - 2]))) {
      ++i;
      continue;
    }
    // (pkg@)* name ((:: | .)name)*: a dot qualifies as :: does (lsp.Location, DS-319); a dot not followed
    // by a name (Foo., Args&&..., tuple[int, ...]) ends the name and gives no token
    std::size_t j = i;
    for (;;) {
      std::size_t k = j;
      while (k < n && inWord(text[k])) ++k;
      if (k < n && text[k] == '@' && k + 1 < n && start(text[k + 1])) {
        j = k + 1;
        continue;
      }
      if (k + 2 < n && text[k] == ':' && text[k + 1] == ':' && start(text[k + 2])) {
        j = k + 2;
        continue;
      }
      if (k + 1 < n && text[k] == '.' && start(text[k + 1])) {
        j = k + 1;
        continue;
      }
      j = k;
      break;
    }
    TypeToken token;
    // the name is read as if written with ::, and resolved by the same rules; the type's text stays as written
    for (std::size_t k = i; k < j; ++k) token.name += text[k] == '.' ? std::string("::") : std::string(1, text[k]);
    // the arguments of a bracket written right after the name: its arity (R15)
    std::size_t b = j;
    while (b < n && text[b] == ' ') ++b;
    if (b < n && (text[b] == '<' || text[b] == '[')) {
      int depth = 0;
      std::string arg;
      for (std::size_t k = b; k < n; ++k) {
        const char c = text[k];
        if (c == '<' || c == '[' || c == '(') {
          if (depth++ == 0) continue;
        }
        if (c == '>' || c == ']' || c == ')') {
          if (--depth == 0) {
            if (!std::string(text::trimWhitespace(arg)).empty()) token.args.push_back(std::string(text::trimWhitespace(arg)));
            break;
          }
        }
        if (c == ',' && depth == 1) {
          token.args.push_back(std::string(text::trimWhitespace(arg)));
          arg.clear();
          continue;
        }
        arg += c;
      }
    }
    out.push_back(token);
    i = j;
  }
  return out;
}

}  // namespace bind
}  // namespace ds
