#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/axis/Cut.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/SequenceBuilder.hpp"

namespace {

/// What a sequence diagram is built of (DS-369): one lifeline per key, whatever asks for it, and the messages it emits.
class TestSequenceBuilder : public ::testing::Test {
 protected:
  /// The diagram `name` of tests/corpus/`corpus`, its model read and checked.
  const ds::model::Entity& diagram(const std::string& corpus, const std::string& name) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/" + corpus)) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    return *model.find("diagram:" + name);
  }

  static const nlohmann::json* node(const nlohmann::json& body, const std::string& id) {
    for (const auto& n : body["nodes"]) {
      if (n["id"] == id) return &n;
    }
    return nullptr;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestSequenceBuilder, ALifelineIsMadeOnceForItsKey) {
  const ds::model::Entity& seq = diagram("behaviour", "SEQ_Run");
  const ds::axis::Cut cut(model, metamodel, seq);
  ds::render::SequenceBuilder builder(model, cut);
  EXPECT_EQ(builder.lifeline("caller", "caller", "actor"), "caller");
  EXPECT_EQ(builder.lifeline("caller", "someone else", "participant"), "caller");
  ASSERT_EQ(builder.nodes.size(), 1u);
  EXPECT_EQ(builder.nodes[0]["label"], "caller");  // the first asking names it
  EXPECT_EQ(builder.shortName("ds@ds::Checker::run"), "run");
  EXPECT_EQ(builder.shortName("run"), "run");
}

TEST_F(TestSequenceBuilder, AParticipantNamedAsItsTypeSaysItOnceAndOtherwiseBoth) {
  const ds::model::Entity& seq = diagram("behaviour", "SEQ_Run");
  const ds::axis::Cut cut(model, metamodel, seq);
  ds::render::SequenceBuilder builder(model, cut);
  EXPECT_EQ(builder.lineOf("ds@ds::Model", "model", "ds@ds::Checker::run"), "ds@ds::Model|model");
  EXPECT_EQ(builder.nodes[0]["label"], "model: Model");
  EXPECT_EQ(builder.lineOf("ds@ds::Model", "Model", "ds@ds::Checker::run"), "ds@ds::Model|Model");
  EXPECT_EQ(builder.nodes[1]["label"], "Model");
  EXPECT_EQ(builder.closedAround("ds@ds::Model"), "");  // no cut written: no box is closed
}
}  // namespace
