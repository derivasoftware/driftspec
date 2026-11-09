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
  static ds::model::Model load(const std::string& dir) {
    ds::model::Model model;
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(dir)) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    return model;
  }

  static std::vector<std::string> workspaces() {
    std::vector<std::string> out{std::string(DRIFTSPEC_SOURCE_DIR) + "/design"};
    for (const auto& d : std::filesystem::directory_iterator(std::string(DRIFTSPEC_EXAMPLES_DIR))) {
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
      const ds::model::KindSpec& spec = ds::model::specOf(e.kind);
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
            << dir << " " << e.id << " inside a " << ds::model::kindName(parent);
    }
    for (const auto& r : model.relations()) {
      ++relations;
      const ds::model::RelationSpec* spec = ds::model::relationOf(r.kind);
      ASSERT_NE(spec, nullptr) << dir << " relation " << r.kind;
      const ds::model::Entity* from = model.find(r.from);
      const ds::model::Entity* to = model.find(r.to);
      if (!from || !to) continue;  // an end the checker reports as undeclared
      EXPECT_TRUE(ds::model::joins(*spec, from->kind, to->kind))
          << dir << " " << r.kind << " " << ds::model::kindName(from->kind) << " -> " << ds::model::kindName(to->kind) << " (" << r.from
          << " -> " << r.to << ")";
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
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  requirement REQ-00001-1 {\n"
                               "    title: The tool shall do one thing\n"
                               "    status: draft\n"
                               "    priority: high\n"
                               "  }\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, checkSteps).run(binder);
  std::vector<std::string> unknown;
  for (const auto& f : model.findings()) {
    if (f.code == "W-FIELD-UNKNOWN") unknown.push_back(f.message);
  }
  ASSERT_EQ(unknown.size(), 1u);
  EXPECT_NE(unknown[0].find("'priority'"), std::string::npos);
}

TEST_F(TestMetamodelConformance, ARelationTheLanguageDoesNotAllowIsReportedAndNotBound) {
  const auto check = [](const std::string& base) {
    ds::model::Model model;
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText(
        "a.ds", "package p {\n  interface engine {\n    kind: library\n  }\n  module m {\n    class Api" + base + "\n  }\n}\n"));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    return model;
  };
  const ds::model::Model wrong = check(" : engine { }");  // DS-93: a class that names an interface as its base
  std::vector<std::string> kind;
  for (const auto& f : wrong.findings())
    if (f.code == "E-RELATION-KIND") kind.push_back(f.message);
  ASSERT_EQ(kind.size(), 1u);
  EXPECT_NE(kind[0].find("@interface engine"), std::string::npos) << kind[0];
  for (const auto& r : wrong.relations()) EXPECT_FALSE(r.kind == "inherits" && r.from == "p@m::Api") << r.to;

  const ds::model::Model right = check(" {\n      @interface engine\n    }");  // the way the language realises it
  for (const auto& f : right.findings()) EXPECT_NE(f.code, "E-RELATION-KIND") << f.message;
  bool realises = false;
  for (const auto& r : right.relations()) realises |= r.kind == "realises" && r.from == "p@m::Api" && r.to == "p@engine";
  EXPECT_TRUE(realises);
}

}  // namespace
