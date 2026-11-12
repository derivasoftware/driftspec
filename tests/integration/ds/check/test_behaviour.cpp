#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Block 3: behaviour bound to the static model, over examples/behaviour (its `!` lines are the oracle).
class TestBehaviour : public ::testing::Test {
 protected:
  ds::model::Model model;
  std::map<std::string, int> byCode;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/behaviour")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    for (const auto& f : model.findings()) ++byCode[f.code];
  }

  const ds::model::Relation* relation(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all()) {
      if (r.kind == kind && r.from == from && r.to == to) return &r;
    }
    return nullptr;
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
  }
};

TEST_F(TestBehaviour, TheOracleFindings) {
  EXPECT_EQ(byCode["W-PARTICIPANT-UNDECLARED"], 1);  // binder in checkMessage
  EXPECT_EQ(byCode["E-MSG-ARITY"], 1);               // Model::find(id, scope)
  EXPECT_EQ(byCode["W-ACTION-UNDECLARED"], 1);       // / report is Model's
  EXPECT_EQ(byCode["W-MSG-UNDECLARED"], 0);
  EXPECT_EQ(byCode["E-DUPLICATE-IDENTITY"], 1);  // the default scenario of checkMessage declared again from another module
}

TEST_F(TestBehaviour, ABehaviourIsIdentifiedByItsSubject) {
  ASSERT_NE(model.find("ds@ds::Checker::run::sequence"), nullptr);
  ASSERT_NE(model.find("ds@ds::Checker::resolveReference::sequence/duplicate-identity"), nullptr);
  // written in module scenarios, identified by its subject (B2): scenarios may live anywhere
  ASSERT_NE(model.find("ds@ds::Checker::resolveReference::sequence/timeout"), nullptr);
  EXPECT_EQ(model.find("ds@scenarios::ds::Checker::resolveReference::sequence/timeout"), nullptr);
  EXPECT_NE(relation("calls", "ds@ds::Checker::resolveReference::sequence/timeout::m0", "ds@ds::Checker::resolve"), nullptr);
  EXPECT_NE(relation("behaviour-of", "ds@ds::Checker::run::sequence", "ds@ds::Checker::run"), nullptr);
  EXPECT_NE(relation("behaviour-of", "ds@ds::Checker::state", "ds@ds::Checker"), nullptr);
}

TEST_F(TestBehaviour, ParticipantsComeFromTheStaticContext) {
  // self, the attribute `model`, the parameter `binder`: resolved without a participant line
  const ds::model::Entity* m0 = model.find("ds@ds::Checker::run::sequence::m0");
  ASSERT_NE(m0, nullptr);
  EXPECT_EQ(m0->fields.at("to"), "binder");
  EXPECT_EQ(m0->fields.at("type"), "ds@ds::Binder");
  EXPECT_NE(relation("calls", "ds@ds::Checker::run::sequence::m0", "ds@ds::Binder::references"), nullptr);
  const ds::model::Entity* m1 = model.find("ds@ds::Checker::resolveReference::sequence::m1");
  ASSERT_NE(m1, nullptr);
  EXPECT_EQ(m1->fields.at("type"), "ds@ds::Model");  // the attribute, by name
  EXPECT_NE(relation("calls", m1->id, "ds@ds::Model::declare"), nullptr);
}

TEST_F(TestBehaviour, AMessageToAnOperationWithABehaviourExpandsToIt) {
  EXPECT_NE(relation("expands-to", "ds@ds::Checker::run::sequence::m1", "ds@ds::Checker::resolveReference::sequence"), nullptr);
  EXPECT_EQ(model.find("ds@ds::Checker::run::sequence::m1")->fields.at("expands"), "ds@ds::Checker::resolveReference::sequence");
}

