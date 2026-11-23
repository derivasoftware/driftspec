#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Browser.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The engine for a page in a browser (REQ-00113-1): one request of texts in, one document out.
class TestBrowser : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  nlohmann::json ask(const nlohmann::json& files) const {
    return nlohmann::json::parse(ds::api::Browser(metamodel).answer(nlohmann::json{{"files", files}}.dump()));
  }
  const nlohmann::json shop{{"shop.ds",
                             "package shop {\n"
                             "  requirement SREQ-00001-1: The shop shall sell\n"
                             "  module store {\n"
                             "    class Cart {\n"
                             "      @requirement SREQ-00001-1\n"
                             "      + total() -> int\n"
                             "    }\n"
                             "  }\n"
                             "}\n"},
                            {"diagrams.ds", "diagram CL_Store {\n  kind: class\n  include: shop@store::*\n}\n"}};
};

TEST_F(TestBrowser, AWorkspaceAnswersItsReportItsIdentitiesAndWhereEachIsWritten) {
  const auto out = ask(shop);
  EXPECT_TRUE(out["report"]["findings"].empty());
  EXPECT_NE(std::find(out["entities"].begin(), out["entities"].end(), "shop@store::Cart"), out["entities"].end());
  EXPECT_EQ(out["locations"]["shop@store::Cart"][0], "shop.ds");
  EXPECT_EQ(out["locations"]["shop@store::Cart"][1], 4);
  EXPECT_EQ(out["locations"]["shop@store::Cart"][2], 7);
}

TEST_F(TestBrowser, ADiagramComesDrawnWithWhatItAsksFor) {
  const auto out = ask(shop);
  ASSERT_EQ(out["diagrams"].size(), 1u);
  const auto& d = out["diagrams"][0];
  EXPECT_EQ(d["id"], "CL_Store");
  EXPECT_EQ(d["kind"], "class");
  EXPECT_EQ(d["include"], "shop@store::*");
  EXPECT_EQ(d["svg"].get<std::string>().rfind("<svg", 0), 0u);
}

TEST_F(TestBrowser, AnArtefactComesAsARowWithWhatKeepsIt) {
  const auto out = ask(shop);
  ASSERT_EQ(out["artefacts"].size(), 1u);
  const auto& row = out["artefacts"][0];
  EXPECT_EQ(row["id"], "SREQ-00001-1");
  EXPECT_EQ(row["level"], "system");
  EXPECT_EQ(row["keptBy"], nlohmann::json::array({"shop@store::Cart"}));
}

TEST_F(TestBrowser, ARequestThatCannotBeReadIsAnsweredWithAnError) {
  const auto out = nlohmann::json::parse(ds::api::Browser(metamodel).answer("not json"));
  EXPECT_TRUE(out.contains("error"));
}

}  // namespace
