#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/emit/TextSink.hpp"

namespace {

/// The one place that names the sinks (DS-397, DS-398, DS-399): a door is handed the sink of its technology as an
/// ISink, JSON indented or compact or text, and a technology ds has no sink of fails with NotImplemented.
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

TEST(TestSinkFactory, TheTextSinkIsHandedOutAsASink) {
  const auto sink = ds::emit::SinkFactory::text();
  ASSERT_NE(sink, nullptr);
  EXPECT_NE(dynamic_cast<const ds::emit::TextSink*>(sink.get()), nullptr);
  sink->beginRecord().field("a").integer(1).endRecord().table({"x", "y"}).row({"1", "2"}).endTable().paragraph("said");
  EXPECT_EQ(sink->result(), "1  2\nsaid\n");
}

TEST(TestSinkFactory, EveryTechnologyNamedIsTheSinkOfItsName) {
  EXPECT_EQ(ds::emit::SinkFactory::names(), (std::vector<std::string>{"json", "text"}));
  for (const auto& technology : ds::emit::SinkFactory::names()) {
    for (bool pretty : {false, true}) {
      const auto sink = ds::emit::SinkFactory::of(technology, pretty);
      const auto made = technology == "json" ? ds::emit::SinkFactory::json(pretty) : ds::emit::SinkFactory::text();
      for (ds::emit::ISink* s : {sink.get(), made.get()}) s->beginRecord().field("a").integer(1).endRecord().paragraph("p");
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
    EXPECT_EQ(std::string(e.what()), "nothing is written in yaml; there is: json text");
  }
}

TEST(TestNotImplemented, ItNamesTheDocumentTheTechnologyAndWhatThereIs) {
  const ds::emit::NotImplemented failure("diagram", "pdf", {"svg", "text"});
  EXPECT_EQ(failure.document(), "diagram");
  EXPECT_EQ(failure.technology(), "pdf");
  EXPECT_EQ(std::string(failure.what()), "the diagram is not written in pdf; there is: svg text");
}

TEST(TestNotImplemented, WithNothingThereItSaysOnlyWhatIsNotWritten) {
  // a sink handed a document it accepted nothing of knows no technology that writes it (DS-399)
  EXPECT_EQ(std::string(ds::emit::NotImplemented("scene", "text", {}).what()), "the scene is not written in text");
  EXPECT_EQ(std::string(ds::emit::NotImplemented("", "pdf", {}).what()), "nothing is written in pdf");
}

}  // namespace
