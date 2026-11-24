#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/query/Filter.hpp"

namespace {

ds::model::Entity diagram(const std::string& kind) {
  ds::model::Entity d;
  d.kind = ds::model::Kind::Diagram;
  d.id = "diagram:D";
  if (!kind.empty()) d.fields["kind"] = kind;
  d.fields["include"] = "p@m::*";
  return d;
}

/// Filters alone (DS-385): the one place that names the filter of each kind of diagram, read in the language it is
/// handed. What each filter takes of a design is held by tests/integration/ds/query/test_diagram_filters.cpp.
class TestFilters : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  std::unique_ptr<ds::query::IFilter> of(const ds::model::Entity& diagram) const {
    return ds::query::FilterFactory::of(metamodel, diagram);
  }
};

TEST_F(TestFilters, ADiagramGetsTheFilterOfItsKind) {
  EXPECT_NE(dynamic_cast<ds::query::ClassFilter*>(of(diagram("class")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(of(diagram("sequence")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::StateFilter*>(of(diagram("state")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::RequirementsFilter*>(of(diagram("requirement")).get()), nullptr);
}

TEST_F(TestFilters, ATourIsASequenceATableIsAMachineAndAnUnwrittenKindIsAClassDiagram) {
  EXPECT_NE(dynamic_cast<ds::query::SequenceFilter*>(of(diagram("tour")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::StateFilter*>(of(diagram("table")).get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::query::ClassFilter*>(of(diagram("")).get()), nullptr);
}

TEST_F(TestFilters, WhatIsNoDiagramHasNoFilter) {
  ds::model::Entity c = diagram("class");
  c.kind = ds::model::Kind::Class;
  EXPECT_EQ(of(c), nullptr);
}

/// The factory by its own name (DS-404): a diagram gets a filter, anything else none.
TEST(TestFilterFactory, ADiagramGetsAFilterAndNothingElseDoes) {
  const ds::model::Metamodel metamodel;
  EXPECT_NE(ds::query::FilterFactory::of(metamodel, diagram("class")), nullptr);
  ds::model::Entity cls;
  cls.kind = ds::model::Kind::Class;
  cls.id = "p@m::C";
  EXPECT_EQ(ds::query::FilterFactory::of(metamodel, cls), nullptr);
}

}  // namespace
