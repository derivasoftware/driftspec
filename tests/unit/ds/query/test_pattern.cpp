#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Pattern.hpp"

namespace {

/// One pattern of a diagram, read once and asked of each entity (DS-212, DS-385), over a store that holds a
/// requirement, a template and a class with a member, and a till beside it.
class TestPattern : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("shop.ds",
                                       "package shop {\n"
                                       "  module store {\n"
                                       "    requirement REQ-00001-1: The store shall keep its stock\n"
                                       "    class Item { + price: int }\n"
                                       "    class Stock {\n"
                                       "      + take(n: int) -> Item\n"
                                       "    }\n"
                                       "    class Shelf<T> { }\n"
                                       "    class Shelves { }\n"
                                       "  }\n"
                                       "  module till {\n"
                                       "    class Till { }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  /// Whether the pattern written `text` takes the entity `id`; an entity the model does not hold is a failure.
  bool takes(const std::string& text, const std::string& id) const {
    const ds::model::Entity* entity = model.find(id);
    EXPECT_NE(entity, nullptr) << id;
    return entity && ds::query::Pattern(model, text).takes(*entity);
  }
};

TEST_F(TestPattern, AnIdentityTakesWhatItNamesAndNothingElse) {
  EXPECT_TRUE(takes("shop@store::Item", "shop@store::Item"));
  EXPECT_FALSE(takes("shop@store::Item", "shop@store::Stock"));
  EXPECT_FALSE(takes("shop@store::Stock", "shop@store::Stock::take"));  // a class named is no scope: its members stay out
  EXPECT_FALSE(takes("shop@store::Ite", "shop@store::Item"));           // a part of a name is no name
  EXPECT_FALSE(takes("shop@nowhere", "shop@store::Item"));              // what nothing declares takes nothing
}

TEST_F(TestPattern, APrefixTakesWhatStartsWithItInItsIdentityOrItsScope) {
  EXPECT_TRUE(takes("shop@store::It*", "shop@store::Item"));
  EXPECT_FALSE(takes("shop@store::It*", "shop@store::Stock"));
  EXPECT_TRUE(takes("shop@store::Stock*", "shop@store::Stock::take"));  // over the identity
  EXPECT_TRUE(takes("shop@sto*", "REQ-00001-1"));                       // over the scope: written in shop@store
  EXPECT_FALSE(takes("shop@store::*", "shop@till::Till"));
  EXPECT_TRUE(takes("*", "shop@till::Till"));  // nothing before the * takes everything
}

TEST_F(TestPattern, APrefixEndingInAModulesSeparatorTakesWhatTheModuleHoldsItsArtefactsToo) {
  EXPECT_TRUE(takes("shop@store::*", "shop@store::Item"));
  EXPECT_TRUE(takes("shop@store::*", "shop@store::Stock::take"));
  EXPECT_TRUE(takes("shop@store::*", "REQ-00001-1"));  // named by no prefix of its own, written in the module
  EXPECT_FALSE(takes("shop@till::*", "REQ-00001-1"));
}

TEST_F(TestPattern, AModuleNamedAsSuchTakesItselfAndWhatItHoldsAtAnyDepth) {
  EXPECT_TRUE(takes("shop@store", "shop@store"));
  EXPECT_TRUE(takes("shop@store", "shop@store::Item"));
  EXPECT_TRUE(takes("shop@store", "shop@store::Stock::take"));
  EXPECT_TRUE(takes("shop@store", "REQ-00001-1"));
  EXPECT_FALSE(takes("shop@store", "shop@till::Till"));
  EXPECT_FALSE(takes("shop@store", "shop@till"));
}

TEST_F(TestPattern, ATemplateWrittenWithoutItsArityTakesTheTemplateAndNoOtherName) {  // Shelf<T> is Shelf<1>
  EXPECT_TRUE(takes("shop@store::Shelf", "shop@store::Shelf<1>"));
  EXPECT_FALSE(takes("shop@store::Shelf", "shop@store::Shelves"));     // a longer name is another name
  EXPECT_FALSE(takes("shop@store::Shel", "shop@store::Shelf<1>"));     // and so is a shorter one
  EXPECT_TRUE(takes("shop@store::Shelf<1>", "shop@store::Shelf<1>"));  // with its arity, its identity
}

TEST_F(TestPattern, ReadOnceItTakesAsManyEntitiesAsItIsAskedOf) {
  const ds::query::Pattern pattern(model, "shop@store");
  int taken = 0;
  for (const auto& pair : model.entities()) taken += pattern.takes(pair.second) ? 1 : 0;
  int held = 0;  // what the module is, and what is written in it at any depth
  for (const auto& pair : model.entities())
    held += pair.first == "shop@store" || pair.second.scope == "shop@store" || pair.second.scope.rfind("shop@store::", 0) == 0;
  EXPECT_EQ(taken, held);
  EXPECT_GT(taken, 5);
}

}  // namespace
