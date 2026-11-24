#include "ds/parse/Lookahead.hpp"

#include <algorithm>
#include <cctype>

namespace ds {
namespace parse {

Lookahead::Lookahead(const TSLanguage& language, TSStateId state) {
  TSLookaheadIterator* it = ts_lookahead_iterator_new(&language, state);
  if (!it) return;
  while (ts_lookahead_iterator_next(it)) {
    const TSSymbol symbol = ts_lookahead_iterator_current_symbol(it);
    const std::string name = ts_lookahead_iterator_current_symbol_name(it);
    const TSSymbolType type = ts_language_symbol_type(&language, symbol);
    if (name == "name") field_ = true;
    if (name == "@") annotation_ = true;
    if (type == TSSymbolTypeAnonymous && !name.empty() && std::isalpha(static_cast<unsigned char>(name[0])) &&
        std::find(keywords_.begin(), keywords_.end(), name) == keywords_.end())
      keywords_.push_back(name);
  }
  ts_lookahead_iterator_delete(it);
  std::sort(keywords_.begin(), keywords_.end());
}

std::vector<std::string> Lookahead::words() const {
  std::vector<std::string> words = keywords_;
  if (annotation_) words.push_back("@annotation");
  if (field_) words.push_back("key: value");
  return words;
}

}  // namespace parse
}  // namespace ds
