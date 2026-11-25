#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/ClassProjector.hpp"

namespace {

/// A class diagram's body (DS-369): the boxes a diagram takes, and the cut it is lifted onto.
class TestClassProjector : public ::testing::Test {
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

TEST_F(TestClassProjector, TheBoxesADiagramIncludesAreItsNodesInTheirModule) {
  const nlohmann::json body = ds::render::ClassProjector(model, metamodel).project(diagram("static", "CL_Model"));
  ASSERT_NE(node(body, "ds@ds"), nullptr);
  EXPECT_EQ((*node(body, "ds@ds"))["kind"], "container");
  ASSERT_NE(node(body, "ds@ds::Check"), nullptr);
  EXPECT_EQ((*node(body, "ds@ds::Check"))["parent"], "ds@ds");
  EXPECT_FALSE(body.contains("ir"));  // the head is IrExport's: a projector writes the body alone
  EXPECT_FALSE(body.contains("title"));
}

TEST_F(TestClassProjector, ACutWrittenClosesTheBoxesItSaysAndWhatTheyHoldIsNotDrawn) {
  const nlohmann::json body = ds::render::ClassProjector(model, metamodel).project(diagram("cut", "HLD_Open"));
  ASSERT_NE(node(body, "shop@bank"), nullptr);
  EXPECT_EQ((*node(body, "shop@bank"))["kind"], "box");  // closed: one box, its classifier its kind
  EXPECT_EQ((*node(body, "shop@bank"))["classifier"], "package");
  ASSERT_NE(node(body, "shop@sales"), nullptr);
  EXPECT_EQ((*node(body, "shop@sales"))["kind"], "container");  // open: drawn around what it holds
  EXPECT_FALSE(body.contains("ir"));                            // the head is IrExport's: a projector writes the body alone
  EXPECT_FALSE(body.contains("title"));
}
}  // namespace
