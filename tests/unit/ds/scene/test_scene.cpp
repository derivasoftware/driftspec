#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "ds/emit/JsonSink.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Scene.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;

Entity entity(Kind kind, const std::string& id, const std::string& name, const std::string& scope) {
  Entity e;
  e.kind = kind;
  e.id = id;
  e.name = name;
  e.scope = scope;
  return e;
}

/// The structural scene of a small model: a package, a module, a class with a public and a
/// private member, a function (REQ-00069-2).
class TestScene : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::map<std::string, ds::scene::Node> byId;

  void SetUp() override {
    model.declare(entity(Kind::Package, "p", "p", ""));
    model.declare(entity(Kind::Module, "p@m", "m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "A", "p@m"));
    Entity run = entity(Kind::Method, "p@m::A::run", "run", "p@m::A");
    run.params = {{"name", "string"}};
    run.fields["returns"] = "std::vector<model::Entity>";
    run.location.line = 2;
    model.declare(run);
    Entity helper = entity(Kind::Method, "p@m::A::helper", "helper", "p@m::A");
    helper.fields["visibility"] = "-";
    helper.location.line = 3;
    model.declare(helper);
    Entity f = entity(Kind::Function, "p@m::load", "load", "p@m");
    f.params = {{"dir", "path"}};
    f.fields["returns"] = "Model";
    model.declare(f);
    model.relate({"satisfies", "p@m::A", "REQ-00001-1", {}, ""});
    for (const auto& n : ds::scene::Scene(model, metamodel).build().nodes) byId[n.id] = n;
  }

  static bool inside(const ds::layout::Box& in, const ds::layout::Box& out) {
    return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w && in.y + in.h <= out.y + out.h;
  }
};

TEST_F(TestScene, EveryBoxSitsInsideItsParent) {
  ASSERT_EQ(byId.count("p@m::A::run"), 1u);
  for (const auto& pair : byId) {
    const auto& n = pair.second;
    if (n.parent.empty()) continue;
    EXPECT_TRUE(inside(n.full, byId.at(n.parent).full)) << n.id;
  }
  EXPECT_EQ(byId.at("p@m::A").kind, "unit");
  EXPECT_EQ(byId.at("p@m").kind, "module");
  EXPECT_EQ(byId.at("p@m::A").requirements.size(), 1u);
}

TEST_F(TestScene, AMemberReadsAsItsSignatureUnqualified) {
  EXPECT_EQ(byId.at("p@m::A::run").label, "+ run(name) → vector<Entity>");
  EXPECT_EQ(byId.at("p@m::load#sig").label, "load(dir) → Model");
  EXPECT_EQ(byId.at("p@m::load").stereotype, "function");
}

/// One fixture per unit of ds::scene the tests look at: a node, its box, the document.
class TestNode : public TestScene {};
class TestBox : public TestScene {};
class TestSceneDocument : public TestScene {};

TEST_F(TestNode, APrivateMemberHasNoLeanPlaceAndItsClassShrinks) {
  EXPECT_FALSE(byId.at("p@m::A::helper").lean);
  EXPECT_TRUE(byId.at("p@m::A::run").lean);
  EXPECT_LT(byId.at("p@m::A").leanBox.h, byId.at("p@m::A").full.h);
}

TEST_F(TestBox, ARowIsAsWideAsItsUnitAndOneRowTall) {
  const auto& row = byId.at("p@m::A::run").full;
  const auto& unit = byId.at("p@m::A").full;
  EXPECT_DOUBLE_EQ(row.w, unit.w);
  EXPECT_GT(row.h, 0);
  EXPECT_LT(row.h, unit.h);
}

TEST_F(TestSceneDocument, TheWorldHoldsEveryNodeInBothVariants) {
  const auto doc = ds::scene::Scene(model, metamodel).build();
  for (const auto& n : doc.nodes) {
    EXPECT_TRUE(inside(n.full, doc.world)) << n.id;
    if (n.lean) {
      EXPECT_TRUE(inside(n.leanBox, doc.leanWorld)) << n.id;
    }
  }
}

TEST_F(TestScene, TheDocumentIsCompactAndSaysHowToReadItself) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const auto doc = nlohmann::json::parse(ds::format::SceneJson().write(ds::scene::Scene(model, metamodel).build(), {}, sink));
  EXPECT_EQ(doc["version"], 2);
  const auto& fields = doc["fields"]["node"];
  const auto column = [&fields](const std::string& name) {
    for (size_t i = 0; i < fields.size(); ++i)
      if (fields[i] == name) return i;
    return fields.size();
  };
  ASSERT_FALSE(doc["nodes"].empty());
  for (const auto& n : doc["nodes"]) {
    EXPECT_EQ(n[column("full")].size(), 4u) << n[column("id")];
    EXPECT_TRUE(n[column("parent")].is_number_integer());  // a node names its parent by index
    if (n[column("id")] == "p@m::A::helper") {
      EXPECT_TRUE(n[column("lean")].is_null());
    }
  }
  EXPECT_EQ(doc["world"].size(), 4u);
  EXPECT_EQ(doc["lean_world"].size(), 4u);
}

