#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ClassBinder.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/LegacyBinder.hpp"
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
  const ds::bind::MemberBodyBinder bodies{fields, annotations};
  const ds::bind::MemberBinder members{types, signatures, bodies};
  const ds::bind::ClassBinder classes{fields, annotations, extensions, members};
  const ds::bind::LegacyBinder legacy{fields, annotations, types};
  const ds::bind::Dispatch dispatch{ds::parse::Parser::language(), {&packages, &modules, &classes, &legacy}};
};

TEST_F(TestLegacyBinder, ItTakesTheConstructsNoBinderOfTheirOwnTakesYet) {
  const std::vector<std::string> kinds = legacy.kinds();
  for (const char* kind : {"sequence"}) EXPECT_NE(std::find(kinds.begin(), kinds.end(), kind), kinds.end()) << kind;
  EXPECT_EQ(std::find(kinds.begin(), kinds.end(), "requirement"), kinds.end());  // the artefacts have their binder
  EXPECT_EQ(std::find(kinds.begin(), kinds.end(), "package"), kinds.end());      // and the boxes theirs (DS-356)
  EXPECT_EQ(std::find(kinds.begin(), kinds.end(), "state"), kinds.end());        // and the state machines theirs
}

TEST_F(TestLegacyBinder, ASequenceIsTheBehaviourOfItsOperationWithItsMessages) {
  const ds::parse::ParsedFile file = parser.parseText("a.ds",
                                                      "package shop {\n"
                                                      "  module till {\n"
                                                      "    class Stock { + take() -> none }\n"
                                                      "    class Till {\n"
                                                      "      - stock: Stock\n"
                                                      "      + sell() -> none\n"
                                                      "    }\n"
                                                      "    sequence Till::sell {\n"
                                                      "      self -> stock.take()\n"
                                                      "    }\n"
                                                      "  }\n"
                                                      "}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
  ctx.children(reader.root());
  ASSERT_NE(model.find("shop@till::Till::sell::sequence"), nullptr);
  ASSERT_EQ(notes.messages.size(), 1U);  // the message, for the checker to type
  EXPECT_EQ(notes.messages[0].op, "take");
}

}  // namespace
