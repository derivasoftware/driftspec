#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <string>

namespace {

/// The grammar ships highlight and fold queries that name only nodes it defines (REQ-00052-1).
class TestParser : public ::testing::Test {};

TEST_F(TestParser, TheQueriesNameOnlyNodesTheGrammarDefines) {
  std::ifstream types(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/src/node-types.json");
  ASSERT_TRUE(types.good());
  const auto nodes = nlohmann::json::parse(types);
  std::set<std::string> named;
  for (const auto& n : nodes) {
    if (n.value("named", false)) named.insert(n["type"].get<std::string>());
  }
  for (const char* query : {"highlights.scm", "folds.scm"}) {
    std::ifstream q(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/queries/" + query);
    ASSERT_TRUE(q.good()) << query;
    std::string raw((std::istreambuf_iterator<char>(q)), std::istreambuf_iterator<char>());
    std::string text;  // without the comment lines
    for (size_t at = 0; at < raw.size();) {
      const size_t end = raw.find('\n', at);
      const std::string line = raw.substr(at, end == std::string::npos ? std::string::npos : end - at);
      if (line.rfind(";", 0) != 0) text += line + "\n";
      at = end == std::string::npos ? raw.size() : end + 1;
    }
    const std::regex node(R"(\(([a-z_]+)[\s)])");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), node); it != std::sregex_iterator(); ++it) {
      EXPECT_TRUE(named.count((*it)[1].str())) << query << " names a node the grammar does not define: " << (*it)[1].str();
    }
  }
}

}  // namespace
