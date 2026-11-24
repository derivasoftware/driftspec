#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/InterfaceBinder.hpp"
#include "ds/bind/PackageBinder.hpp"
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
class TestInterfaceBinder : public ::testing::Test {
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
  const ds::bind::PackageBinder packages{fields, annotations};
  const ds::bind::InterfaceBinder interfaces{fields, annotations, signatures};
  const ds::bind::Dispatch dispatch{parser.language(), {&packages, &interfaces}};
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

TEST_F(TestInterfaceBinder, ItTakesInterfaces) { EXPECT_EQ(interfaces.kinds(), std::vector<std::string>{"interface"}); }

TEST_F(TestInterfaceBinder, AnInterfaceIsExposedAndItsOperationsRealiseTheirUseCases) {
  bindText("package shop {\n  interface door {\n    kind: cli\n    operation run(args: Args) -> int : UC-00001-1\n  }\n}\n");
  ASSERT_NE(model.find("shop@door"), nullptr);
  EXPECT_TRUE(related("exposes", "shop", "shop@door"));
  const ds::model::Entity* run = model.find("shop@door::run");
  ASSERT_NE(run, nullptr);
  EXPECT_EQ(run->field("returns"), "int");
  EXPECT_TRUE(related("contains", "shop@door", "shop@door::run"));
  EXPECT_TRUE(refers("shop@door::run", "UC-00001-1", "realises"));
}

}  // namespace
