#include <gtest/gtest.h>

#include <map>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"

namespace {

/// Block 1: requirements, constraints and verifications as decided, over examples/artefacts (its `!` lines are the oracle).
class TestArtefacts : public ::testing::Test {
 protected:
  driftspec::model::Model model;
  std::map<std::string, int> byCode;

  void SetUp() override {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/artefacts")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    driftspec::check::Checker(model).run(binder);
    for (const auto& f : model.findings()) ++byCode[f.code];
  }

  const driftspec::model::Relation* relation(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.relations()) {
      if (r.kind == kind && r.from == from && r.to == to) return &r;
    }
    return nullptr;
  }

  std::string field(const std::string& id, const std::string& key) const {
    const driftspec::model::Entity* e = model.find(id);
    if (!e) return "<no entity " + id + ">";
    auto it = e->fields.find(key);
    return it == e->fields.end() ? "" : it->second;
  }
};

TEST_F(TestArtefacts, TheOracleFindingsAndNothingElse) {
  EXPECT_EQ(byCode["W-REQ-NO-VERIFICATION"], 1);  // SREQ-00002-1
  EXPECT_EQ(byCode["W-REVISION-DRIFT"], 1);       // REQ-00002-1 -> SREQ-00001-1
  EXPECT_EQ(byCode["E-REF-UNPINNED"], 1);         // REQ-00003-1 -> SREQ-00001
  EXPECT_EQ(byCode["W-PARENT-IGNORED"], 1);       // REQ-00003-1
  EXPECT_EQ(byCode["W-VER-NO-TEST"], 1);          // VER-00004-3
  EXPECT_EQ(byCode["W-REF-UNDECLARED"], 1);       // Report
  EXPECT_EQ(model.findings().size(), 6u);
}

TEST_F(TestArtefacts, TheRevisionIsPartOfTheIdentityAndLinksNameIt) {
  ASSERT_NE(model.find("SREQ-00001-2"), nullptr);
  EXPECT_EQ(model.find("SREQ-00001"), nullptr);
  EXPECT_NE(relation("refines", "REQ-00001-2", "SREQ-00001-2"), nullptr);
  // a drifted link still lands on the declared revision, so the graph stays whole
  EXPECT_NE(relation("refines", "REQ-00002-1", "SREQ-00001-2"), nullptr);
  // an unpinned link creates nothing: no relation, not even an expected node
  EXPECT_EQ(relation("refines", "REQ-00003-1", "SREQ-00001-2"), nullptr);
  EXPECT_EQ(model.find("SREQ-00001"), nullptr);
}

TEST_F(TestArtefacts, LevelByPlacementAndNestingGivesTheRelation) {
  EXPECT_EQ(model.find("SREQ-00001-2")->scope, "iris@");          // in the package: system level
  EXPECT_EQ(model.find("REQ-00001-2")->scope, "iris@projection");  // in a module: software level
  EXPECT_NE(relation("refines", "SREQ-00004-1", "SREQ-00001-2"), nullptr);   // requirement nested in a requirement
  EXPECT_NE(relation("verifies", "VER-00001-3", "REQ-00001-2"), nullptr);    // verification nested in a requirement
  EXPECT_NE(relation("verifies", "VER-00050-1", "SREQ-00001-2"), nullptr);   // verification elsewhere, @requirement
  EXPECT_NE(relation("about", "CST-00002-1", "iris"), nullptr);
  EXPECT_NE(relation("requires", "iris@projection::Projection", "REQ-00001-2"), nullptr);
  EXPECT_NE(relation("constrained-by", "iris@projection::Projection", "CST-00002-1"), nullptr);
}

TEST_F(TestArtefacts, FieldsAsDecided) {
  EXPECT_EQ(field("SREQ-00003-1", "title"), "The system shall report the stamps of the last activity");  // one-line form
  EXPECT_EQ(field("SREQ-00003-1", "status"), "");                                                          // draft by absence
  EXPECT_EQ(field("SREQ-00001-2", "tags"), "projection, tracker");
  EXPECT_EQ(field("SREQ-00001-2", "asil"), "");                                                            // no asil: tags are the user's
  EXPECT_EQ(field("VER-00001-3", "method"), "test");
  EXPECT_NE(field("VER-00001-3", "tests").find("test_it_req00003_namespacing.py::"), std::string::npos);
  EXPECT_NE(field("VER-00050-1", "evidence").find("doc/reviews/"), std::string::npos);
  EXPECT_EQ(field("CST-00002-1", "category"), "architecture");
  EXPECT_EQ(field("CST-00002-1", "bok"), "BOK-00002-1, BOK-00007-1");
}

TEST_F(TestArtefacts, TheSeparateFileIsTheNormalPlace) {
  EXPECT_NE(relation("refines", "REQ-00005-1", "SREQ-00001-2"), nullptr);
  EXPECT_NE(relation("verifies", "VER-00006-1", "REQ-00005-1"), nullptr);
  EXPECT_EQ(model.find("REQ-00005-1")->scope, "iris@projection");
}

}  // namespace
