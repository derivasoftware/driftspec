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
  const ds::bind::Dispatch dispatch{ds::parse::Parser::language(), {&diagrams}};
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

}  // namespace
