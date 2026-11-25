#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

const char* kShop =
    "package p {\n"
    "  module m {\n"
    "    class Vault {\n"
    "      + take(n: int) -> int\n"
    "    }\n"
    "    class Desk {\n"
    "      - vault: Vault\n"
    "      + pay(n: int) -> int\n"
    "    }\n"
    "    sequence Desk::pay {\n"
    "      alt there is enough {\n"
    "        self -> vault.take(n) -> got\n"
    "      } else / refused {\n"
    "        return 0\n"
    "      }\n"
    "      return got\n"
    "    }\n"
    "  }\n"
    "}\n";

/// A workspace loaded once and asked as often as needed (REQ-00107-2, DS-173): the engine whole,
/// from the texts to each answer.
class TestWorkspace : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::api::Workspace ws{metamodel, {{"shop.ds", kShop}}};
  static nlohmann::json json(const std::string& text) { return nlohmann::json::parse(text); }
};

TEST_F(TestWorkspace, ItAnswersWhatTheCommandLineAnswers) {
  EXPECT_EQ(ws.report(), ds::api::Api(metamodel).check({{"shop.ds", kShop}}));
  EXPECT_EQ(ws.exportModel(), ds::api::Api(metamodel).exportModel({{"shop.ds", kShop}}));
  EXPECT_EQ(json(ws.find("p@m::Desk"))["kind"], "class");
  EXPECT_EQ(ws.find("p@m::Nobody"), "null");
}

TEST_F(TestWorkspace, OneHopIsTheIndexOfTheModel) {
  bool owns = false;
  for (const auto& r : json(ws.from("p@m::Desk::vault"))) owns |= r["kind"] == "owns" && r["to"] == "p@m::Vault";
  EXPECT_TRUE(owns);
  EXPECT_FALSE(json(ws.to("p@m::Vault")).empty());
  EXPECT_EQ(json(ws.members("p@m")), (nlohmann::json{"p@m::Desk", "p@m::Desk::pay::sequence", "p@m::Vault"}));
  EXPECT_EQ(json(ws.parent("p@m::Desk::vault")), "p@m::Desk");
}

TEST_F(TestWorkspace, ABehaviourIsAnsweredAsItsTreePathAndWalk) {
  const std::string seq = "p@m::Desk::pay::sequence";
  const auto tree = json(ws.tree(seq));
  ASSERT_EQ(tree["frames"].size(), 1u);
  EXPECT_EQ(tree["frames"][0]["kind"], "alt");
  EXPECT_EQ(tree["frames"][0]["else_scenario"], "refused");
  EXPECT_EQ(tree["messages"].size(), 3u);
  EXPECT_EQ(json(ws.steps(seq, "refused")).size(), 1u);  // the else ends at its return
  EXPECT_EQ(json(ws.steps(seq, "*")).size(), 3u);        // every branch
  EXPECT_FALSE(json(ws.walk(seq, "", 1)).empty());
}

TEST_F(TestWorkspace, ItAsksAsDsQueryAndDrawsAsDsRender) {
  bool desk = false;
  for (const auto& a : json(ws.ask("entities", "p@m"))) desk |= a["id"] == "p@m::Desk";
  EXPECT_TRUE(desk);
  EXPECT_EQ(json(ws.ask("nonsense", ""))[0]["kind"], "error");
  EXPECT_EQ(ws.render("CL_None", "svg", 0), "");
  EXPECT_NE(ws.page("shop", {"p@m::Desk"}).find("<html"), std::string::npos);
}

TEST_F(TestWorkspace, ADiagramAskedInAFormatDsDoesNotWriteFailsNotImplemented) {
  // a pair ds does not write fails before anything is drawn, naming the document, the technology and what there is
  // (DS-398): it does not draw text instead
  try {
    (void)ws.render("CL_Any", "yaml", 0);
    FAIL() << "a diagram was drawn in yaml";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.document(), "diagram");
    EXPECT_EQ(e.technology(), "yaml");
    EXPECT_EQ(std::string(e.what()), "the diagram is not written in yaml; there is: cells ir layout svg text");
  }
  // the engine's render draws svg, the IR, or text for any other name, as it always has: it asks no pair there is none of
  EXPECT_EQ(ds::api::Api(metamodel).render({{"shop.ds", kShop}}, "CL_Any", "yaml", 0), "");
}

TEST_F(TestWorkspace, TwoLoadedAtOnceEachKeepItsOwnModel) {
  std::string after = kShop;
  after.replace(after.find("+ take(n: int) -> int"), 21, "+ take(n: int) -> bool");
  const ds::api::Workspace later{metamodel, {{"shop.ds", after}}};
  // the second load answers from its own model, and the first is as it was
  EXPECT_NE(later.find("p@m::Vault::take").find("bool"), std::string::npos);
  EXPECT_EQ(ws.find("p@m::Vault::take").find("bool"), std::string::npos);
}

TEST_F(TestWorkspace, AnIdentityIsWrittenAsTheModelWritesIt) {
  EXPECT_EQ(ds::api::Identity::qualify("p@m", "C"), "p@m::C");
  EXPECT_EQ(ds::api::Identity::outer("p@m::C"), "p@m");
  EXPECT_EQ(ds::api::Identity::boxOf("p@"), "p");
  EXPECT_EQ(ds::api::Identity::behaviourOf("p@m::C::run::sequence/s::m2"), "p@m::C::run::sequence/s");
  EXPECT_EQ(ds::api::Identity::subjectOf("p@m::C::run::sequence"), "p@m::C::run");
  EXPECT_EQ(ds::api::Identity::scenarioOf("p@m::C::run::sequence/s"), "s");
  EXPECT_EQ(ds::api::Identity::withoutRevision("REQ-00001-2"), "REQ-00001");
}

}  // namespace
