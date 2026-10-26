#include "ds/parse/Parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace ds {
namespace parse {

namespace {

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
      file.errorDetail = std::string("missing `") + ts_node_type(node) + "`";
    } else {
      std::string snippet = text.substr(ts_node_start_byte(node), ts_node_end_byte(node) - ts_node_start_byte(node));
      snippet = snippet.substr(0, snippet.find('\n'));
      if (snippet.size() > 24) snippet = snippet.substr(0, 24) + "...";
      file.errorDetail = "unexpected `" + snippet + "`";
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

std::string textOf(const ParsedFile& file, TSNode node) {
  const uint32_t start = ts_node_start_byte(node);
  const uint32_t end = ts_node_end_byte(node);
  return file.text.substr(start, end - start);
}

}  // namespace parse
}  // namespace ds
