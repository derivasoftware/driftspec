#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A sequence calls a class's static operation by its qualified name, with no instance (REQ-00108-1, DS-60).
class TestStaticCalls : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  module diff {\n"
                                       "    class Diff {\n"
                                       "      + static compare(a: int, b: int) -> int\n"
                                       "      + plain(a: int) -> int\n"
                                       "    }\n"
                                       "  }\n"
                                       "  module app {\n"
                                       "    class App {\n"
                                       "      + run() -> int\n"
                                       "    }\n"
                                       "    sequence App::run {\n"
                                       "      -> diff::Diff::compare(1, 2) -> c\n"
                                       "      -> diff::Diff::plain(1) -> d\n"
                                       "      return c\n"
                                       "    }\n"
                                       "    function main() -> int\n"
                                       "    sequence main {\n"
                                       "      -> diff::Diff::compare(3, 4) -> r\n"
                                       "      return r\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors);
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  bool calls(const std::string& message, const std::string& op) const {
    for (const auto& r : model.index().all())
      if (r.kind == "calls" && r.from == message && r.to == op) return true;
    return false;
  }

  int reported(const std::string& code) const {
    int n = 0;
    for (const auto& f : model.findings()) n += f.code == code;
    return n;
  }
};

TEST_F(TestStaticCalls, AMethodsBehaviourCallsAStaticOperationOnItsClass) {
  EXPECT_TRUE(calls("p@app::App::run::sequence::m0", "p@diff::Diff::compare"));
}

TEST_F(TestStaticCalls, AFunctionsBehaviourCallsAStaticOperationOnItsClass) {
  EXPECT_TRUE(calls("p@app::main::sequence::m0", "p@diff::Diff::compare"));
}

TEST_F(TestStaticCalls, AnOperationThatIsNotStaticNeedsAnInstance) {
  EXPECT_FALSE(calls("p@app::App::run::sequence::m1", "p@diff::Diff::plain"));
  EXPECT_EQ(reported("W-MSG-NOT-STATIC"), 1);
  EXPECT_EQ(reported("W-MSG-UNDECLARED"), 0);
  EXPECT_EQ(static_cast<int>(model.findings().size()), 1);
}

}  // namespace
