#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/SequenceProjector.hpp"

namespace {

/// A sequence diagram's body (DS-369): the lifelines a behaviour stands on and its messages, in order.
class TestSequenceProjector : public ::testing::Test {
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

TEST_F(TestSequenceProjector, TheCallerAndTheSubjectAreLifelinesAndTheMessagesAreOrdered) {
  const ds::render::IrDocument body = ds::render::SequenceProjector(model, metamodel).project(diagram("behaviour", "SEQ_Run"));
  ASSERT_NE(node(body, "caller"), nullptr);
  EXPECT_EQ(node(body, "caller")->classifier, "actor");
  ASSERT_NE(node(body, "ds@ds::Checker"), nullptr);
  EXPECT_EQ(node(body, "ds@ds::Checker")->kind, "lifeline");
  int order = 0;
  for (const auto& e : body.edges) {
    if (e.kind == "message") EXPECT_EQ(e.order, order++);
  }
  EXPECT_GT(order, 2);
  EXPECT_TRUE(body.kind.empty());  // the head is IrExport's: a projector writes the body alone
  EXPECT_TRUE(body.title.empty());
}
}  // namespace
