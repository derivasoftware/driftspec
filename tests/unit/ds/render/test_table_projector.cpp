#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/TableProjector.hpp"

namespace {

/// A machine as a table (DS-251, DS-369): its events down, its states across, and no node.
class TestTableProjector : public ::testing::Test {
 protected:
  /// The diagram `name` of tests/corpus/`corpus`, its model read and checked.
  const ds::model::Entity& diagram(const std::string& corpus, const std::string& name) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/" + corpus)) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    return *model.find("diagram:" + name);
  }

  static const ds::render::IrNode* node(const ds::render::IrDocument& body, const std::string& id) {
    for (const auto& n : body.nodes) {
      if (n.id == id) return &n;
    }
    return nullptr;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestTableProjector, TheTableHoldsTheMachineAndTheDrawingNoNode) {
  const ds::render::IrDocument body = ds::render::TableProjector(model, metamodel).project(diagram("states", "TB_Order"));
  ASSERT_TRUE(body.table.has_value());
  EXPECT_FALSE(body.table->columns.empty());
  EXPECT_FALSE(body.table->rows.empty());
  EXPECT_TRUE(body.nodes.empty());
  EXPECT_TRUE(body.edges.empty());
  EXPECT_TRUE(body.kind.empty());  // the head is IrExport's: a projector writes the body alone
  EXPECT_TRUE(body.title.empty());
}
}  // namespace
