#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A type is read the same by the binder and the checker (REQ-00028-2, REQ-00029-2; DS-112, DS-113):
/// a qualified container is the container, and a template's arity counts its own arguments only.
class TestTypeReading : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  module m {\n"
                                       "    class Item {\n"
                                       "      + price() -> int\n"
                                       "    }\n"
                                       "    class Index<K, V> {\n"
                                       "      + find() -> none\n"
                                       "    }\n"
                                       "    class Cart {\n"
                                       "      - owns items: std::vector<Item>\n"
                                       "      - owns byName: std::map<string, Item>\n"
                                       "      - refs index: Index<string, map<string, int>>\n"
                                       "      + total() -> int\n"
                                       "    }\n"
                                       "    sequence Cart::total {\n"
                                       "      self -> items.price() -> a\n"
                                       "      self -> byName.price() -> b\n"
                                       "      self -> index.find()\n"
                                       "      return a\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  int calls(const std::string& to) const {
    int n = 0;
    for (const auto& r : model.index().all()) n += r.kind == "calls" && r.to == to ? 1 : 0;
    return n;
  }
};

TEST_F(TestTypeReading, AQualifiedContainerHandsTheCallToItsElement) {
  EXPECT_EQ(calls("p@m::Item::price"), 2);  // through std::vector<Item> and through the value of std::map<string, Item>
  for (const auto& f : model.findings()) ADD_FAILURE() << f.code << " " << f.message;
}

TEST_F(TestTypeReading, ATemplatesArityCountsItsOwnArgumentsOnly) {
  EXPECT_EQ(calls("p@m::Index<2>::find"), 1);  // Index<string, map<string, int>> is Index<2>, not Index<3>
}

}  // namespace
