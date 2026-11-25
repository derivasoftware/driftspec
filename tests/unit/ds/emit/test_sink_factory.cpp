#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"

namespace {

/// The one place that names the sinks (DS-397, DS-398): a door is handed the sink of its technology as an ISink,
/// indented or compact, and a technology ds has no sink of fails with NotImplemented.
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

TEST(TestSinkFactory, EveryTechnologyNamedIsTheSinkOfItsName) {
  EXPECT_EQ(ds::emit::SinkFactory::names(), std::vector<std::string>{"json"});
  for (const auto& technology : ds::emit::SinkFactory::names()) {
    for (bool pretty : {false, true}) {
      const auto sink = ds::emit::SinkFactory::of(technology, pretty);
      const auto made = ds::emit::SinkFactory::json(pretty);
      sink->beginRecord().field("a").integer(1).endRecord();
      made->beginRecord().field("a").integer(1).endRecord();
      EXPECT_EQ(sink->result(), made->result()) << technology;
    }
  }
}

TEST(TestSinkFactory, ATechnologyWithNoSinkFailsNotImplementedNamingItAndWhatThereIs) {
  try {
    (void)ds::emit::SinkFactory::of("yaml", false);
    FAIL() << "a technology with no sink was made";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.technology(), "yaml");
    EXPECT_EQ(e.document(), "");
    EXPECT_EQ(std::string(e.what()), "nothing is written in yaml; there is: json");
  }
}

TEST(TestNotImplemented, ItNamesTheDocumentTheTechnologyAndWhatThereIs) {
  const ds::emit::NotImplemented failure("diagram", "pdf", {"svg", "text"});
  EXPECT_EQ(failure.document(), "diagram");
  EXPECT_EQ(failure.technology(), "pdf");
  EXPECT_EQ(std::string(failure.what()), "the diagram is not written in pdf; there is: svg text");
}

}  // namespace
