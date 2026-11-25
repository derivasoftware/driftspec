#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// A finding names its subject (REQ-00240-1, DS-426): the rule a rule's finding is about, a breach included, by
/// identity; a finding about a file names none. The report and the export write the subject only when there is one.
class TestFindingSubject : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::api::Api api{metamodel};
  const ds::api::Files files{{"a.ds",
                              "package p {\n"
                              "  requirement REQ-00001-1: The boxes shall be few\n"
                              "  module m {\n"
                              "    class A { }\n"
                              "    rule none {\n"
                              "      @requirement REQ-00001-1\n"
                              "      for class c in p@m\n"
                              "      never c is class\n"
                              "    }\n"
                              "    rule nothing {\n"
                              "      for class c in p@m\n"
                              "    }\n"
                              "    rule unknown {\n"
                              "      @requirement REQ-00001-1\n"
                              "      for gizmo g in p@m\n"
                              "      never g is class\n"
                              "    }\n"
                              "  }\n"
                              "}\n"},
                             {"b.ds", "package q {\n  module {\n"}};

  static std::string subjectOf(const nlohmann::json& findings, const std::string& code) {
    for (const auto& f : findings)
      if (f["code"] == code) return f.value("subject", std::string("<none>"));
    return "<absent>";
  }
};

TEST_F(TestFindingSubject, ARulesFindingsNameTheRuleABreachIncluded) {
  const nlohmann::json report = nlohmann::json::parse(api.check(files));
  EXPECT_EQ(subjectOf(report["findings"], "E-RULE-BROKEN"), "p@m::none");             // the breach's entities are the breach's
  EXPECT_EQ(subjectOf(report["coverage"], "W-RULE-NO-REQUIREMENT"), "p@m::nothing");  // coverage names its subject too
  EXPECT_EQ(subjectOf(report["findings"], "W-RULE-NO-CLAIM"), "p@m::nothing");
  EXPECT_EQ(subjectOf(report["findings"], "E-RULE-UNKNOWN"), "p@m::unknown");
}

TEST_F(TestFindingSubject, AFindingAboutAFileNamesNoneAndTheKeyIsLeftOut) {
  const nlohmann::json report = nlohmann::json::parse(api.check(files));
  EXPECT_EQ(subjectOf(report["findings"], "E-SYNTAX"), "<none>");  // no key at all, never an empty one
  for (const char* list : {"findings", "coverage"})
    for (const auto& f : report[list])
      if (f.contains("subject")) EXPECT_FALSE(f["subject"].get<std::string>().empty());
}

TEST_F(TestFindingSubject, TheExportWritesTheSubjectAsTheReportDoes) {
  const nlohmann::json model = nlohmann::json::parse(api.exportModel(files));
  EXPECT_EQ(subjectOf(model["findings"], "E-RULE-BROKEN"), "p@m::none");
  EXPECT_EQ(subjectOf(model["findings"], "E-SYNTAX"), "<none>");
}

}  // namespace
