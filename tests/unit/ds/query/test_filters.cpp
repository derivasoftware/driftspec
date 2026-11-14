#include <gtest/gtest.h>

#include <string>

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

/// filterFor alone: the one place that names the filter of each kind of diagram. What each filter
/// takes of a design is held by tests/integration/ds/query/test_filters.cpp.
TEST(TestFilterFor, ADiagramGetsTheFilterOfItsKind) {
  EXPECT_NE(dynamic_cast<ds::query::ClassFilter*>(ds::query::filterFor(diagram("class")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(ds::query::filterFor(diagram("sequence")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::StateFilter*>(ds::query::filterFor(diagram("state")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::RequirementsFilter*>(ds::query::filterFor(diagram("requirement")).get()), nullptr);
}

TEST(TestFilterFor, WhatIsNoDiagramHasNoFilter) {
  ds::model::Entity c = diagram("class");
  c.kind = ds::model::Kind::Class;
  EXPECT_EQ(ds::query::filterFor(c), nullptr);
}

}  // namespace
