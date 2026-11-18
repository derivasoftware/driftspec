#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" const TSLanguage* tree_sitter_driftspec(void);

namespace {

/// The grammar's corpus (grammar/test/corpus, the format `tree-sitter test` reads): each case is a
/// text and the tree the grammar gives it, written without fields. The grammar parses every case to
/// the tree it records, and the cases reach every named node it defines (REQ-00051-1, DS-7).
class TestGrammarCorpus : public ::testing::Test {
 protected:
  struct Case {
    std::string file;
    std::string name;
    std::string input;
    std::string tree;
  };

  static std::string corpusDir() { return std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/test/corpus"; }

  static std::string read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.good()) << path;
    std::stringstream text;
    text << in.rdbuf();
    std::string out = text.str();
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());  // a Windows checkout ends lines in CRLF
    return out;
  }

  /// Every case of every corpus file: a header between lines of `=`, the text, `---`, the tree.
  static std::vector<Case> cases() {
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(corpusDir()))
      if (e.path().extension() == ".txt") files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    std::vector<Case> all;
    const std::regex header(R"((^|\n)=+\n([^\n]+)\n=+\n)");
    for (const auto& file : files) {
      const std::string text = read(file);
      std::vector<std::smatch> heads;
      for (std::sregex_iterator it(text.begin(), text.end(), header), end; it != end; ++it) heads.push_back(*it);
      for (std::size_t i = 0; i < heads.size(); ++i) {
        const auto from = heads[i].position(0) + heads[i].length(0);
        const auto to = i + 1 < heads.size() ? static_cast<std::size_t>(heads[i + 1].position(0)) : text.size();
        const std::string body = text.substr(from, to - from);
        const auto split = body.find("\n---\n");
        if (split == std::string::npos) {
          ADD_FAILURE() << file << ": a case without its tree: " << heads[i][2];
          continue;
        }
        all.push_back({std::filesystem::path(file).filename().string(), heads[i][2], body.substr(0, split + 1), body.substr(split + 5)});
      }
    }
    return all;
  }

  /// A tree written the way the corpus writes it: fields left out, one space between tokens.
  static std::string normal(std::string tree) {
    tree = std::regex_replace(tree, std::regex(R"([a-z_]+: )"), "");
    tree = std::regex_replace(tree, std::regex(R"(\s+)"), " ");
    tree = std::regex_replace(tree, std::regex(R"(\( )"), "(");
    tree = std::regex_replace(tree, std::regex(R"( \))"), ")");
    const auto first = tree.find_first_not_of(' ');
    const auto last = tree.find_last_not_of(' ');
    return first == std::string::npos ? "" : tree.substr(first, last - first + 1);
  }

  static std::string parse(const std::string& input) {
    std::unique_ptr<TSParser, decltype(&ts_parser_delete)> parser(ts_parser_new(), ts_parser_delete);
    ts_parser_set_language(parser.get(), tree_sitter_driftspec());
    std::unique_ptr<TSTree, decltype(&ts_tree_delete)> tree(
        ts_parser_parse_string(parser.get(), nullptr, input.c_str(), static_cast<uint32_t>(input.size())), ts_tree_delete);
    char* text = ts_node_string(ts_tree_root_node(tree.get()));
    std::string out(text);
    free(text);
    return out;
  }
};

TEST_F(TestGrammarCorpus, EveryCaseParsesToTheTreeItRecords) {
  const auto all = cases();
  ASSERT_FALSE(all.empty());
  for (const auto& c : all) {
    const std::string got = normal(parse(c.input));
    EXPECT_EQ(got.find("ERROR"), std::string::npos) << c.file << ": " << c.name;
    EXPECT_EQ(got.find("MISSING"), std::string::npos) << c.file << ": " << c.name;
    EXPECT_EQ(got, normal(c.tree)) << c.file << ": " << c.name;
  }
}

TEST_F(TestGrammarCorpus, TheCasesReachEveryNamedNodeOfTheGrammar) {
  std::ifstream types(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/src/node-types.json");
  ASSERT_TRUE(types.good());
  std::set<std::string> named;
  for (const auto& n : nlohmann::json::parse(types))
    if (n.value("named", false)) named.insert(n["type"].get<std::string>());
  std::set<std::string> reached;
  const std::regex node(R"(\(([a-z_]+))");
  for (const auto& c : cases()) {
    const std::string tree = c.tree;
    for (std::sregex_iterator it(tree.begin(), tree.end(), node), end; it != end; ++it) reached.insert((*it)[1]);
  }
  for (const auto& n : named) EXPECT_TRUE(reached.count(n)) << "no case of the corpus reaches " << n;
}

}  // namespace
