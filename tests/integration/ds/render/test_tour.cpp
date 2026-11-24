#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace {

/// A tour (DS-245): one drawing that holds the boxes that take part in a behaviour and the sequence between
/// them, on one cut. Over tests/corpus/cut, a purchase with its detail, and tests/corpus/levels, the same purchase
/// written between the boxes.
class TestTour : public ::testing::Test {
 protected:
  void SetUp() override {
    load(detail, "cut");
    load(written, "levels");
  }

  void load(ds::model::Model& model, const std::string& corpus) const {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/" + corpus)) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::string ir(const ds::model::Model& model, const std::string& name) const {
    return ds::render::IrExport(model, metamodel).render("diagram:" + name, false);
  }

  /// The boxes of the part that draws who takes part.
  static std::set<std::string> boxes(const nlohmann::json& tour) {
    std::set<std::string> out;
    for (const auto& n : tour["nodes"])
      if (n["part"] == "boxes" && n["kind"] == "box") out.insert(n["id"].get<std::string>());
    return out;
  }

  /// What the lifelines of the sequence part stand for, without whoever asks.
  static std::set<std::string> lifelines(const nlohmann::json& tour) {
    std::set<std::string> out;
    for (const auto& n : tour["nodes"]) {
      const std::string id = n["id"];
      if (n["part"] == "sequence" && n["kind"] == "lifeline" && id != "caller") out.insert(id.substr(0, id.find('|')));
    }
    return out;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model detail{metamodel};
  ds::model::Model written{metamodel};
};

TEST_F(TestTour, ATourIsOneDrawingOfTwoPartsTheBoxesAndTheSequence) {
  const auto tour = nlohmann::json::parse(ir(detail, "TOUR_Packages"));
  EXPECT_EQ(tour["parts"], nlohmann::json({"boxes", "sequence"}));  // in the order they are drawn
  for (const char* list : {"nodes", "edges"})
    for (const auto& item : tour[list]) EXPECT_TRUE(item["part"] == "boxes" || item["part"] == "sequence");  // each says which it is of
}

TEST_F(TestTour, EachLifelineOfTheSequenceIsOneBoxOfTheOtherPart) {
  for (const char* name : {"TOUR_Packages", "TOUR_Modules"}) {
    const auto tour = nlohmann::json::parse(ir(detail, name));
    EXPECT_EQ(boxes(tour), lifelines(tour)) << name;
  }
  EXPECT_EQ(boxes(nlohmann::json::parse(ir(detail, "TOUR_Packages"))),
            (std::set<std::string>{"shop@bank", "shop@sales", "shop@shell", "shop@warehouse"}));
}

TEST_F(TestTour, TheBoxesAreJoinedByTheRelationsBetweenThem) {
  const auto tour = nlohmann::json::parse(ir(detail, "TOUR_Packages"));
  std::multiset<std::string> salesToWarehouse;
  for (const auto& e : tour["edges"])
    if (e["part"] == "boxes" && e["from"] == "shop@sales" && e["to"] == "shop@warehouse")
      salesToWarehouse.insert(e["kind"].get<std::string>());
  EXPECT_EQ(salesToWarehouse, (std::multiset<std::string>{"association", "dependency"}));  // a structural bond and a use
}

TEST_F(TestTour, TheSequencePartIsTheSequenceDiagramOfTheSameFields) {
  const auto labels = [](const nlohmann::json& drawing, const std::string& part) {
    std::vector<std::string> out;
    for (const auto& e : drawing["edges"])
      if (part.empty() || e["part"] == part) out.push_back(e["label"].get<std::string>());
    return out;
  };
  EXPECT_EQ(labels(nlohmann::json::parse(ir(detail, "TOUR_Modules")), "sequence"),
            labels(nlohmann::json::parse(ir(detail, "SEQ_Modules")), ""));
}

TEST_F(TestTour, ABoxNamedInAScenarioTakesPartAsOneBoxWithNoCutWritten) {
  const auto tour = nlohmann::json::parse(ir(written, "TOUR_Purchase"));  // a scenario of the use case, and no close:
  EXPECT_EQ(boxes(tour), (std::set<std::string>{"shop@bank", "shop@sales", "shop@shell", "shop@warehouse"}));
  EXPECT_EQ(boxes(tour), lifelines(tour));
  bool joined = false;
  for (const auto& e : tour["edges"]) joined = joined || (e["part"] == "boxes" && e["from"] == "shop@sales" && e["to"] == "shop@warehouse");
  EXPECT_TRUE(joined);
}

TEST_F(TestTour, TheQueryShowsWhoTakesPart) {
  const auto filter = ds::query::FilterFactory::of(metamodel, *detail.find("diagram:TOUR_Packages"));
  EXPECT_EQ(filter->select(detail), (std::set<std::string>{"shop@bank", "shop@sales", "shop@shell", "shop@warehouse"}));
}

TEST_F(TestTour, ItsPartsAreDrawnSideBySideAndOneUnderTheOtherWhenTheWidthAsksForIt) {
  const auto doc = ds::render::IrDocument::parse(ir(detail, "TOUR_Packages"));
  const std::string wide = ds::render::Svg().render(doc);
  EXPECT_NE(wide.find("data-part=\"boxes\""), std::string::npos);
  const auto sequence = wide.find("data-part=\"sequence\"");
  ASSERT_NE(sequence, std::string::npos);
  EXPECT_NE(wide.rfind(" y=\"0\" ", sequence), std::string::npos);  // beside the boxes: at the top
  const std::string narrow = ds::render::Svg().render(doc, 600);
  const auto under = narrow.find("data-part=\"sequence\"");
  ASSERT_NE(under, std::string::npos);
  EXPECT_NE(narrow.rfind("x=\"0\" y=\"", under), std::string::npos);  // under them: at the left, further down
  EXPECT_EQ(narrow.substr(narrow.rfind("x=\"0\" y=\"", under), 14).find("y=\"0\""), std::string::npos);
}

TEST_F(TestTour, InATerminalTheBoxesComeAboveTheSequence) {
  const std::string text = ds::render::Text().render(ds::render::IrDocument::parse(ir(detail, "TOUR_Packages")), false);
  const auto box = text.find("P sales"), lifeline = text.find("caller");
  ASSERT_NE(box, std::string::npos);
  ASSERT_NE(lifeline, std::string::npos);
  EXPECT_LT(box, lifeline);
}

}  // namespace
