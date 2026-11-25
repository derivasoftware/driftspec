#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Everything the binder and the checker produce is something the metamodel declares (REQ-00066-1),
/// over the design of ds and every example corpus; a field it does not declare is reported (REQ-00067-1).
class TestMetamodelConformance : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model load(const std::string& dir) const {
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(dir)) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    return model;
  }

  static std::vector<std::string> workspaces() {
    std::vector<std::string> out{std::string(DRIFTSPEC_SOURCE_DIR) + "/design"};
    for (const auto& d : std::filesystem::directory_iterator(std::string(DRIFTSPEC_CORPUS_DIR))) {
      if (d.is_directory()) out.push_back(d.path().string());
    }
    return out;
  }

  static ds::model::Kind parentKind(const ds::model::Model& model, const ds::model::Entity& e, bool* top) {
    *top = e.scope.empty();
    if (e.scope.empty()) return ds::model::Kind::Package;
    if (e.scope.back() == '@') return ds::model::Kind::Package;  // a package names its children with @
    const ds::model::Entity* parent = model.find(e.scope);
    return parent ? parent->kind : ds::model::Kind::Package;
  }
};

TEST_F(TestMetamodelConformance, EveryEntityAndRelationIsOneTheMetamodelDeclares) {
  int entities = 0, relations = 0;
  for (const auto& dir : workspaces()) {
    const ds::model::Model model = load(dir);
    for (const auto& pair : model.entities()) {
      const ds::model::Entity& e = pair.second;
      const ds::model::KindSpec& spec = metamodel.specOf(e.kind);
      for (const auto& flag : e.flags)
        EXPECT_NE(std::find(spec.flags.begin(), spec.flags.end(), flag), spec.flags.end()) << dir << " " << e.id << " flag " << flag;
      if (e.expected) continue;
      ++entities;
      bool top = false;
      const ds::model::Kind parent = parentKind(model, e, &top);
      if (top)
        EXPECT_TRUE(spec.top) << dir << " " << e.id;
      else
        EXPECT_NE(std::find(spec.parents.begin(), spec.parents.end(), parent), spec.parents.end())
            << dir << " " << e.id << " inside a " << metamodel.nameOf(parent);
    }
    for (const auto& r : model.index().all()) {
      ++relations;
      const ds::model::RelationSpec* spec = metamodel.relationOf(r.kind);
      ASSERT_NE(spec, nullptr) << dir << " relation " << r.kind;
      const ds::model::Entity* from = model.find(r.from);
      const ds::model::Entity* to = model.find(r.to);
      if (!from || !to) continue;  // an end the checker reports as undeclared
      EXPECT_TRUE(metamodel.joins(*spec, from->kind, to->kind)) << dir << " " << r.kind << " " << metamodel.nameOf(from->kind) << " -> "
                                                                << metamodel.nameOf(to->kind) << " (" << r.from << " -> " << r.to << ")";
    }
  }
  EXPECT_GT(entities, 1000);
  EXPECT_GT(relations, 2000);
}

TEST_F(TestMetamodelConformance, ParametersAndArgumentsAreReadOnceAndAgreeWithTheirText) {
  int params = 0, messages = 0;
  for (const auto& dir : workspaces()) {
    const ds::model::Model model = load(dir);
    for (const auto& pair : model.entities()) {
      const ds::model::Entity& e = pair.second;
      if (e.kind == ds::model::Kind::Message) {
        ++messages;
        const auto argc = e.fields.find("argc");
        ASSERT_NE(argc, e.fields.end()) << e.id;
        EXPECT_EQ(std::to_string(e.args.size()), argc->second) << dir << " " << e.id;
        std::string joined;
        for (const auto& a : e.args) joined += (joined.empty() ? "" : ", ") + a;
        EXPECT_EQ(joined, e.fields.at("args")) << dir << " " << e.id;
        continue;
      }
      const auto text = e.fields.find("params");
      if (text == e.fields.end()) {
        EXPECT_TRUE(e.params.empty()) << dir << " " << e.id;
        continue;
      }
      ++params;
      std::string joined;
      for (const auto& p : e.params) {
        const bool tpl = e.kind == ds::model::Kind::Class;  // a template lists names; its bounds are bound.* fields
        joined += (joined.empty() ? "" : ", ") + p.name + (tpl || p.type.empty() ? "" : ": " + p.type);
      }
      EXPECT_EQ(joined, text->second) << dir << " " << e.id;
    }
  }
  EXPECT_GT(params, 200);
  EXPECT_GT(messages, 300);
}

TEST_F(TestMetamodelConformance, AFieldItsKindDoesNotDeclareIsReportedAndADeclaredOneIsNot) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  requirement REQ-00001-1 {\n"
                               "    title: The tool shall do one thing\n"
                               "    status: draft\n"
                               "    priority: high\n"
                               "  }\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  std::vector<std::string> unknown;
  for (const auto& f : model.findings()) {
    if (f.code == "W-FIELD-UNKNOWN") unknown.push_back(f.message);
  }
  ASSERT_EQ(unknown.size(), 1u);
  EXPECT_NE(unknown[0].find("'priority'"), std::string::npos);
}

