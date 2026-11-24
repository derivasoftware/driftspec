#include <gtest/gtest.h>

#include <memory>
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
#include "ds/query/Query.hpp"

namespace {

using Ids = std::set<std::string>;

/// The query is a filter engine (DS-127): a filter carries its criteria and reads the model it is
/// handed; a diagram's content is the filter of its kind. Nothing here draws.
class TestFilters : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("f.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  Ids of(const std::string& diagram) {
    const ds::model::Entity* d = model.find("diagram:" + diagram);
    if (!d) return {"no diagram " + diagram};
    return ds::query::Query(model, metamodel).select(*ds::query::FilterFactory::of(metamodel, *d));
  }

  static const char* shop() {
    return "package shop {\n"
           "  module store {\n"
           "    class Item { + price: int }\n"
           "    class Stock {\n"
           "      - items: std::vector<Item>\n"
           "      + take(n: int) -> Item\n"
           "    }\n"
           "    class Ledger { }\n"
           "  }\n"
           "  module till {\n"
           "    class Till {\n"
           "      - stock: store::Stock\n"
           "      + sell(n: int) -> int\n"
           "    }\n"
           "    sequence Till::sell {\n"
           "      self -> stock.take(n) -> item\n"
           "      return n\n"
           "    }\n"
           "  }\n"
           "}\n";
  }
};

TEST_F(TestFilters, AClassDiagramTakesItsPatternsMinusItsExclusionsAndTheBoxesAround) {
  load(std::string(shop()) + "diagram CL_Store {\n  kind: class\n  include: shop@store::*\n  exclude: shop@store::Ledger\n}\n");
  EXPECT_EQ(of("CL_Store"), (Ids{"shop@store", "shop@store::Item", "shop@store::Stock"}));
}

TEST_F(TestFilters, NeighboursWidenAClassDiagramByTheRelationsItDraws) {
  load(std::string(shop()) + "diagram CL_Till {\n  kind: class\n  include: shop@till::Till\n  neighbours: 1\n}\n");
  EXPECT_EQ(of("CL_Till"), (Ids{"shop@till", "shop@till::Till", "shop@store", "shop@store::Stock"}));
}

TEST_F(TestFilters, NeighboursPastWhatTheSelectionReachesTakeTheReach) {  // DS-362: a step that adds nothing ends the walk
  std::string text =
      "package p {\n  module m {\n    class A {\n      - b: B\n    }\n    class B {\n      - c: C\n    }\n    class C\n  }\n}\n";
  for (const char* n : {"1", "2", "3", "2147483647"})
    text += std::string("diagram CL_") + n + " {\n  kind: class\n  include: p@m::A\n  neighbours: " + n + "\n}\n";
  load(text);
  const Ids reach{"p@m", "p@m::A", "p@m::B", "p@m::C"};     // A and what it relates to, two steps away
  EXPECT_EQ(of("CL_1"), (Ids{"p@m", "p@m::A", "p@m::B"}));  // one step short of it
  EXPECT_EQ(of("CL_2"), reach);
  EXPECT_EQ(of("CL_3"), reach);           // the step past it adds nothing
  EXPECT_EQ(of("CL_2147483647"), reach);  // and nor does any after it: the walk ends there
}

TEST_F(TestFilters, AClosedBoxStandsForWhatItHolds) {
  load(std::string(shop()) + "diagram HLD_Shop {\n  kind: class\n  include: shop@*\n  open: shop@till\n}\n");
  const Ids shown = of("HLD_Shop");
  EXPECT_TRUE(shown.count("shop@store"));          // closed: the box itself
  EXPECT_FALSE(shown.count("shop@store::Stock"));  // and not what it holds
  EXPECT_TRUE(shown.count("shop@till::Till"));     // open
}

TEST_F(TestFilters, ASequenceDiagramTakesWhoTakesPartNotHowItIsDrawn) {
  load(std::string(shop()) + "diagram SEQ_Sell {\n  kind: sequence\n  include: shop@till::Till::sell\n}\n");
  // the class it runs on and the types it calls: no caller, no lifeline named after a variable
  EXPECT_EQ(of("SEQ_Sell"), (Ids{"shop@till::Till", "shop@store::Stock"}));
}

TEST_F(TestFilters, ARequirementsDiagramTakesItsRootsAndTheirUmbrella) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: The system shall sell\n"
      "  requirement REQ-00002-1 {\n    @refines REQ-00001-1\n    title: The till shall take money\n  }\n"
      "  module m {\n    class Till {\n      @requirement REQ-00002-1\n      + sell() -> int\n    }\n  }\n"
      "  requirement REQ-00003-1: The system shall not lose stock\n"
      "}\n"
      "diagram REQ_Sell {\n  kind: requirement\n  include: REQ-00001-1\n}\n");
  EXPECT_EQ(of("REQ_Sell"), (Ids{"REQ-00001-1", "REQ-00002-1", "p@m::Till"}));
}

