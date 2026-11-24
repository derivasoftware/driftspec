#include "ds/parse/SyntaxError.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ds/parse/Lookahead.hpp"

namespace ds {
namespace parse {

std::string SyntaxError::contextOf(TSNode node) {
  static const std::pair<const char*, const char*> kWhere[] = {
      {"class_body", "in a class"},        {"member_body", "in the body of a member"},
      {"module", "in a module"},           {"package", "in a package"},
      {"interface", "in an interface"},    {"operation", "in an operation"},
      {"sequence", "in a sequence"},       {"state", "in a state machine"},
      {"requirement", "in a requirement"}, {"verification", "in a verification"},
      {"constraint", "in a constraint"},   {"usecase", "in a use case"},
      {"diagram", "in a diagram"},         {"source_file", "at the top of the file"}};
  for (TSNode n = ts_node_parent(node); !ts_node_is_null(n); n = ts_node_parent(n)) {
    const std::string type = ts_node_type(n);
    for (const auto& w : kWhere)
      if (type == w.first) return w.second;
  }
  return "at the top of the file";
}

std::string SyntaxError::expectedAt(TSNode node) {
  const TSNode before = ts_node_prev_sibling(node);
  const TSStateId state = ts_node_is_null(before) ? ts_node_parse_state(node) : ts_node_next_parse_state(before);
  const Lookahead takes(*ts_node_language(node), state);
  if (takes.keywords().size() > 16) return "";  // recovery lost its place: the list would be the whole language, noise
  std::string out;
  for (const auto& w : takes.words()) out += (out.empty() ? "" : ", ") + w;
  return out;
}

bool SyntaxError::place(TSNode node, ParsedFile& file) const {
  const bool missing = ts_node_is_missing(node);
  const bool error = std::string(ts_node_type(node)) == "ERROR";
  if (error) {  // recovery may wrap a whole block: a narrower error inside it says more
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
      const TSNode child = ts_node_child(node, i);
      if (ts_node_has_error(child) && place(child, file)) return true;
    }
  }
  if (missing || error) {
    const TSPoint at = ts_node_start_point(node);
    file.errorLine = static_cast<int>(at.row) + 1;
    file.errorColumn = static_cast<int>(at.column) + 1;
    if (missing) {
      file.errorDetail = std::string("missing `") + ts_node_type(node) + "` " + contextOf(node);
      const std::string expected = expectedAt(node);  // the parser closed early: what was written there is not taken
      if (!expected.empty()) file.errorDetail += ", which takes " + expected;
    } else {
      std::string snippet = text_.substr(ts_node_start_byte(node), ts_node_end_byte(node) - ts_node_start_byte(node));
      snippet = snippet.substr(0, snippet.find('\n'));
      if (snippet.size() > 24) {  // cut on a code point boundary, never inside one: the message stays UTF-8 (DS-334)
        std::size_t cut = 24;
        while (cut > 0 && (static_cast<unsigned char>(snippet[cut]) & 0xC0) == 0x80) --cut;
        snippet = snippet.substr(0, cut) + "...";
      }
      file.errorDetail = "unexpected `" + snippet + "` " + contextOf(node);
      const std::string expected = expectedAt(node);
      if (!expected.empty()) file.errorDetail += "; it takes " + expected;
    }
    return true;
  }
  if (!ts_node_has_error(node)) return false;
  for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
    if (place(ts_node_child(node, i), file)) return true;
  }
  return false;
}

}  // namespace parse
}  // namespace ds