TEST_F(TestScene, ThePageIsTheViewerWithTheSceneAndTheTitleInside) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "a<b"}, sink);
  EXPECT_EQ(page.rfind("<!doctype html>", 0), 0u);
  EXPECT_NE(page.find("<canvas"), std::string::npos);
  const auto at = page.find("const PACKED='") + std::string("const PACKED='").size();
  const std::string scene = ds::format::common::unpack(page.substr(at, page.find('\'', at) - at));
  EXPECT_NE(scene.find("\"p@m::A::run\""), std::string::npos);  // the scene travels packed, and unpacks whole
  EXPECT_EQ(page.find("\"p@m::A::run\""), std::string::npos);
  EXPECT_NE(page.find("a&lt;b \u00b7 DriftSpec"), std::string::npos);
  EXPECT_EQ(page.find("/*SCENE*/"), std::string::npos);
  EXPECT_EQ(page.find("/*TITLE*/"), std::string::npos);
}

/// The links of a scene: each relation of the model between two of its nodes, with what produces it.
class TestLink : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::vector<ds::scene::Link> links;

  void SetUp() override {
    model.declare(entity(Kind::Package, "p", "p", ""));
    model.declare(entity(Kind::Module, "p@m", "m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "A", "p@m"));
    model.declare(entity(Kind::Class, "p@m::B", "B", "p@m"));
    Entity run = entity(Kind::Method, "p@m::A::run", "run", "p@m::A");
    model.declare(run);
    Entity b = entity(Kind::Attribute, "p@m::A::b_", "b_", "p@m::A");
    b.fields["type"] = "B";
    model.declare(b);
    model.declare(entity(Kind::Method, "p@m::B::go", "go", "p@m::B"));
    model.relate({"calls", "p@m::A::run::sequence::m0", "p@m::B::go", {}, ""});
    model.relate({"calls", "p@m::A::run::sequence::m3", "p@m::B::go", {}, ""});
    model.relate({"owns", "p@m::A::b_", "p@m::B", {}, "*"});
    model.relate({"uses", "p@m::A::run", "p@m::A", {}, ""});  // a member naming its own class stays inside the box
    links = ds::scene::Scene(model, metamodel).build().links;
  }

  const ds::scene::Link* link(const std::string& kind) const {
    for (const auto& l : links)
      if (l.kind == kind) return &l;
    return nullptr;
  }
};

TEST_F(TestLink, ACallGoesFromTheMemberWhoseBehaviourWritesItToTheMemberItCalls) {
  const auto* call = link("calls");
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->from, "p@m::A::run");
  EXPECT_EQ(call->to, "p@m::B::go");
  EXPECT_EQ(call->count, 2);
  ASSERT_EQ(call->sequences.size(), 1u);
  EXPECT_EQ(call->sequences[0], "p@m::A::run::sequence");
}

TEST_F(TestLink, OwnershipGoesFromTheAttributeWithItsMultiplicity) {
  const auto* owns = link("owns");
  ASSERT_NE(owns, nullptr);
  EXPECT_EQ(owns->from, "p@m::A::b_");
  EXPECT_EQ(owns->to, "p@m::B");
  EXPECT_EQ(owns->multiplicity, "*");
}

TEST_F(TestLink, ARelationInsideOneBoxIsNoLink) { EXPECT_EQ(link("uses"), nullptr); }

/// A diagram of the workspace, as the scene lists it: the nodes it shows.
class TestDiagramView : public TestScene {};