/// Filters: the one place that names the filter of each kind of diagram (DS-385).
TEST_F(TestFilters, EachKindOfDiagramHasItsFilterAndAnythingElseHasNone) {
  load(std::string(shop()) + "diagram SEQ_Sell {\n  kind: sequence\n  include: shop@till::Till::sell\n}\n" +
       "diagram REQ_None {\n  kind: requirement\n  include: REQ-09999-1\n}\n");
  // a failed load leaves the model empty: say so, never dereference what it does not hold
  const ds::model::Entity* sell = model.find("diagram:SEQ_Sell");
  const ds::model::Entity* none = model.find("diagram:REQ_None");
  const ds::model::Entity* till = model.find("shop@till::Till");
  ASSERT_NE(sell, nullptr);
  ASSERT_NE(none, nullptr);
  ASSERT_NE(till, nullptr);
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(ds::query::FilterFactory::of(metamodel, *sell).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::RequirementsFilter*>(ds::query::FilterFactory::of(metamodel, *none).get()), nullptr);
  EXPECT_EQ(ds::query::FilterFactory::of(metamodel, *till), nullptr);
}

/// Each form of a pattern, read once by Pattern (DS-385), takes in a class diagram what it names: a template written
/// without its arity, a module named as such, a prefix over the identity or the scope; an exclusion takes out alike.
TEST_F(TestFilters, EachFormOfAPatternTakesWhatItNamesInAClassDiagram) {
  load(
      "package p {\n"
      "  module m {\n    class Box<T> { }\n    class Boxes { }\n    class Item { }\n  }\n"
      "  module n {\n    class Till { }\n    class Tray { }\n  }\n"
      "  module o {\n    class Old { }\n    class Gone { }\n  }\n"
      "}\n"
      "diagram CL_Template {\n  include: p@m::Box\n}\n"
      "diagram CL_Module {\n  include: p@n\n  exclude: p@n::Tray\n}\n"
      "diagram CL_Prefix {\n  include: p@o::*, p@m::It*\n  exclude: p@o::Go*\n}\n");
  EXPECT_EQ(of("CL_Template"), (Ids{"p@m", "p@m::Box<1>"}));  // Box<T>, by its arity
  EXPECT_EQ(of("CL_Module"), (Ids{"p@n", "p@n::Till"}));
  EXPECT_EQ(of("CL_Prefix"), (Ids{"p@m", "p@m::Item", "p@o", "p@o::Old"}));
}

TEST_F(TestFilters, ANumberTooLargeForAnIntIsReadAsNoNumberAndNeverAnAbort) {  // DS-337: std::stoi threw out_of_range
  load(std::string(shop()) + "diagram SEQ_Deep {\n  kind: sequence\n  include: shop@till::Till::sell\n  depth: 99999999999\n}\n" +
       "diagram SEQ_Lots {\n  kind: sequence\n  include: shop@till::Till::sell\n  depth: lots\n}\n" +
       "diagram CL_Wide {\n  kind: class\n  include: shop@till::Till\n  neighbours: 99999999999\n}\n" +
       "diagram CL_Some {\n  kind: class\n  include: shop@till::Till\n  neighbours: some\n}\n");
  Ids deep, wide;
  EXPECT_NO_THROW(deep = of("SEQ_Deep"));
  EXPECT_NO_THROW(wide = of("CL_Wide"));
  EXPECT_EQ(deep, of("SEQ_Lots"));
  EXPECT_EQ(wide, of("CL_Some"));
  EXPECT_EQ(wide, (Ids{"shop@till", "shop@till::Till"}));  // no neighbours, as for a word
}

/// A filter of our own: built with its criteria and passed in; the query knows nothing of it.
class PublicMethods : public ds::query::IFilter {
 public:
  std::set<std::string> boxes(const ds::model::Model& model) const override { return select(model); }
  std::set<std::string> select(const ds::model::Model& model) const override {
    std::set<std::string> out;
    for (const auto& pair : model.entities()) {
      const auto vis = pair.second.fields.find("visibility");
      if (pair.second.kind == ds::model::Kind::Method && vis != pair.second.fields.end() && vis->second == "+") out.insert(pair.first);
    }
    return out;
  }
};

TEST_F(TestFilters, TheQueryAppliesAFilterItIsGivenWithoutKnowingIt) {
  load(shop());
  const ds::query::Query query(model, metamodel);
  const ds::query::IQuery& entry = query;  // what the rest of ds sees
  EXPECT_EQ(entry.select(PublicMethods()), (Ids{"shop@store::Stock::take", "shop@till::Till::sell"}));
}

TEST_F(TestFilters, WhatADiagramShowsIsAnswerdByIdentityOnly) {
  load(std::string(shop()) + "diagram SEQ_Sell {\n  kind: sequence\n  include: shop@till::Till::sell\n}\n");
  for (const auto& a : ds::query::Query(model, metamodel).ask("shows", "SEQ_Sell")) {
    EXPECT_NE(model.find(a.id), nullptr) << a.id;  // every answer names an identity (REQ-00003-1)
  }
}

}  // namespace
