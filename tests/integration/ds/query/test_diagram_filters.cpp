#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
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
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("f.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto steps = ds::check::steps();
    ds::check::Checker(model, steps).run(binder);
  }

  Ids of(const std::string& diagram) {
    const ds::model::Entity* d = model.find("diagram:" + diagram);
    if (!d) return {"no diagram " + diagram};
    return ds::query::Query(model).select(*ds::query::filterFor(*d));
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

/// filterFor: the one place that names the filter of each kind of diagram.
class TestFilterFor : public TestFilters {};

TEST_F(TestFilterFor, EachKindOfDiagramHasItsFilterAndAnythingElseHasNone) {
  load(std::string(shop()) + "diagram SEQ_Sell {\n  kind: sequence\n  include: shop@till::Till::sell\n}\n" +
       "diagram REQ_None {\n  kind: requirement\n  include: REQ-09999-1\n}\n");
  // a failed load leaves the model empty: say so, never dereference what it does not hold
  const ds::model::Entity* sell = model.find("diagram:SEQ_Sell");
  const ds::model::Entity* none = model.find("diagram:REQ_None");
  const ds::model::Entity* till = model.find("shop@till::Till");
  ASSERT_NE(sell, nullptr);
  ASSERT_NE(none, nullptr);
  ASSERT_NE(till, nullptr);
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(ds::query::filterFor(*sell).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::RequirementsFilter*>(ds::query::filterFor(*none).get()), nullptr);
  EXPECT_EQ(ds::query::filterFor(*till), nullptr);
}

/// A filter of our own: built with its criteria and passed in; the query knows nothing of it.
class PublicMethods : public ds::query::IFilter {
 public:
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
  const ds::query::Query query(model);
  const ds::query::IQuery& entry = query;  // what the rest of ds sees
  EXPECT_EQ(entry.select(PublicMethods()), (Ids{"shop@store::Stock::take", "shop@till::Till::sell"}));
}

TEST_F(TestFilters, WhatADiagramShowsIsAnswerdByIdentityOnly) {
  load(std::string(shop()) + "diagram SEQ_Sell {\n  kind: sequence\n  include: shop@till::Till::sell\n}\n");
  for (const auto& a : ds::query::Query(model).ask("shows", "SEQ_Sell")) {
    EXPECT_NE(model.find(a.id), nullptr) << a.id;  // every answer names an identity (REQ-00003-1)
  }
}

}  // namespace