TEST_F(TestDiagramView, ADiagramListsTheNodesItShows) {
  Entity diagram = entity(Kind::Diagram, "diagram:CL_M", "CL_M", "");
  diagram.fields["kind"] = "class";
  diagram.fields["include"] = "p@m::*";
  model.declare(diagram);
  const auto doc = ds::scene::Scene(model, metamodel).build();
  ASSERT_EQ(doc.diagrams.size(), 1u);
  EXPECT_EQ(doc.diagrams[0].name, "CL_M");
  EXPECT_EQ(doc.diagrams[0].kind, "class");
  EXPECT_NE(std::find(doc.diagrams[0].nodes.begin(), doc.diagrams[0].nodes.end(), "p@m::A"), doc.diagrams[0].nodes.end());
}

/// A tour: a public behaviour walked along its scenario, each call expanded into the sequence of what it calls.
class TestTour : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::scene::SceneDocument doc;

  static Entity message(const std::string& sequence, int order, const std::string& op, const std::string& expands) {
    Entity m = entity(Kind::Message, sequence + "::m" + std::to_string(order), op, sequence);
    m.fields["op"] = op;
    m.fields["order"] = std::to_string(order);
    m.fields["result"] = op + "ed";
    if (!expands.empty()) m.fields["expands"] = expands;
    return m;
  }

  void SetUp() override {
    model.declare(entity(Kind::Package, "p", "p", ""));
    model.declare(entity(Kind::Module, "p@m", "m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "A", "p@m"));
    model.declare(entity(Kind::Class, "p@m::B", "B", "p@m"));
    model.declare(entity(Kind::Method, "p@m::A::run", "run", "p@m::A"));
    model.declare(entity(Kind::Method, "p@m::B::go", "go", "p@m::B"));
    model.declare(entity(Kind::Method, "p@m::B::step", "step", "p@m::B"));
    model.declare(entity(Kind::Sequence, "p@m::A::run::sequence", "run", "p@m"));
    model.declare(entity(Kind::Sequence, "p@m::B::go::sequence", "go", "p@m"));
    model.declare(message("p@m::A::run::sequence", 0, "go", "p@m::B::go::sequence"));
    model.declare(message("p@m::B::go::sequence", 0, "step", ""));
    model.declare(message("p@m::B::go::sequence", 1, "step", ""));
    model.relate({"calls", "p@m::A::run::sequence::m0", "p@m::B::go", {}, ""});
    model.relate({"calls", "p@m::B::go::sequence::m0", "p@m::B::step", {}, ""});
    model.relate({"calls", "p@m::B::go::sequence::m1", "p@m::B::step", {}, ""});
    doc = ds::scene::Scene(model, metamodel).build();
  }

  int behaviour(const std::string& id) const {
    for (size_t i = 0; i < doc.behaviours.size(); ++i)
      if (doc.behaviours[i].id == id) return static_cast<int>(i);
    return -1;
  }
};

class TestBehaviour : public TestTour {};
class TestCall : public TestTour {};

TEST_F(TestCall, ACallKeepsItsLabelAndWhatComesBack) {
  const auto& calls = doc.behaviours[static_cast<size_t>(behaviour("p@m::A::run"))].calls;
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].to, "p@m::B::go");
  EXPECT_EQ(calls[0].label, "go()");
  EXPECT_EQ(calls[0].result, "goed");
}

TEST_F(TestBehaviour, EachBehaviourIsWrittenOnceAndACallNamesTheOneItGoesOnInto) {
  const int run = behaviour("p@m::A::run"), go = behaviour("p@m::B::go");
  ASSERT_GE(run, 0);
  ASSERT_GE(go, 0);
  ASSERT_EQ(doc.behaviours[static_cast<size_t>(run)].calls.size(), 1u);
  EXPECT_EQ(doc.behaviours[static_cast<size_t>(run)].calls[0].expands, go);
  EXPECT_EQ(doc.behaviours[static_cast<size_t>(go)].calls.size(), 2u);
  std::set<std::string> seen;
  for (const auto& b : doc.behaviours) EXPECT_TRUE(seen.insert(b.id).second) << b.id;
}

TEST_F(TestTour, AMethodWithABehaviourIsATourOffered) {
  bool offered = false;
  for (const auto& t : doc.tours) offered |= t.behaviour == behaviour("p@m::A::run") && t.group == "method";
  EXPECT_TRUE(offered);  // how it is replayed: tests/integration/viewer/tours.test.ts, on the viewer's own code (DS-130)
}

