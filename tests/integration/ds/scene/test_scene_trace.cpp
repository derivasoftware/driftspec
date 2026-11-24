#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The requirements a reader lists in the viewer (REQ-00078-1), over examples/hola: SREQ-00001-1
/// is refined by REQ-00001-1, which Greeter answers to and VER-00001-1 proves with two tests.
class TestSceneTrace : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::scene::SceneDocument doc;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola")) binder.bind(f);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    doc = ds::scene::Scene(model, metamodel).build();
  }

  const ds::scene::Requirement& requirement(const std::string& id) const {
    for (const auto& r : doc.requirements)
      if (r.id == id) return r;
    throw std::runtime_error("no requirement " + id);
  }

  const ds::scene::Verification& verification(const std::string& id) const {
    for (const auto& v : doc.verifications)
      if (v.id == id) return v;
    throw std::runtime_error("no verification " + id);
  }

  static bool has(const std::vector<std::string>& in, const std::string& what) { return std::find(in.begin(), in.end(), what) != in.end(); }

  static std::string packedIn(const std::string& page) {
    const auto at = page.find("const PACKED='") + std::string("const PACKED='").size();
    return ds::format::common::unpack(page.substr(at, page.find('\'', at) - at));
  }
};

TEST_F(TestSceneTrace, EveryRequirementAndConstraintIsListedWithTheOneItRefines) {
  EXPECT_EQ(doc.requirements.size(), 9u);  // three system requirements, three requirements, one constraint, two use cases
  EXPECT_EQ(requirement("REQ-00001-1").parent, "SREQ-00001-1");
  EXPECT_EQ(requirement("REQ-00003-1").parent, "SREQ-00003-1");
  EXPECT_EQ(requirement("SREQ-00001-1").parent, "");
  EXPECT_EQ(requirement("CST-00001-1").kind, "constraint");
  EXPECT_EQ(requirement("REQ-00001-1").title, "The greeter shall build the greeting of a name");
  EXPECT_EQ(requirement("REQ-00001-1").status, "draft");
  EXPECT_EQ(requirement("REQ-00001-1").scope, "hola@core");  // where it is written
}

TEST_F(TestSceneTrace, WhatAnswersToARequirementIsTheNodeThatStandsForIt) {
  EXPECT_TRUE(has(requirement("REQ-00001-1").nodes, "hola@core::Greeter"));
  EXPECT_TRUE(has(requirement("SREQ-00001-1").nodes, "hola@shell::greet"));  // an operation of an interface
  EXPECT_TRUE(has(requirement("REQ-00002-1").nodes, "hola@app::main"));
  EXPECT_TRUE(has(requirement("CST-00001-1").nodes, "hola@web"));  // a constraint, through the views of web
}

TEST_F(TestSceneTrace, AVerificationCarriesItsMethodItsTestsAndWhatItProves) {
  const auto& v = verification("VER-00001-1");
  EXPECT_EQ(v.method, "test");
  EXPECT_EQ(v.requirements, std::vector<std::string>{"REQ-00001-1"});
  ASSERT_EQ(v.tests.size(), 2u);
  EXPECT_EQ(v.tests[0], "tests/unit/core/test_greeter.cpp::TestGreeter.GreetsByName");
}

TEST_F(TestSceneTrace, AUseCaseIsListedWithItsContractItsOutcomesAndWhatRealisesIt) {
  const auto& generic = requirement("UC-00001-1");
  EXPECT_EQ(generic.kind, "usecase");
  EXPECT_EQ(generic.signature, "greet(name: string) -> core::Greeting");
  ASSERT_EQ(generic.outcomes.size(), 2u);
  EXPECT_EQ(generic.outcomes[0].name, "default");  // the ending that goes well comes first
  EXPECT_EQ(generic.outcomes[1].returns, "none");
  EXPECT_EQ(generic.binds, (std::vector<std::string>{"SREQ-00001-1", "SREQ-00002-1"}));
  EXPECT_TRUE(has(generic.nodes, "hola@shell::greet"));       // the operation that realises it
  EXPECT_EQ(requirement("UC-00002-1").parent, "UC-00001-1");  // the specific under the generic
  EXPECT_TRUE(has(requirement("UC-00002-1").nodes, "hola@web::greet"));
}

TEST_F(TestSceneTrace, ThePageListsTheRequirementsInTheBottomPanel) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "hola"}, sink);
  EXPECT_NE(page.find("data-tab=\"reqs\""), std::string::npos);  // a tab beside the tours
  const std::string scene = packedIn(page);
  EXPECT_NE(scene.find("\"requirement\":[\"id\",\"kind\",\"title\",\"status\",\"text\",\"parent\",\"scope\",\"nodes\",\"signature\","
                       "\"outcomes\",\"binds\"]"),
            std::string::npos);
  EXPECT_NE(scene.find("\"VER-00001-1\""), std::string::npos);
}

}  // namespace