TEST_F(TestBehaviour, AnAltHoldsItsSidesAsBranchesAndEachSideItsSteps) {
  const std::string seq = "ds@ds::Checker::resolveReference::sequence";
  const ds::model::Entity* alt = model.find(seq + "::f0");
  ASSERT_NE(alt, nullptr);
  EXPECT_EQ(alt->fields.at("kind"), "alt");
  EXPECT_EQ(alt->fields.at("condition"), "target is empty");
  // the tree as written (DS-172): the sequence holds m0, the alt and m3; the alt its two sides; the first side its calls
  const std::vector<std::string> top = {seq + "::m0", seq + "::f0", seq + "::m3"};
  EXPECT_EQ(model.index().children(seq), top);
  const std::vector<std::string> sides = {seq + "::f0::b0", seq + "::f0::b1"};
  EXPECT_EQ(model.index().children(seq + "::f0"), sides);
  const std::vector<std::string> first = {seq + "::m1", seq + "::m2"};
  EXPECT_EQ(model.index().children(seq + "::f0::b0"), first);
  EXPECT_TRUE(model.index().children(seq + "::f0::b1").empty());  // an else with nothing in it
  const ds::model::SequenceTree tree(model, seq);
  EXPECT_EQ(tree.first(seq + "::f0"), seq + "::m1");
  EXPECT_EQ(tree.elseOf(seq + "::f0"), "");
  EXPECT_FALSE(tree.twoSides(seq + "::f0"));
}

TEST_F(TestBehaviour, EveryEntityKnowsWhereItEndsAsWellAsWhereItStarts) {
  // the place right after its last character, so a tool that edits the text needs no parser of its own (DS-172)
  const ds::model::Entity* alt = model.find("ds@ds::Checker::resolveReference::sequence::f0");
  ASSERT_NE(alt, nullptr);
  EXPECT_GT(alt->location.endLine, alt->location.line);  // the alt spans its two sides
  const ds::model::Entity* m = model.find("ds@ds::Checker::resolveReference::sequence::m0");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->location.endLine, m->location.line);  // a message is one line
  EXPECT_GT(m->location.endColumn, m->location.column);
}

TEST_F(TestBehaviour, CouplingIsDerivedFromWhatABehaviourTouchesOutsideItsModule) {
  EXPECT_NE(relation("couples-structural", "ds@lsp::Publisher::publish::sequence", "ds@ds::Model"), nullptr);
  EXPECT_NE(relation("couples-transient", "ds@lsp::Publisher::publish::sequence", "ds@ds::Checker"), nullptr);
  // inside ds nothing couples: cohesion
  for (const auto& r : model.index().all()) EXPECT_FALSE(r.kind.rfind("couples", 0) == 0 && r.from.rfind("ds@ds::", 0) == 0) << r.from;
}

TEST_F(TestBehaviour, StateEventsAndActionsAreOperations) {
  EXPECT_NE(relation("on", "ds@ds::Checker::state", "ds@ds::Checker::run"), nullptr);
  EXPECT_NE(relation("on", "ds@ds::Checker::state", "ds@ds::Checker::resolveReference"), nullptr);
}

TEST_F(TestBehaviour, ASequenceDiagramExpandsByDepthAndLinksAtDepthZero) {
  const nlohmann::json deep = diagram("SEQ_Run");
  int lifelines = 0, frames = 0;
  for (const auto& n : deep["nodes"]) {
    lifelines += n["kind"] == "lifeline";
    frames += n["kind"] == "frame";
  }
  EXPECT_EQ(lifelines, 6);  // caller, self, binder, model, and the unresolved binder/parser of checkMessage
  EXPECT_EQ(frames, 3);     // loop, alt (inlined), loop
  bool altSpansExpansion = false;
  for (const auto& n : deep["nodes"])
    altSpansExpansion |= n["kind"] == "frame" && n["label"] == "loop each reference" && n["span"][1] > n["span"][0];
  EXPECT_TRUE(altSpansExpansion);
  const nlohmann::json shallow = diagram("SEQ_Run_Overview");
  int links = 0;
  for (const auto& e : shallow["edges"]) links += e.contains("href");
  EXPECT_EQ(links, 2);                     // resolveReference and checkMessage are reachable, not inlined
  EXPECT_EQ(shallow["edges"].size(), 7u);  // the caller's run(binder) first; run returns none
  EXPECT_EQ(shallow["edges"][0]["from"], "caller");
  EXPECT_EQ(shallow["edges"][0]["label"], "run(binder: Binder)");
}

TEST_F(TestBehaviour, LiftingToModuleLevelKeepsOnlyCrossModuleMessages) {
  const nlohmann::json lifted = diagram("SEQ_Modules");
  EXPECT_EQ(lifted["nodes"].size(), 2u);  // lsp, ds
  EXPECT_EQ(lifted["edges"].size(), 2u);  // Model::find, Checker::resolve
  EXPECT_EQ(lifted["edges"][0]["from"], "ds@lsp");
  EXPECT_EQ(lifted["edges"][0]["to"], "ds@ds");
}

}  // namespace