/// The requirements of a small model as the viewer lists them (REQ-00078-1): R-2 refines R-1,
/// the class A answers to R-2, and V-1 proves it with a test.
class TestRequirement : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::scene::SceneDocument doc;

  void SetUp() override {
    model.declare(entity(Kind::Package, "p", "p", ""));
    model.declare(entity(Kind::Module, "p@m", "m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "A", "p@m"));
    Entity top = entity(Kind::Requirement, "R-1", "R-1", "p@");
    top.fields["title"] = "The system shall greet";
    model.declare(top);
    Entity low = entity(Kind::Requirement, "R-2", "R-2", "p@m");
    low.fields["title"] = "The greeter shall build a greeting";
    low.fields["status"] = "approved";
    model.declare(low);
    Entity proof = entity(Kind::Verification, "V-1", "V-1", "p@m");
    proof.fields["method"] = "test";
    proof.fields["tests"] = "tests/a.cpp::TestA.Greets\ntests/a.cpp::TestA.Rejects";
    model.declare(proof);
    model.relate({"refines", "R-2", "R-1", {}, ""});
    model.relate({"satisfies", "p@m::A", "R-2", {}, ""});
    model.relate({"verifies", "V-1", "R-2", {}, ""});
    doc = ds::scene::Scene(model, metamodel).build();
  }

  const ds::scene::Requirement& requirement(const std::string& id) const {
    return *std::find_if(doc.requirements.begin(), doc.requirements.end(), [&](const auto& r) { return r.id == id; });
  }
};

TEST_F(TestRequirement, ARequirementKnowsTheOneItRefinesWhereItIsWrittenAndWhatAnswersToIt) {
  ASSERT_EQ(doc.requirements.size(), 2u);
  EXPECT_EQ(requirement("R-2").parent, "R-1");
  EXPECT_EQ(requirement("R-2").scope, "p@m");
  EXPECT_EQ(requirement("R-2").nodes, std::vector<std::string>{"p@m::A"});
  EXPECT_EQ(requirement("R-2").status, "approved");
  EXPECT_EQ(requirement("R-1").parent, "");
  EXPECT_TRUE(requirement("R-1").nodes.empty());
}

/// A verification of the same model: its method, its tests one per line, what it proves.
class TestVerification : public TestRequirement {};

TEST_F(TestVerification, AVerificationCarriesItsTestsOnePerLineAndWhatItProves) {
  ASSERT_EQ(doc.verifications.size(), 1u);
  const auto& v = doc.verifications[0];
  EXPECT_EQ(v.method, "test");
  EXPECT_EQ(v.tests, (std::vector<std::string>{"tests/a.cpp::TestA.Greets", "tests/a.cpp::TestA.Rejects"}));
  EXPECT_EQ(v.requirements, std::vector<std::string>{"R-2"});
}

/// A use case of a small model, listed with the requirements (REQ-00083-1): its outcomes, the
/// operation that realises it and the requirement that binds it.
class TestOutcome : public ::testing::Test {};

TEST_F(TestOutcome, AUseCaseCarriesItsOutcomesTheDefaultFirst) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  model.declare(entity(Kind::Package, "p", "p", ""));
  model.declare(entity(Kind::Interface, "p@shell", "shell", "p@"));
  model.declare(entity(Kind::Operation, "p@shell::greet", "greet", "p@shell"));
  model.declare(entity(Kind::Requirement, "R-1", "R-1", "p@"));
  Entity uc = entity(Kind::UseCase, "UC-1", "UC-1", "p@");
  uc.fields["title"] = "Greet";
  uc.fields["operation"] = "greet";
  model.declare(uc);
  Entity sad = entity(Kind::Outcome, "UC-1::empty", "empty", "UC-1");
  sad.fields["text"] = "nobody is greeted";
  model.declare(sad);
  Entity happy = entity(Kind::Outcome, "UC-1::default", "default", "UC-1");
  happy.fields["text"] = "a greeting";
  model.declare(happy);
  model.relate({"realises", "p@shell::greet", "UC-1", {}, ""});
  model.relate({"satisfies", "UC-1", "R-1", {}, ""});
  const auto doc = ds::scene::Scene(model, metamodel).build();
  const auto it = std::find_if(doc.requirements.begin(), doc.requirements.end(), [](const auto& r) { return r.id == "UC-1"; });
  ASSERT_NE(it, doc.requirements.end());
  EXPECT_EQ(it->kind, "usecase");
  EXPECT_EQ(it->signature, "greet()");
  ASSERT_EQ(it->outcomes.size(), 2u);
  EXPECT_EQ(it->outcomes[0].name, "default");
  EXPECT_EQ(it->outcomes[1].text, "nobody is greeted");
  EXPECT_EQ(it->binds, std::vector<std::string>{"R-1"});
}

}  // namespace
