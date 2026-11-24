#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The order of the findings and the coverage (REQ-00193-1, DS-382): by where they are written, then by what they say,
/// never by the order the steps of the check ran in.
class TestFindingOrder : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;

  using Key = std::tuple<std::string, int, std::string, std::string>;
  static Key keyOf(const ds::model::Finding& f) { return {f.location.file, f.location.line, f.code, f.message}; }
  static bool ordered(const std::vector<ds::model::Finding>& list) {
    for (std::size_t i = 1; i < list.size(); ++i)
      if (keyOf(list[i]) < keyOf(list[i - 1])) return false;
    return true;
  }
};

TEST_F(TestFindingOrder, TheFindingsAndTheCoverageOfADesignAreInTheOrderOfWhereAndWhat) {
  ds::parse::Parser parser;
  // the audit and the levels corpora have findings and coverage from several steps, some on one line
  for (const char* corpus : {"/audit", "/levels"}) {
    const ds::model::Model model = ds::api::Api(metamodel).load(parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + corpus));
    ASSERT_FALSE(model.findings().empty()) << corpus;
    ASSERT_FALSE(model.coverage().empty()) << corpus;
    EXPECT_TRUE(ordered(model.findings())) << corpus;
    EXPECT_TRUE(ordered(model.coverage())) << corpus;
  }
}

TEST_F(TestFindingOrder, TheOrderTheyWereReportedInLeavesNoTrace) {
  const ds::model::Location line{"a.ds", 3, 1};
  const std::vector<ds::model::Finding> reported{
      metamodel.finding(ds::model::Code::W_FIELD_UNKNOWN, "b", line), metamodel.finding(ds::model::Code::E_SYNTAX, "x", {"a.ds", 4, 1}),
      metamodel.finding(ds::model::Code::W_FIELD_UNKNOWN, "a", line), metamodel.finding(ds::model::Code::E_DUPLICATE_IDENTITY, "z", line)};
  std::vector<Key> orders[2];
  for (int pass = 0; pass < 2; ++pass) {
    ds::model::Model model{metamodel};
    for (std::size_t i = 0; i < reported.size(); ++i) model.report(reported[pass ? reported.size() - 1 - i : i]);
    model.sortFindings();
    for (const auto& f : model.findings()) orders[pass].push_back(keyOf(f));
  }
  EXPECT_EQ(orders[0], orders[1]);
  ASSERT_EQ(orders[0].size(), 4u);
  EXPECT_EQ(std::get<2>(orders[0][0]), "E-DUPLICATE-IDENTITY");  // on line 3: by code, then by message
  EXPECT_EQ(std::get<3>(orders[0][1]), "a");
  EXPECT_EQ(std::get<3>(orders[0][2]), "b");
  EXPECT_EQ(std::get<1>(orders[0][3]), 4);
}

}  // namespace
