#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/format/Format.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

const char* const kShop =
    "package shop {\n"
    "  module desk {\n"
    "    requirement REQ-00001-1 {\n"
    "      title: The desk shall sell\n"
    "      status: draft\n"
    "    }\n"
    "    class Desk {\n"
    "      @requirement REQ-00001-1\n"
    "      + sell(item: string) -> int\n"
    "    }\n"
    "  }\n"
    "}\n"
    "diagram CL_Shop {\n"
    "  kind: class\n"
    "  include: shop@*\n"
    "}\n"
    "diagram REQ_Shop {\n"
    "  kind: requirement\n"
    "  include: shop@*\n"
    "}\n";

const char* const kWalk =
    "diagram WALK_Shop {\n"
    "  kind: walkthrough\n"
    "  title: The shop\n"
    "  step The promise {\n"
    "    show: REQ_Shop\n"
    "    focus: REQ-00001-1\n"
    "    at: REQ-00001-1\n"
    "    text: The desk sells.\n"
    "  }\n"
    "  step Who sells {\n"
    "    show: CL_Shop\n"
    "    focus: shop@desk::Desk\n"
    "    text: The desk.\n"
    "  }\n"
    "}\n";

/// What a page packs under a name, inflated; "" when it packs nothing so named.
std::string packed(const std::string& page, const std::string& name) {
  const std::string mark = "const " + name + "='";
  const size_t from = page.find(mark);
  if (from == std::string::npos) return "";
  const size_t start = from + mark.size(), end = page.find('\'', start);
  return ds::format::common::Packing::unpack(page.substr(start, end - start));
}

/// A page whose workspace writes a walkthrough carries the mode that plays it and every diagram drawn exactly as
/// `ds render <ws> <D> -f svg` draws it; a page with none is written as it was (REQ-00244-1, DS-470).
TEST(TestWalkthroughPage, EveryCardIsTheDiagramAsRenderDrawsItByteForByte) {
  const ds::model::Metamodel metamodel;
  const auto ws = ds::api::Workspace::madeFor(metamodel, {{"shop.ds", kShop}, {"walk.ds", kWalk}});
  const std::string page = ws.page("shop", {});
  const auto cards = nlohmann::json::parse(packed(page, "DRAWINGS"));
  ASSERT_EQ(cards.size(), 2u);  // every diagram but the walkthrough
  for (const auto& d : {"CL_Shop", "REQ_Shop"}) EXPECT_EQ(cards.at(d).get<std::string>(), ws.render(d, "svg", 0)) << d;
  EXPECT_NE(packed(page, "VIEWER").find("function walkDecode("), std::string::npos);  // the mode that plays it
}

TEST(TestWalkthroughPage, APageWithNoWalkthroughCarriesNeitherTheCardsNorTheMode) {
  const ds::model::Metamodel metamodel;
  const auto ws = ds::api::Workspace::madeFor(metamodel, {{"shop.ds", kShop}});
  const std::string page = ws.page("shop", {});
  EXPECT_EQ(page.find("const DRAWINGS="), std::string::npos);
  EXPECT_EQ(packed(page, "VIEWER").find("walkDecode"), std::string::npos);
}

TEST(TestWalkthroughPage, AStepLightsARequirementByItsIdentity) {
  const ds::model::Metamodel metamodel;
  const auto ws = ds::api::Workspace::madeFor(metamodel, {{"shop.ds", kShop}, {"walk.ds", kWalk}});
  const auto scene = nlohmann::json::parse(packed(ws.page("shop", {}), "PACKED"));
  const auto& step = scene["walkthroughs"].at(0)[2].at(0);
  EXPECT_EQ(step[3], nlohmann::json::array({"REQ-00001-1"}));  // no node of the map, lit all the same
  EXPECT_EQ(step[5], "REQ-00001-1");
  EXPECT_NE(ws.render("REQ_Shop", "svg", 0).find("data-id=\"REQ-00001-1\""), std::string::npos);  // as its drawing names it
}

}  // namespace
