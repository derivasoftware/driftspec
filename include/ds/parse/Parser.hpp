#ifndef DRIFTSPEC_PARSE_PARSER_HPP
#define DRIFTSPEC_PARSE_PARSER_HPP

#include <memory>
#include <string>
#include <vector>

#include "tree_sitter/api.h"

namespace ds {
namespace parse {

/// One parsed file: its path, its text and the concrete syntax tree.
struct ParsedFile {
  std::string path;
  std::string text;
  std::shared_ptr<TSTree> tree;  ///< freed with ts_tree_delete
  bool hasSyntaxErrors = false;
};

/// Wraps the tree-sitter parser for the DriftSpec grammar.
class Parser {
 public:
  Parser();
  ~Parser();
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  ParsedFile parseText(const std::string& path, const std::string& text);
  ParsedFile parseFile(const std::string& path);
  /// Every *.ds file under `directory`, recursively, in path order.
  std::vector<ParsedFile> parseDirectory(const std::string& directory);

 private:
  TSParser* parser_;
};

/// The text a node spans.
std::string textOf(const ParsedFile& file, TSNode node);

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_PARSER_HPP
