#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ClassBinder.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/MemberBinder.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/ModuleBinder.hpp"
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
class TestMemberBodyBinder : public ::testing::Test {
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
  const ds::bind::MemberBodyBinder bodies{fields, annotations};
  const ds::bind::MemberBinder members{types, signatures, bodies};
  const ds::bind::PackageBinder packages{fields, annotations};
  const ds::bind::ModuleBinder modules{fields, annotations};
  const ds::bind::ClassBinder classes{fields, annotations, extensions, members};
  const ds::bind::Dispatch dispatch{ds::parse::Parser::language(), {&packages, &modules, &classes}};
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

TEST_F(TestMemberBodyBinder, TheBodyOfAMemberGivesItsFieldsAndAnnotations) {
  bindText(
      "package p {\n  module m {\n    class Till {\n      + sell() -> none {\n        title: Sells one\n        @requirement REQ-00001-1\n "
      "     }\n    }\n  }\n}\n");
  const ds::model::Entity* sell = model.find("p@m::Till::sell");
  ASSERT_NE(sell, nullptr);
  EXPECT_EQ(sell->field("title"), "Sells one");
  EXPECT_TRUE(refers("p@m::Till::sell", "REQ-00001-1", "satisfies"));
}

}  // namespace
