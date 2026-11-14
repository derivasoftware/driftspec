#include <gtest/gtest.h>

#include <string>

namespace ds {
namespace format {
const char* viewerTemplate();  // viewer/scene.html, compiled into ds by CMake
}  // namespace format
}  // namespace ds

namespace {

/// The page ds carries inside (REQ-00070-1): the viewer as written, with the places ds fills.
class TestViewerTemplate : public ::testing::Test {};

TEST_F(TestViewerTemplate, ThePageIsWholeAndHoldsThePlacesDsFills) {
  const std::string page = ds::format::viewerTemplate();
  EXPECT_EQ(page.rfind("<!doctype html>", 0), 0u);
  EXPECT_NE(page.find("</html>"), std::string::npos);
  for (const char* place : {"/*SCENE*/", "/*TITLE*/", "/*CSS*/", "/*FOCUS*/"}) EXPECT_NE(page.find(place), std::string::npos) << place;
}

}  // namespace
