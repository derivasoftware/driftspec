#include <gtest/gtest.h>

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/SinkFactory.hpp"

namespace {

/// The one place that names the sinks (DS-397): a door is handed the JSON sink as an ISink, indented or compact.
TEST(TestSinkFactory, TheJsonSinkIsHandedOutAsASink) {
  const auto sink = ds::emit::SinkFactory::json(false);
  ASSERT_NE(sink, nullptr);
  EXPECT_NE(dynamic_cast<const ds::emit::JsonSink*>(sink.get()), nullptr);
  sink->beginRecord().field("a").integer(1).field("b").beginList().value("x").endList().endRecord();
  EXPECT_EQ(sink->result(), R"({"a":1,"b":["x"]})");
}

TEST(TestSinkFactory, APrettySinkIndentsByTwoSpaces) {
  const auto sink = ds::emit::SinkFactory::json(true);
  sink->beginRecord().field("a").integer(1).endRecord();
  EXPECT_EQ(sink->result(), "{\n  \"a\": 1\n}");
}

}  // namespace
