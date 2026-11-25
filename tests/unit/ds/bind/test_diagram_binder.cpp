#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/DiagramBinder.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/parse/TreeReader.hpp"

namespace {

using ds::parse::Field;
using ds::parse::TreeReader;

/// The parts every construct shares and a file read, for one binder's tests.
class TestDiagramBinder : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::bind::FieldBinder fields{};
  const ds::bind::AnnotationBinder annotations{};
  const ds::bind::ExtensionBinder extensions{annotations};
  const ds::bind::TypeBinder types{};
  const ds::bind::SignatureBinder signatures{types};
  const ds::bind::FormulaBinder formulas{};
  const ds::bind::DiagramBinder diagrams{fields, formulas};
  const ds::bind::Dispatch dispatch{parser.language(), {&diagrams}};
  ds::parse::ParsedFile file;

  /// The first node of `type` under `node`, depth first.
  static TSNode find(TSNode node, const char* type) {
    if (TreeReader::is(node, type)) return node;
    for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
      const TSNode found = find(ts_node_named_child(node, i), type);
      if (!ts_node_is_null(found)) return found;
    }
    return TSNode{};
  }
  /// Binds `text` with the binders of the table, from its root.
  void bindText(const std::string& text) {
    file = parser.parseText("a.ds", text);
    const ds::parse::TreeReader reader(file);
    const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
    ctx.children(reader.root());
  }
  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().fromOfKind(from, kind))
      if (r.to == to) return true;
    return false;
  }
  bool refers(const std::string& from, const std::string& target, const std::string& relation) const {
    for (const auto& r : notes.references)
      if (r.from == from && r.target == target && r.relation == relation) return true;
    return false;
  }
};

TEST_F(TestDiagramBinder, ItTakesDiagrams) { EXPECT_EQ(diagrams.kinds(), std::vector<std::string>{"diagram"}); }

TEST_F(TestDiagramBinder, ADiagramIsDeclaredWithItsFields) {
  bindText("diagram CL_M {\n  kind: class\n  include: p@m::*\n}\n");
  const ds::model::Entity* d = model.find(ds::model::Identity::ofDiagram("CL_M").text());
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->kind, ds::model::Kind::Diagram);
  EXPECT_EQ(d->field("kind"), "class");
  EXPECT_EQ(d->field("include"), "p@m::*");
}

/// A walkthrough's steps (DS-468): each an entity numbered in the order written, its title its name, show: split
/// into the diagram and the scenario, and what it shows and lights left to the linker as soft references.
class TestWalkthroughSteps : public TestDiagramBinder {};

TEST_F(TestWalkthroughSteps, EachStepIsAnEntityNumberedInTheOrderWritten) {
  bindText(
      "diagram WALK_A {\n"
      "  kind: walkthrough\n"
      "  title: A walk\n"
      "  step The promise {\n"
      "    show: REQ_A\n"
      "    focus: REQ-00001-1, p@m::C\n"
      "    text: It promises.\n"
      "  }\n"
      "  step When it fails {\n"
      "    show: SEQ_A / refused\n"
      "    highlight: p@m::C::run\n"
      "    at: p@m::C::run\n"
      "    text: It answers 1.\n"
      "  }\n"
      "}\n");
  const ds::model::Entity* first = model.find("diagram:WALK_A::1");
  const ds::model::Entity* second = model.find("diagram:WALK_A::2");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->kind, ds::model::Kind::Step);
  EXPECT_EQ(first->name, "The promise");
  EXPECT_EQ(first->field("show"), "REQ_A");
  EXPECT_EQ(first->field("text"), "It promises.");
  EXPECT_EQ(second->field("show"), "SEQ_A");  // the diagram, and the scenario apart
  EXPECT_EQ(second->field("scenario"), "refused");
  EXPECT_EQ(model.find("diagram:WALK_A")->field("title"), "A walk");
  EXPECT_TRUE(related("contains", "diagram:WALK_A", "diagram:WALK_A::2"));
  EXPECT_TRUE(refers("diagram:WALK_A::1", "diagram:REQ_A", "shows"));
  EXPECT_TRUE(refers("diagram:WALK_A::1", "REQ-00001-1", "focuses"));
  EXPECT_TRUE(refers("diagram:WALK_A::1", "p@m::C", "focuses"));
  EXPECT_TRUE(refers("diagram:WALK_A::2", "p@m::C::run", "highlights"));
  EXPECT_TRUE(refers("diagram:WALK_A::2", "p@m::C::run", "points-at"));
  for (const auto& r : notes.references) EXPECT_TRUE(r.soft) << r.target;  // the check says what names nothing (DS-469)
}

}  // namespace
