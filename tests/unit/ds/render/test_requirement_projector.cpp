#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/RequirementProjector.hpp"

namespace {

/// A requirement diagram's body (DS-369): the requirements, each a box, and what answers to each.
class TestRequirementProjector : public ::testing::Test {
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

TEST_F(TestRequirementProjector, EachRequirementIsABoxOfItsKind) {
  const nlohmann::json body = ds::render::RequirementProjector(model, metamodel).project(diagram("artefacts", "REQ_Tracker"));
  ASSERT_NE(node(body, "REQ-00001-2"), nullptr);
  EXPECT_EQ((*node(body, "REQ-00001-2"))["kind"], "box");
  EXPECT_EQ((*node(body, "REQ-00001-2"))["classifier"], "requirement");
  EXPECT_FALSE(body["edges"].empty());
  EXPECT_FALSE(body.contains("ir"));  // the head is IrExport's: a projector writes the body alone
  EXPECT_FALSE(body.contains("title"));
}
}  // namespace
