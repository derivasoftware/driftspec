#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include <fstream>
#include <sstream>
#include <string>

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace {

/// The queries the grammar ships are taken by the grammar itself (REQ-00052-1): tree-sitter compiles
/// each against the language, and each finds something in a specification.
class TestGrammarQueries : public ::testing::Test {
 protected:
  static std::string read(const std::string& path) {
    std::ifstream in(path);
    EXPECT_TRUE(in.good()) << path;
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
  }

  /// How many captures a query of the grammar finds in a specification; -1 when the grammar refuses it.
  static int captures(const std::string& queryFile, const std::string& specification) {
    const std::string source = read(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/queries/" + queryFile);
    uint32_t offset = 0;
    TSQueryError error = TSQueryErrorNone;
    TSQuery* query = ts_query_new(tree_sitter_driftspec(), source.c_str(), static_cast<uint32_t>(source.size()), &offset, &error);
    if (!query) {
      ADD_FAILURE() << queryFile << " is refused at byte " << offset << " (error " << error << ")";
      return -1;
    }
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_driftspec());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, specification.c_str(), static_cast<uint32_t>(specification.size()));
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
    int found = 0;
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) found += match.capture_count;
    ts_query_cursor_delete(cursor);
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    ts_query_delete(query);
    return found;
  }

  const std::string greeter = read(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola/core/Greeter.ds");
};

TEST_F(TestGrammarQueries, TheHighlightQueryIsTakenByTheGrammarAndColoursASpecification) {
  EXPECT_GT(captures("highlights.scm", greeter), 0);
}

TEST_F(TestGrammarQueries, TheFoldQueryIsTakenByTheGrammarAndFoldsASpecification) { EXPECT_GT(captures("folds.scm", greeter), 0); }

}  // namespace
