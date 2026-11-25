#include <gtest/gtest.h>

#include <string>

#include "ds/format/Format.hpp"

namespace {

/// How the scene and the viewer travel inside a page: deflated and written in base64.

TEST(TestPacking, TextDeflatedAndWrittenInBase64) {
  const std::string text = std::string(5000, 'a') + " áé </script> \"quoted\" " + std::string(300, 'z');
  const std::string packed = ds::format::common::Packing::pack(text);
  EXPECT_LT(packed.size(), text.size() / 4);  // deflate: much smaller, even in base64
  EXPECT_EQ(packed.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="), std::string::npos);
}

TEST(TestPacking, ThePackGivesBackItsTextAndAnythingElseNothing) {
  const std::string text = "the scene á </script>";
  EXPECT_EQ(ds::format::common::Packing::unpack(ds::format::common::Packing::pack(text)), text);
  EXPECT_EQ(ds::format::common::Packing::unpack("not a pack"), "");
}

}  // namespace
