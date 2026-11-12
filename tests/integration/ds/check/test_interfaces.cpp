#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The external interfaces of a process and how they go down, over examples/interfaces.
class TestInterfaces : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/interfaces")) {
      syntaxErrors += f.hasSyntaxErrors ? 1 : 0;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    for (const auto& f : model.findings()) lines[f.code].push_back(f.message);
  }

  bool reported(const std::string& code, const std::string& text) const {
    const auto found = lines.find(code);
    if (found == lines.end()) return false;
    for (const auto& line : found->second) {
      if (line.find(text) != std::string::npos) return true;
    }
    return false;
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all()) {
      if (r.kind == kind && r.from == from && r.to == to) return true;
    }
    return false;
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
  }

  ds::model::Model model;
  std::map<std::string, std::vector<std::string>> lines;
  int syntaxErrors = 0;
};

TEST_F(TestInterfaces, AnInterfaceHoldsItsTypedOperationsAndTheirFields) {
  EXPECT_EQ(syntaxErrors, 0);
  const auto* op = model.find("shop@shell::buy");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->kind, ds::model::Kind::Operation);
  EXPECT_EQ(op->fields.at("params"), "sku: string");
  EXPECT_EQ(op->fields.at("returns"), "int");
  EXPECT_EQ(op->fields.at("command"), "shop buy <sku>");
  EXPECT_EQ(op->fields.at("exit 2"), "refused");
}

TEST_F(TestInterfaces, RequirementsAndConstraintsLinkToAnInterfaceOrAnOperation) {
  EXPECT_TRUE(related("requires", "shop@shell", "SREQ-00002-1"));
  EXPECT_TRUE(related("requires", "shop@shell::buy", "SREQ-00001-1"));
}

TEST_F(TestInterfaces, AnOperationIsTheSubjectOfItsSequenceAndItsTraceGoesDown) {
  const auto* message = model.find("shop@shell::buy::sequence/refused::m0");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->fields.at("expands"), "shop@till::main::sequence/refused");
  EXPECT_TRUE(related("behaviour-of", "shop@shell::buy::sequence", "shop@shell::buy"));
}

TEST_F(TestInterfaces, AnOperationWithNoSequenceIsAFinding) {
  EXPECT_TRUE(reported("W-OPERATION-NO-BEHAVIOUR", "shop@shell::forgotten"));
  EXPECT_EQ(lines["W-OPERATION-NO-BEHAVIOUR"].size(), 1U);
}

TEST_F(TestInterfaces, AnOutcomeThatNamesNoScenarioIsAFinding) {
  EXPECT_TRUE(reported("W-SCENARIO-UNDECLARED", "exits 3 on 'stolen'"));
  EXPECT_EQ(lines["W-SCENARIO-UNDECLARED"].size(), 1U);
}

TEST_F(TestInterfaces, AnOutcomeIsCheckedAgainstTheValueItsTraceEndsIn) {
  EXPECT_TRUE(reported("W-OUTCOME-MISMATCH", "promises exit 7 on 'default', and that trace ends in 0"));
  EXPECT_FALSE(reported("W-OUTCOME-MISMATCH", "exit 2"));  // refused ends in 2, two calls down
  EXPECT_EQ(lines["W-OUTCOME-MISMATCH"].size(), 1U);
}

TEST_F(TestInterfaces, AViewIsAStateWithItsTextAndWhatBindsIt) {
  const auto* home = model.find("shop@web::state::Home");
  ASSERT_NE(home, nullptr);
  EXPECT_EQ(home->kind, ds::model::Kind::View);
  EXPECT_NE(home->fields.at("text").find("A field for the code"), std::string::npos);
  EXPECT_TRUE(related("requires", home->id, "SREQ-00001-1"));
  EXPECT_TRUE(related("constrained-by", home->id, "CST-00001-1"));
}

TEST_F(TestInterfaces, AnEventThatNamesAnOperationGoesDownAndAnyOtherIsALabel) {
  EXPECT_TRUE(related("on", "shop@web::state", "shop@web::buy"));
  EXPECT_EQ(lines.count("W-EVENT-UNDECLARED"), 0U);
}

TEST_F(TestInterfaces, TheNavigationReportsViewsWithNoWayInOrOut) {
  EXPECT_TRUE(reported("W-VIEW-UNREACHABLE", "the view Lost"));
  EXPECT_TRUE(reported("W-VIEW-NO-WAY-OUT", "the view End"));
  EXPECT_TRUE(reported("W-VIEW-UNDECLARED", "'Ghost'"));
  EXPECT_EQ(lines["W-VIEW-UNREACHABLE"].size(), 1U);
  EXPECT_EQ(lines["W-VIEW-NO-WAY-OUT"].size(), 1U);
}

TEST_F(TestInterfaces, AnInterfaceIsDrawnAsABoxWithItsOperations) {
  const auto ir = diagram("IF_Shell");
  ASSERT_EQ(ir["nodes"].size(), 1U);
  EXPECT_EQ(ir["nodes"][0]["id"], "shop@shell");
  EXPECT_EQ(ir["nodes"][0]["classifier"], "interface");
  EXPECT_NE(ir["nodes"][0].dump().find("buy(sku: string) -> int"), std::string::npos);
}

TEST_F(TestInterfaces, TheSequenceOfAnOperationIsDrawnDownThroughItsScenario) {
  const std::string drawn = diagram("SEQ_Buy").dump();
  EXPECT_NE(drawn.find("main(sku) / refused"), std::string::npos);
  EXPECT_NE(drawn.find("buy(sku: string)"), std::string::npos);
}

}  // namespace
