#include <gtest/gtest.h>

#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/query/Filter.hpp"

namespace {

ds::model::Entity diagram(const std::string& kind) {
  ds::model::Entity d;
  d.kind = ds::model::Kind::Diagram;
  d.id = "diagram:D";
  d.fields["kind"] = kind;
  d.fields["include"] = "p@m::*";
  return d;
}

/// filterFor alone: the one place that names the filter of each kind of diagram, read in the language it is
/// handed. What each filter takes of a design is held by tests/integration/ds/query/test_filters.cpp.
class TestFilterFor : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
};

TEST_F(TestFilterFor, ADiagramGetsTheFilterOfItsKind) {
  EXPECT_NE(dynamic_cast<ds::query::ClassFilter*>(ds::query::filterFor(metamodel, diagram("class")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(ds::query::filterFor(metamodel, diagram("sequence")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::StateFilter*>(ds::query::filterFor(metamodel, diagram("state")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::RequirementsFilter*>(ds::query::filterFor(metamodel, diagram("requirement")).get()), nullptr);
}

TEST_F(TestFilterFor, WhatIsNoDiagramHasNoFilter) {
  ds::model::Entity c = diagram("class");
  c.kind = ds::model::Kind::Class;
  EXPECT_EQ(ds::query::filterFor(metamodel, c), nullptr);
}

}  // namespace
