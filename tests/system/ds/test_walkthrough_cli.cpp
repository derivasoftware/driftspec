// The viewer page as the command line writes it (DS-483): `ds render <ws> -f html` on a workspace with a walkthrough
// carries its cards and its mode, and is the page the engine's door writes; a drawing alone goes the same one path.
// ds is run from outside, as a user runs it; the engine is linked only to read the page and to say what it should be.
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/format/Format.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// What ds writes on stdout.
std::string ds(const std::string& args) {
  const std::string command = std::string(DRIFTSPEC_DS_BIN) + " " + args + " 2>/dev/null";
  std::string out;
  if (FILE* p = ::popen(command.c_str(), "r")) {
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof buffer, p)) > 0) out.append(buffer, n);
    ::pclose(p);
  }
  return out;
}

/// What a page packs under a name, inflated; "" when it packs nothing so named.
std::string packed(const std::string& page, const std::string& name) {
  const std::string mark = "const " + name + "='";
  const size_t from = page.find(mark);
  if (from == std::string::npos) return "";
  const size_t start = from + mark.size(), end = page.find('\'', start);
  return ds::format::common::Packing::unpack(page.substr(start, end - start));
}

/// The .ds files of a workspace, read as the engine's door is given them.
ds::api::Files filesOf(const std::string& dir) {
  ds::api::Files out;
  for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
    if (!e.is_regular_file() || e.path().extension() != ".ds") continue;
    std::ifstream in(e.path(), std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    out[e.path().string()] = text.str();
  }
  return out;
}

const std::string kShop = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/shop";

TEST(TestWalkthroughCli, TheCommandLinesViewerPageCarriesTheCardsAndIsTheDoorsPage) {
  const std::string page = ds("render " + kShop + " -f html");
  ASSERT_NE(page.find("const DRAWINGS='"), std::string::npos) << "the page carries no cards";
  EXPECT_NE(packed(page, "VIEWER").find("function walkDecode("), std::string::npos) << "nor the mode that plays them";
  const ds::model::Metamodel metamodel;
  const auto ws = ds::api::Workspace::madeFor(metamodel, filesOf(kShop), ds::api::Output::Viewer);
  EXPECT_EQ(packed(page, "DRAWINGS"), packed(ws.page("shop", {}), "DRAWINGS"));  // the same cards as the door's page
}

TEST(TestWalkthroughCli, ADiagramDrawnAloneIsItsCardByteForByte) {
  const std::string page = ds("render " + kShop + " -f html");
  const auto cards = nlohmann::json::parse(packed(page, "DRAWINGS"));
  for (const char* d : {"CL_Shop", "SEQ_Buy", "REQ_Shop"}) {
    ASSERT_TRUE(cards.contains(d)) << d << " has no card";
    EXPECT_EQ(cards.at(d).get<std::string>(), ds("render " + kShop + " " + d + " -f svg"))
        << d << "'s card is not what render -f svg draws";
  }
}

}  // namespace
