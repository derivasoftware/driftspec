#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
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
    ds::check::Checker(model).run(binder);
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
  ds::check::Checker(model).run(binder);
  std::vector<std::string> unknown;
  for (const auto& f : model.findings()) {
    if (f.code == "W-FIELD-UNKNOWN") unknown.push_back(f.message);
  }
  ASSERT_EQ(unknown.size(), 1u);
  EXPECT_NE(unknown[0].find("'priority'"), std::string::npos);
}

}  // namespace
