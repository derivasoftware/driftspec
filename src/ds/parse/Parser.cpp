#include "ds/parse/Parser.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ds/parse/Lookahead.hpp"
#include "ds/parse/SyntaxError.hpp"
#include "ds/text/Utf8.hpp"

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace ds {
namespace parse {

Parser::Parser() : parser_(ts_parser_new()) { ts_parser_set_language(parser_, tree_sitter_driftspec()); }

Parser::~Parser() { ts_parser_delete(parser_); }

ParsedFile Parser::parseText(const std::string& path, const std::string& text) {
  const std::size_t bad = ds::text::Utf8::invalidAt(text);
  if (bad != std::string::npos) {
    // not UTF-8 (DS-411): marked at the first byte that is not part of a character, its line and its column in bytes
    // counted from 1 as a syntax error's are; read no further, so none of its text reaches the model or a writer
    ParsedFile file;
    file.path = path;
    file.hasEncodingError = true;
    const std::size_t lineStart = text.rfind('\n', bad);  // the byte is no new line: a new line is ASCII
    file.errorLine = 1 + static_cast<int>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(bad), '\n'));
    file.errorColumn = 1 + static_cast<int>(lineStart == std::string::npos ? bad : bad - lineStart - 1);
    char byte[8];
    std::snprintf(byte, sizeof byte, "0x%02X", static_cast<unsigned>(static_cast<unsigned char>(text[bad])));
    file.errorDetail = std::string("byte ") + byte + " is not part of a character";
    file.tree = std::shared_ptr<TSTree>(ts_parser_parse_string(parser_, nullptr, "", 0), ts_tree_delete);
    return file;
  }
  // a file written on Windows ends its lines in CRLF: read as LF, so no text the model keeps ends in a
  // carriage return; lines and columns stay where they were (DS-315)
  std::string read;
  read.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i)
    if (!(text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')) read += text[i];
  ParsedFile file;
  file.path = path;
  file.text = read;
  TSTree* raw = ts_parser_parse_string(parser_, nullptr, read.c_str(), static_cast<uint32_t>(read.size()));
  file.tree = std::shared_ptr<TSTree>(raw, ts_tree_delete);
  file.hasSyntaxErrors = ts_node_has_error(ts_tree_root_node(raw));
  if (file.hasSyntaxErrors && !SyntaxError(read).place(ts_tree_root_node(raw), file)) {
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
  const TSStateId state = ts_node_is_null(last) ? 1 : ts_node_next_parse_state(last);
  std::vector<std::string> words = Lookahead(*ts_tree_language(tree), state).words();
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

}  // namespace parse
}  // namespace ds
