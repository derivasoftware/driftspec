#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"

namespace {

/// The one place that names the query (DS-397): what asks the model a question is handed an IQuery, which answers
/// as the query does.
TEST(TestQueryFactory, TheQueryIsHandedOutAsAnIQuery) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(
      parser.parseText("t.ds", "package p { module m { class A { } class B { } } }\ndiagram D {\n  kind: class\n  include: p@m::*\n}\n"));
  const auto questions = ds::query::QueryFactory::of(model, metamodel);
  ASSERT_NE(questions, nullptr);
  EXPECT_NE(dynamic_cast<const ds::query::Query*>(questions.get()), nullptr);
  std::vector<std::string> shown;
  for (const auto& a : questions->ask("shows", "D")) shown.push_back(a.id);
  EXPECT_EQ(shown, (std::vector<std::string>{"p@m", "p@m::A", "p@m::B"}));  // p@m::* takes the module and what is in it
  std::vector<std::string> direct;
  for (const auto& a : ds::query::Query(model, metamodel).shows("D")) direct.push_back(a.id);
  EXPECT_EQ(shown, direct);
}

}  // namespace
