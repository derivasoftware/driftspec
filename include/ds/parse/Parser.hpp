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
  int errorLine = 0;  ///< where the file stops parsing, 1-based; 0 when it parses
  int errorColumn = 0;
  std::string errorDetail;  ///< what is wrong there: unexpected `text`, or missing `token`

  /// The text a node of its tree spans.
  std::string textOf(TSNode node) const;
};

/// Wraps the tree-sitter parser for the DriftSpec grammar.
class Parser {
 public:
  Parser();
  ~Parser();
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  ParsedFile parseText(const std::string& path, const std::string& text);
  /// What the grammar takes at a place of a text (0-based line and column), read from the parse state
  /// after the last token before it: its keywords, "@annotation" and "key: value" (DS-84).
  std::vector<std::string> expectedAt(const std::string& text, int line, int column);
  ParsedFile parseFile(const std::string& path);
  /// Every *.ds file under `directory`, recursively, in path order.
  std::vector<ParsedFile> parseDirectory(const std::string& directory);

 private:
  TSParser* parser_;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_PARSER_HPP
