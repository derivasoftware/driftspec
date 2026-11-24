#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/LegacyBinder.hpp"
#include "ds/bind/ModuleBinder.hpp"
#include "ds/bind/PackageBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/parse/TreeReader.hpp"

namespace {

/// The constructs with no binder of their own yet (DS-355), bound by the walk the binder had.
class TestLegacyBinder : public ::testing::Test {
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
  const ds::bind::PackageBinder packages{fields, annotations};
  const ds::bind::ModuleBinder modules{fields, annotations};
  const ds::bind::LegacyBinder legacy{fields, annotations, extensions, types, signatures};
  const ds::bind::Dispatch dispatch{ds::parse::Parser::language(), {&packages, &modules, &legacy}};
};

TEST_F(TestLegacyBinder, ItTakesTheConstructsNoBinderOfTheirOwnTakesYet) {
  const std::vector<std::string> kinds = legacy.kinds();
  for (const char* kind : {"usecase", "class", "function", "sequence", "state"})
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), kind), kinds.end()) << kind;
  EXPECT_EQ(std::find(kinds.begin(), kinds.end(), "requirement"), kinds.end());  // the artefacts have their binder
  EXPECT_EQ(std::find(kinds.begin(), kinds.end(), "package"), kinds.end());      // and the boxes theirs (DS-356)
}

TEST_F(TestLegacyBinder, AClassInAModuleIsDeclaredAndContained) {
  const ds::parse::ParsedFile file = parser.parseText("a.ds",
                                                      "package shop {\n"
                                                      "  module till {\n"
                                                      "    class Till : Base {\n"
                                                      "      + sell() -> none\n"
                                                      "    }\n"
                                                      "  }\n"
                                                      "}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
  ctx.children(reader.root());
  ASSERT_NE(model.find("shop"), nullptr);
  ASSERT_NE(model.find("shop@till"), nullptr);
  ASSERT_NE(model.find("shop@till::Till"), nullptr);
  ASSERT_NE(model.find("shop@till::Till::sell"), nullptr);
  bool contained = false;
  for (const auto& r : model.index().fromOfKind("shop@till", "contains")) contained |= r.to == "shop@till::Till";
  EXPECT_TRUE(contained);
  bool base = false;  // the base is a name for the checker to resolve
  for (const auto& r : notes.references) base |= r.from == "shop@till::Till" && r.target == "Base" && r.relation == "inherits";
  EXPECT_TRUE(base);
}

}  // namespace
