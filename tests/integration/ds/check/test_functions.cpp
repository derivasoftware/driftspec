#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Functions of a module (F1-F5) over tests/corpus/functions/functions.ds, whose `!` comments are the oracle.
class TestFunctions : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::map<std::string, int> byCode;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/functions")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    for (const auto& f : model.findings()) ++byCode[f.code];
  }

  std::string field(const std::string& id, const std::string& key) const {
    const ds::model::Entity* e = model.find(id);
    if (!e) return "<no entity " + id + ">";
    auto it = e->fields.find(key);
    return it == e->fields.end() ? "" : it->second;
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all()) {
      if (r.kind == kind && r.from == from && r.to == to) return true;
    }
    return false;
  }

  nlohmann::json render(const std::string& diagram) const {
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false));
  }
};

TEST_F(TestFunctions, TheOracleFindingsAndNothingElse) {
  EXPECT_EQ(byCode["E-DUPLICATE-IDENTITY"], 1);  // describe twice: a function is identified by its name
  EXPECT_EQ(byCode["E-MSG-ARITY"], 1);
  EXPECT_EQ(byCode["W-MSG-UNDECLARED"], 0);  // summarise and cache nobody designs: text (DS-18)
  EXPECT_EQ(model.findings().size(), 2u);
}

TEST_F(TestFunctions, AFunctionIsAnOperationOfItsModule) {
  const ds::model::Entity* gather = model.find("iris@tasks::gather");
  ASSERT_NE(gather, nullptr);
  EXPECT_EQ(gather->kind, ds::model::Kind::Function);
  EXPECT_EQ(gather->scope, "iris@tasks");
  EXPECT_TRUE(related("contains", "iris@tasks", "iris@tasks::gather"));
  EXPECT_EQ(field("iris@tasks::gather", "params"), "board: Board, project: std::string");
  EXPECT_EQ(field("iris@tasks::gather", "returns"), "std::vector<Task>");
  EXPECT_EQ(field("iris@tasks::gather", "visibility"), "+");  // public when nothing is written
  EXPECT_EQ(field("iris@tasks::_row_of", "visibility"), "-");
  EXPECT_EQ(model.find("iris@requests::get"), nullptr);  // F5: what nobody designs is not declared
}

TEST_F(TestFunctions, ItsSignatureGivesUseAndItsBodyIsAMethods) {
  EXPECT_TRUE(related("uses", "iris@tasks::gather", "iris@tasks::Board"));
  EXPECT_TRUE(related("uses", "iris@tasks::gather", "iris@tasks::Task"));  // through std::vector<Task>
  EXPECT_TRUE(related("satisfies", "iris@tasks::gather", "REQ-00900-1"));
  EXPECT_EQ(field("iris@tasks::gather", "note"), "the one door to the board");
}

TEST_F(TestFunctions, ItsBehaviourRunsOnItsParametersAndCallsItsNeighbours) {
  ASSERT_NE(model.find("iris@tasks::gather::sequence"), nullptr);
  EXPECT_TRUE(related("calls", "iris@tasks::gather::sequence::m0", "iris@tasks::Board::issues"));  // a parameter
  EXPECT_TRUE(related("calls", "iris@tasks::gather::sequence::m1", "iris@tasks::_row_of"));        // a neighbour
  // a method reaches a function of its module, and one of another module by its name
  EXPECT_TRUE(related("calls", "iris@cli::Command::run::sequence::m0", "iris@cli::main"));
  EXPECT_TRUE(related("calls", "iris@cli::Command::run::sequence::m1", "iris@tasks::gather"));
  EXPECT_TRUE(related("expands-to", "iris@cli::Command::run::sequence::m1", "iris@tasks::gather::sequence"));
  EXPECT_TRUE(related("couples-transient", "iris@cli::Command::run::sequence", "iris@tasks::gather"));
}

TEST_F(TestFunctions, AClassDiagramDrawsEachFunctionAsABox) {
  const nlohmann::json ir = render("CL_Tasks");
  const nlohmann::json* gather = nullptr;
  for (const auto& n : ir["nodes"]) {
    if (n["id"] == "iris@tasks::gather") gather = &n;
  }
  ASSERT_NE(gather, nullptr);
  EXPECT_EQ((*gather)["classifier"], "function");
  EXPECT_EQ((*gather)["label"], "gather");
  EXPECT_EQ((*gather)["parent"], "iris@tasks");
  bool uses = false;
  for (const auto& e : ir["edges"])
    uses |= e["from"] == "iris@tasks::gather" && e["to"] == "iris@tasks::Board" && e["kind"] == "dependency";
  EXPECT_TRUE(uses);
}

TEST_F(TestFunctions, ASequenceOfAFunctionRunsOnItsModule) {
  const nlohmann::json ir = render("SEQ_Gather");
  bool module = false, entry = false;
  for (const auto& n : ir["nodes"]) module |= n["id"] == "iris@tasks" && n["label"] == "tasks";
  for (const auto& e : ir["edges"]) entry |= e["from"] == "caller" && e["to"] == "iris@tasks";
  EXPECT_TRUE(module);
  EXPECT_TRUE(entry);
}

}  // namespace
