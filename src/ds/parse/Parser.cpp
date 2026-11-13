#include "ds/parse/Parser.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace ds {
namespace parse {

namespace {

// where an error sits, in the reader's words: the nearest declaration around it
std::string contextOf(TSNode node) {
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

// the words a parse state takes: its keywords, then an annotation and a field when it takes those
std::vector<std::string> wordsIn(const TSLanguage* language, TSStateId state) {
  TSLookaheadIterator* it = ts_lookahead_iterator_new(language, state);
  if (!it) return {};
  std::vector<std::string> words;
  bool field = false, annotation = false;
  while (ts_lookahead_iterator_next(it)) {
    const TSSymbol symbol = ts_lookahead_iterator_current_symbol(it);
    const std::string name = ts_lookahead_iterator_current_symbol_name(it);
    const TSSymbolType type = ts_language_symbol_type(language, symbol);
    if (name == "name") field = true;
    if (name == "@") annotation = true;
    if (type == TSSymbolTypeAnonymous && !name.empty() && std::isalpha(static_cast<unsigned char>(name[0])) &&
        std::find(words.begin(), words.end(), name) == words.end())
      words.push_back(name);
  }
  ts_lookahead_iterator_delete(it);
  std::sort(words.begin(), words.end());
  if (annotation) words.push_back("@annotation");
  if (field) words.push_back("key: value");
  return words;
}

// what the grammar would have taken where the error starts: its keywords, and a field or an
// annotation when it takes those; a hint, read from the parse state, not a rule of its own
std::string expectedAt(TSNode node) {
  const TSLanguage* language = ts_node_language(node);
  const TSNode before = ts_node_prev_sibling(node);
  const TSStateId state = ts_node_is_null(before) ? ts_node_parse_state(node) : ts_node_next_parse_state(before);
  TSLookaheadIterator* it = ts_lookahead_iterator_new(language, state);
  if (!it) return "";
  std::vector<std::string> words;
  bool field = false, annotation = false;
  while (ts_lookahead_iterator_next(it)) {
    const TSSymbol symbol = ts_lookahead_iterator_current_symbol(it);
    const std::string name = ts_lookahead_iterator_current_symbol_name(it);
    const TSSymbolType type = ts_language_symbol_type(language, symbol);
    if (name == "name") field = true;
    if (name == "@") annotation = true;
    if (type == TSSymbolTypeAnonymous && !name.empty() && std::isalpha(static_cast<unsigned char>(name[0])) &&
        std::find(words.begin(), words.end(), name) == words.end())
      words.push_back(name);
  }
  ts_lookahead_iterator_delete(it);
  if (words.size() > 16) return "";  // recovery lost its place: the list would be the whole language, noise
  std::sort(words.begin(), words.end());
  std::string out;
  for (const auto& w : words) out += (out.empty() ? "" : ", ") + w;
  if (annotation) out += std::string(out.empty() ? "" : ", ") + "@annotation";
  if (field) out += std::string(out.empty() ? "" : ", ") + "key: value";
  return out;
}

// the first place the tree does not parse: an ERROR node (text the grammar did not expect)
// or a MISSING one (a token it had to invent), searched in document order
bool firstError(TSNode node, const std::string& text, ParsedFile& file) {
  const bool missing = ts_node_is_missing(node);
  const bool error = std::string(ts_node_type(node)) == "ERROR";
  if (error) {  // recovery may wrap a whole block: a narrower error inside it says more
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
      const TSNode child = ts_node_child(node, i);
      if (ts_node_has_error(child) && firstError(child, text, file)) return true;
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
      std::string snippet = text.substr(ts_node_start_byte(node), ts_node_end_byte(node) - ts_node_start_byte(node));
      snippet = snippet.substr(0, snippet.find('\n'));
      if (snippet.size() > 24) snippet = snippet.substr(0, 24) + "...";
      file.errorDetail = "unexpected `" + snippet + "` " + contextOf(node);
      const std::string expected = expectedAt(node);
      if (!expected.empty()) file.errorDetail += "; it takes " + expected;
    }
    return true;
  }
  if (!ts_node_has_error(node)) return false;
  for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
    if (firstError(ts_node_child(node, i), text, file)) return true;
  }
  return false;
}

}  // namespace

Parser::Parser() : parser_(ts_parser_new()) { ts_parser_set_language(parser_, tree_sitter_driftspec()); }

Parser::~Parser() { ts_parser_delete(parser_); }

ParsedFile Parser::parseText(const std::string& path, const std::string& text) {
  ParsedFile file;
  file.path = path;
  file.text = text;
  TSTree* raw = ts_parser_parse_string(parser_, nullptr, text.c_str(), static_cast<uint32_t>(text.size()));
  file.tree = std::shared_ptr<TSTree>(raw, ts_tree_delete);
  file.hasSyntaxErrors = ts_node_has_error(ts_tree_root_node(raw));
  if (file.hasSyntaxErrors && !firstError(ts_tree_root_node(raw), text, file)) {
    file.errorLine = 1;  // an error the walk cannot place: the file, as before
    file.errorColumn = 1;
  }
  return file;
}

std::vector<std::string> Parser::expectedAt(const std::string& text, int line, int column) {
  TSTree* tree = ts_parser_parse_string(parser_, nullptr, text.c_str(), static_cast<uint32_t>(text.size()));
  const TSNode root = ts_tree_root_node(tree);
  const TSPoint at{static_cast<uint32_t>(line), static_cast<uint32_t>(column)};
  // the last token that ends at or before the place: the state after it is what comes next
  TSNode leaf = ts_node_descendant_for_point_range(root, at, at);
  while (!ts_node_is_null(leaf) && ts_node_child_count(leaf) > 0) leaf = ts_node_child(leaf, 0);
  auto before = [&at](TSNode n) {
    const TSPoint end = ts_node_end_point(n);
    return end.row < at.row || (end.row == at.row && end.column <= at.column);
  };
  TSNode last = {};
  TSTreeCursor cursor = ts_tree_cursor_new(root);
  for (bool more = true; more;) {  // walk the leaves in order, keeping the last one before the place
    const TSNode n = ts_tree_cursor_current_node(&cursor);
    if (ts_node_child_count(n) == 0 && before(n) && std::string(ts_node_type(n)) != "comment") last = n;
    if (ts_tree_cursor_goto_first_child(&cursor)) continue;
    while (!ts_tree_cursor_goto_next_sibling(&cursor)) {
      if (!ts_tree_cursor_goto_parent(&cursor)) {
        more = false;
        break;
      }
    }
  }
  ts_tree_cursor_delete(&cursor);
  const TSLanguage* language = ts_tree_language(tree);
  const TSStateId state = ts_node_is_null(last) ? 1 : ts_node_next_parse_state(last);
  std::vector<std::string> words = wordsIn(language, state);
  ts_tree_delete(tree);
  return words;
}

ParsedFile Parser::parseFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return parseText(path, buffer.str());
}

std::vector<ParsedFile> Parser::parseDirectory(const std::string& directory) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".ds") paths.push_back(entry.path().string());
  }
  std::sort(paths.begin(), paths.end());
  std::vector<ParsedFile> files;
  for (const auto& path : paths) files.push_back(parseFile(path));
  return files;
}

std::string ParsedFile::textOf(TSNode node) const {
  const uint32_t start = ts_node_start_byte(node);
  const uint32_t end = ts_node_end_byte(node);
  return text.substr(start, end - start);
}

}  // namespace parse
}  // namespace ds
