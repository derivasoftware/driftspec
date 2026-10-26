#include "ds/parse/Parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace ds {
namespace parse {

Parser::Parser() : parser_(ts_parser_new()) { ts_parser_set_language(parser_, tree_sitter_driftspec()); }

Parser::~Parser() { ts_parser_delete(parser_); }

ParsedFile Parser::parseText(const std::string& path, const std::string& text) {
  ParsedFile file;
  file.path = path;
  file.text = text;
  TSTree* raw = ts_parser_parse_string(parser_, nullptr, text.c_str(), static_cast<uint32_t>(text.size()));
  file.tree = std::shared_ptr<TSTree>(raw, ts_tree_delete);
  file.hasSyntaxErrors = ts_node_has_error(ts_tree_root_node(raw));
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
