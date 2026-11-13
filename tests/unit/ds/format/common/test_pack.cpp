#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/format/Format.hpp"
#include "ds/scene/Document.hpp"

namespace {

/// How the scene travels inside a page: deflated and written in base64, as compact JSON that says
/// how to read itself.

TEST(TestPack, TextDeflatedAndWrittenInBase64) {
  const std::string text = std::string(5000, 'a') + " áé </script> \"quoted\" " + std::string(300, 'z');
  const std::string packed = ds::format::common::pack(text);
  EXPECT_LT(packed.size(), text.size() / 4);  // deflate: much smaller, even in base64
  EXPECT_EQ(packed.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="), std::string::npos);
}

TEST(TestUnpack, ThePackGivesBackItsTextAndAnythingElseNothing) {
  const std::string text = "the scene á </script>";
  EXPECT_EQ(ds::format::common::unpack(ds::format::common::pack(text)), text);
  EXPECT_EQ(ds::format::common::unpack("not a pack"), "");
}

TEST(TestEncode, EveryRecordIsAnArrayWhoseColumnsTheFieldsName) {
  ds::scene::SceneDocument doc;
  ds::scene::Node node;
  node.id = "p@m::C";
  node.kind = "unit";
  node.label = "C";
  doc.nodes.push_back(node);
  const auto json = nlohmann::json::parse(ds::format::common::encode(doc, false));
  EXPECT_EQ(json["version"], 2);
  const auto columns = json["fields"]["node"];
  ASSERT_EQ(columns[0], "id");
  ASSERT_EQ(json["nodes"].size(), 1u);
  EXPECT_EQ(json["nodes"][0][0], "p@m::C");  // the id is the first column, as fields says
}

}  // namespace
