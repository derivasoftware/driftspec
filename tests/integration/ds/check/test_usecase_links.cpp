#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A use case is an artefact identified by its id with its revision (REQ-00043-1, DS-116): a link to it,
/// from the operation that realises it or the use case that specialises it, is pinned and follows it.
class TestUseCaseLinks : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  usecase UC-00001-2 greet(name: string) -> string {\n"
                                       "    title: Greet a person by name\n"
                                       "    outcome default: the greeting\n"
                                       "  }\n"
                                       "  usecase UC-00002-1 greet(name: string) -> string : UC-00001 {\n"
                                       "    title: Greet from a page\n"
                                       "  }\n"
                                       "  interface shell {\n"
                                       "    kind: cli\n"
                                       "    operation greet(name: string) -> string : UC-00001-1\n"
                                       "  }\n"
                                       "  sequence shell::greet {\n"
                                       "    return name\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::vector<std::string> codes(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all())
      if (r.kind == kind && r.from == from && r.to == to) return true;
    return false;
  }
};

TEST_F(TestUseCaseLinks, ARealisationBehindTheRevisionIsReportedAndFollowsIt) {
  ASSERT_EQ(codes("W-REVISION-DRIFT").size(), 1u);
  EXPECT_NE(codes("W-REVISION-DRIFT")[0].find("UC-00001-1"), std::string::npos);
  EXPECT_TRUE(related("realises", "p@shell::greet", "UC-00001-2"));  // the link goes to the declared revision
  EXPECT_FALSE(related("realises", "p@shell::greet", "UC-00001-1"));
}

TEST_F(TestUseCaseLinks, ASpecialisationThatNamesNoRevisionIsUnpinned) {
  ASSERT_EQ(codes("E-REF-UNPINNED").size(), 1u);
  EXPECT_NE(codes("E-REF-UNPINNED")[0].find("UC-00002-1 refers to UC-00001"), std::string::npos);
}

TEST(TestUseCaseLinksAlone, AUseCaseNobodyDeclaresIsSaidOnce) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  interface shell {\n"
                               "    kind: cli\n"
                               "    operation greet(name: string) -> string : UC-00009-1\n"
                               "  }\n"
                               "  sequence shell::greet {\n"
                               "    return name\n"
                               "  }\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  ASSERT_EQ(model.findings().size(), 1u);
  EXPECT_EQ(model.findings()[0].code, "W-USECASE-UNDECLARED");
}

}  // namespace
