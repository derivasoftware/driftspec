#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/TourProjector.hpp"

namespace {

/// A tour's body (DS-245, DS-369): its boxes and its sequence, two parts of one drawing.
class TestTourProjector : public ::testing::Test {
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

TEST_F(TestTourProjector, EveryNodeSaysWhichOfTheTwoPartsItIsOf) {
  const nlohmann::json body = ds::render::TourProjector(model, metamodel).project(diagram("cut", "TOUR_Modules"));
  EXPECT_EQ(body["parts"], nlohmann::json::array({"boxes", "sequence"}));
  int boxes = 0, lifelines = 0;
  for (const auto& n : body["nodes"]) {
    boxes += n["part"] == "boxes" ? 1 : 0;
    lifelines += n["part"] == "sequence" ? 1 : 0;
  }
  EXPECT_GT(boxes, 0);
  EXPECT_GT(lifelines, 0);
  EXPECT_EQ(boxes + lifelines, static_cast<int>(body["nodes"].size()));
  ASSERT_NE(node(body, "shop@sales@desk"), nullptr);
  EXPECT_EQ((*node(body, "shop@sales@desk"))["part"], "boxes");
  EXPECT_FALSE(body.contains("ir"));  // the head is IrExport's: a projector writes the body alone
  EXPECT_FALSE(body.contains("title"));
}
}  // namespace
