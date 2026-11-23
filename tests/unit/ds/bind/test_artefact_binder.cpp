#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/ArtefactBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/parse/TreeReader.hpp"

namespace {

/// The binder of the process artefacts (DS-355): requirements, verifications and constraints, by their id.
class TestArtefactBinder : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::bind::FieldBinder fields{};
  const ds::bind::AnnotationBinder annotations{};
  const ds::bind::ExtensionBinder extensions{annotations};
  const ds::bind::ArtefactBinder artefacts{fields, annotations, extensions};
  const ds::bind::Dispatch dispatch{ds::parse::Parser::language(), {&artefacts}};

  /// Binds the artefacts written in the package of `text`, with no owner.
  void bind(const std::string& text) {
    const ds::parse::ParsedFile file = parser.parseText("a.ds", text);
    const ds::parse::TreeReader reader(file);
    const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "shop@", ""};
    ctx.children(ts_node_named_child(reader.root(), 0));
  }
  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().fromOfKind(from, kind))
      if (r.to == to) return true;
    return false;
  }
};

TEST_F(TestArtefactBinder, ItTakesRequirementsVerificationsAndConstraints) {
  EXPECT_EQ(artefacts.kinds(), (std::vector<std::string>{"requirement", "verification", "constraint"}));
}

TEST_F(TestArtefactBinder, AnArtefactIsIdentifiedByItsIdAndNestingGivesTheRelation) {
  bind(
      "package shop {\n"
      "  requirement REQ-00001-1 {\n"
      "    title: The shop shall sell\n"
      "    requirement REQ-00002-1 {\n"
      "      title: The shop shall sell twice\n"
      "    }\n"
      "    verification VER-00001-1 {\n"
      "      title: A sale is made\n"
      "    }\n"
      "  }\n"
      "  constraint CST-00001-1: Nothing is sold twice\n"
      "}\n");
  const ds::model::Entity* requirement = model.find("REQ-00001-1");
  ASSERT_NE(requirement, nullptr);
  EXPECT_EQ(requirement->kind, ds::model::Kind::Requirement);
  EXPECT_EQ(requirement->fields.at("title"), "The shop shall sell");
  ASSERT_NE(model.find("VER-00001-1"), nullptr);
  EXPECT_EQ(model.find("VER-00001-1")->kind, ds::model::Kind::Verification);
  ASSERT_NE(model.find("CST-00001-1"), nullptr);
  EXPECT_EQ(model.find("CST-00001-1")->fields.at("title"), "Nothing is sold twice");
  EXPECT_TRUE(related("verifies", "VER-00001-1", "REQ-00001-1"));
  EXPECT_TRUE(related("refines", "REQ-00002-1", "REQ-00001-1"));
}

TEST_F(TestArtefactBinder, AnArtefactWrittenWithAnnotationsOnlyExtendsTheOneDefinedElsewhere) {
  bind(
      "package shop {\n"
      "  requirement REQ-00001-1 {\n"
      "    @refines REQ-00009-1\n"
      "  }\n"
      "}\n");
  const ds::model::Entity* expected = model.find("REQ-00001-1");
  ASSERT_NE(expected, nullptr);
  EXPECT_TRUE(expected->expected);  // nobody defined it here: a definition elsewhere replaces it
  ASSERT_EQ(notes.references.size(), 1U);
  EXPECT_EQ(notes.references[0].target, "REQ-00009-1");
}

}  // namespace
