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
  std::string text;              ///< empty when the file is not UTF-8: none of it is read (DS-411)
  std::shared_ptr<TSTree> tree;  ///< freed with ts_tree_delete; the empty tree when the file is not UTF-8
  bool hasSyntaxErrors = false;
  bool hasEncodingError = false;  ///< a byte that is not part of a UTF-8 character: the file is read no further (DS-411)
  int errorLine = 0;              ///< where the file stops parsing, or its first byte that is not UTF-8, 1-based; 0 when it parses
  int errorColumn = 0;            ///< 1-based, counted in bytes
  std::string errorDetail;        ///< what is wrong there: unexpected `text`, missing `token`, or the byte that is not UTF-8
};

/// Wraps the tree-sitter parser for the DriftSpec grammar.
class Parser {
 public:
  Parser();
  ~Parser();
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  /// The DriftSpec grammar this parser reads with: its symbols name the kinds of node (DS-355). A parsed file carries it
  /// in its tree, so what reads a file is given it with the file and nothing reaches for it (DS-374).
  const TSLanguage& language() const { return *ts_parser_language(parser_); }
  /// A text read into a tree. A text that is not UTF-8 is read no further (DS-411): the file comes back marked at its
  /// first byte that is not part of a character, with the empty tree and no text, so nothing of it reaches the model.
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