/// show:, hide: and an interface's kind: are closed too (DS-435): each name of a list is taken or said, once, and a
/// name the drawing reads passes.
TEST_F(TestMetamodelConformance, EachNameOfAClosedListIsTakenOrSaid) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  interface api {\n"
                               "    kind: telepathy\n"
                               "  }\n"
                               "  interface lib {\n"
                               "    kind: library\n"
                               "  }\n"
                               "}\n"
                               "diagram CL_X {\n"
                               "  kind: class\n"
                               "  include: p@*\n"
                               "  show: ownership, bogus\n"
                               "  hide: nothing, methods\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  std::vector<std::string> wrong;
  for (const auto& f : model.findings())
    if (f.code == "W-FIELD-VALUE") wrong.push_back(f.message);
  ASSERT_EQ(wrong.size(), 3u);  // telepathy, bogus and nothing; library, ownership and methods pass
  const auto said = [&](const std::string& part) {
    for (const auto& w : wrong)
      if (w.find(part) != std::string::npos) return true;
    return false;
  };
  EXPECT_TRUE(said("kind: telepathy"));
  EXPECT_TRUE(said("show: bogus"));
  EXPECT_TRUE(said("hide: nothing"));
}

TEST_F(TestMetamodelConformance, AClosedFieldWithAValueItDoesNotTakeIsReportedAndOneItTakesIsNot) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  requirement REQ-00001-1 {\n"
                               "    title: The tool shall do one thing\n"
                               "    status: aproved\n"
                               "    verification VER-00001-1 {\n"
                               "      title: Someone looks\n"
                               "      method: telepathy\n"
                               "      level: unit\n"
                               "    }\n"
                               "  }\n"
                               "  requirement REQ-00002-1 {\n"
                               "    title: The tool shall do another\n"
                               "    status: approved\n"
                               "  }\n"
                               "}\n"
                               "diagram SEQ_X {\n"
                               "  kind: sequense\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  std::vector<std::string> wrong;
  for (const auto& f : model.findings())
    if (f.code == "W-FIELD-VALUE") wrong.push_back(f.message);
  ASSERT_EQ(wrong.size(), 3u);
  const auto said = [&wrong](const std::string& text) {
    return std::any_of(wrong.begin(), wrong.end(), [&text](const std::string& m) { return m.find(text) != std::string::npos; });
  };
  EXPECT_TRUE(said("kind: sequense"));
  EXPECT_TRUE(said("status: aproved"));
  EXPECT_TRUE(said("method: telepathy"));
}

TEST_F(TestMetamodelConformance, ARelationTheLanguageDoesNotAllowIsReportedAndNotBound) {
  const auto check = [this](const std::string& base) {
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText(
        "a.ds", "package p {\n  interface engine {\n    kind: library\n  }\n  module m {\n    class Api" + base + "\n  }\n}\n"));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    return model;
  };
  const ds::model::Model wrong = check(" : engine { }");  // DS-93: a class that names an interface as its base
  std::vector<std::string> kind;
  for (const auto& f : wrong.findings())
    if (f.code == "E-RELATION-KIND") kind.push_back(f.message);
  ASSERT_EQ(kind.size(), 1u);
  EXPECT_NE(kind[0].find("@interface engine"), std::string::npos) << kind[0];
  for (const auto& r : wrong.index().all()) EXPECT_FALSE(r.kind == "inherits" && r.from == "p@m::Api") << r.to;

  const ds::model::Model right = check(" {\n      @interface engine\n    }");  // the way the language realises it
  for (const auto& f : right.findings()) EXPECT_NE(f.code, "E-RELATION-KIND") << f.message;
  bool realises = false;
  for (const auto& r : right.index().all()) realises |= r.kind == "realises" && r.from == "p@m::Api" && r.to == "p@engine";
  EXPECT_TRUE(realises);

  // DS-226: each relation joins exact pairs of kinds, never any of its sources to any of its targets
  const auto findings = [this](const std::string& text) {
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("b.ds", text));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    std::vector<std::string> kinds;
    for (const auto& f : model.findings())
      if (f.code == "E-RELATION-KIND") kinds.push_back(f.message);
    return kinds;
  };
  const std::string shop =
      "package s {\n  usecase UC-00001-1 buy(item: string) -> int {\n    title: Buy\n  }\n"
      "  interface shell {\n    kind: cli\n    operation buy(item: string) -> int : UC-00001-1\n  }\n"
      "  requirement REQ-00001-1: The shop shall sell\n";
  // a class that names a use case as its base: read whole, not as a class UC nobody designs, and told
  const std::vector<std::string> base = findings(shop + "  module m {\n    class Cart : UC-00001-1 { }\n  }\n}\n");
  ASSERT_EQ(base.size(), 1u);
  EXPECT_EQ(base[0], "s@m::Cart inherits UC-00001-1, a usecase: the language joins inherits only between other kinds");
  // a verification that names an interface: a verification realises nothing
  const std::vector<std::string> verification =
      findings(shop + "  verification VER-00001-1 {\n    @requirement REQ-00001-1\n    @interface shell\n    method: review\n  }\n}\n");
  ASSERT_EQ(verification.size(), 1u);
  EXPECT_EQ(verification[0], "VER-00001-1 realises s@shell, an interface: the language joins realises only between other kinds");
  // what the pairs keep: a class inheriting a class, realising an interface, an operation realising a use case
  EXPECT_TRUE(
      findings(shop + "  module m {\n    class Cart {\n      @interface shell\n    }\n    class Basket : Cart { }\n  }\n}\n").empty());
}

}  // namespace
