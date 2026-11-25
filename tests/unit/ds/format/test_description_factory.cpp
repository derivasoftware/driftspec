#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Document.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The document axis (DS-398): DescriptionFactory gives each document's description, which describes it into any
/// sink it is lent, in both halves, and knows no technology (DS-399); each sink writes the half it can, a sink that
/// accepted nothing of a document fails NotImplemented naming it, and the description lets it through.
class TestDescriptionFactory : public ::testing::Test {
 protected:
  /// A technology made here, that no description knows: each call of either half written as one word.
  class Words : public ds::emit::ISink {
   public:
    ISink& beginRecord() override { return word("{"); }
    ISink& endRecord() override { return word("}"); }
    ISink& field(std::string_view name) override { return word(std::string(name) + ":"); }
    ISink& beginList() override { return word("["); }
    ISink& endList() override { return word("]"); }
    ISink& value(std::string_view text) override { return word("'" + std::string(text) + "'"); }
    ISink& integer(std::int64_t value) override { return word(std::to_string(value)); }
    ISink& number(double value) override { return word(std::to_string(value)); }
    ISink& boolean(bool value) override { return word(value ? "yes" : "no"); }
    ISink& null() override { return word("-"); }
    ISink& paragraph(std::string_view text) override { return word("\"" + std::string(text) + "\""); }
    ISink& table(const std::vector<std::string_view>& columns) override { return word("table" + joined(columns)); }
    ISink& row(const std::vector<std::string_view>& cells) override { return word("row" + joined(cells)); }
    ISink& endTable() override { return word("/table"); }
    ISink& described(std::string_view) override { return *this; }
    std::string result() override {
      std::string out;
      out.swap(out_);
      return out;
    }

   private:
    ISink& word(const std::string& w) {
      out_ += (out_.empty() ? "" : " ") + w;
      return *this;
    }
    static std::string joined(const std::vector<std::string_view>& texts) {
      std::string out = "(";
      for (const auto& t : texts) out += (out.size() > 1 ? "|" : "") + std::string(t);
      return out + ")";
    }
    std::string out_;
  };

  /// A technology that writes nothing at all: every call fails NotImplemented.
  class Nothing : public Words {
   public:
    ISink& beginRecord() override { throw ds::emit::NotImplemented("", "nothing", {}); }
    ISink& beginList() override { throw ds::emit::NotImplemented("", "nothing", {}); }
  };

  const ds::theme::Theme theme;
  const ds::render::IrDocument diagram =
      ds::render::IrDocument::parse(R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                                    R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                                    R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency"}]})");
  const std::vector<ds::query::Answer> answers{{"class", "p@m::A", "a class"}};
  ds::check::Report report;
  ds::scene::SceneDocument scene;

  void SetUp() override {
    report.severities = ds::model::Metamodel().severities();
    report.entities = 2;
    ds::scene::Node n;
    n.id = "p";
    n.kind = "package";
    scene.nodes.push_back(n);
  }

  /// Every description the factory makes, with the name of its document.
  std::vector<std::pair<std::string, std::unique_ptr<ds::format::ADescription>>> every() const {
    std::vector<std::pair<std::string, std::unique_ptr<ds::format::ADescription>>> out;
    out.emplace_back("scene", ds::format::DescriptionFactory::scene(scene));
    out.emplace_back("report", ds::format::DescriptionFactory::report(report));
    out.emplace_back("answers", ds::format::DescriptionFactory::answers(answers));
    out.emplace_back("layout", ds::format::DescriptionFactory::layout(diagram, theme, 0));
    out.emplace_back("cells", ds::format::DescriptionFactory::cells(diagram, theme, 0));
    return out;
  }
};

TEST_F(TestDescriptionFactory, EachDocumentIsDescribedOnceIntoAJsonSinkTheSameEveryTime) {
  for (const auto& [document, description] : every()) {
    ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
    description->describe(sink);
    const std::string first = sink.result();
    nlohmann::json parsed;
    EXPECT_NO_THROW(parsed = nlohmann::json::parse(first)) << document;
    description->describe(sink);
    EXPECT_EQ(sink.result(), first) << document;
  }
}

TEST_F(TestDescriptionFactory, ADescriptionWritesIntoASinkOfATechnologyItDoesNotKnow) {
  Words words;
  ds::format::DescriptionFactory::answers(answers)->describe(words);
  EXPECT_EQ(words.result(), "[ { detail: 'a class' id: 'p@m::A' kind: 'class' } ] table(kind|id|detail) row(class|p@m::A|a class) /table");
  ds::format::DescriptionFactory::report(report)->describe(words);
  EXPECT_EQ(words.result(),
            "{ coverage: [ ] entities: 2 errors: 0 expected: 0 findings: [ ] relations: 0 } table(location|code|message) /table "
            "\"2 entities · 0 expected · 0 relations · 0 findings\"");
}

TEST_F(TestDescriptionFactory, EachDocumentIsNamedAndWrittenByTheSinkOfEachHalfOrFailsNamingIt) {
  // the JSON sink writes the structure of every document; the text sink the presentation the report and the answers
  // say, and of the others, which say none, it accepts nothing: NotImplemented, naming the document (DS-399)
  for (const auto& [document, description] : every()) {
    EXPECT_EQ(description->name(), document);
    const auto json = ds::emit::SinkFactory::of("json", false);
    EXPECT_NO_THROW(description->describe(*json)) << document;
    EXPECT_FALSE(json->result().empty()) << document;
    const auto text = ds::emit::SinkFactory::of("text", false);
    if (document == "report" || document == "answers") {
      EXPECT_NO_THROW(description->describe(*text)) << document;
      EXPECT_FALSE(text->result().empty()) << document;
      continue;
    }
    try {
      description->describe(*text);
      FAIL() << "the " << document << " was written as text it says nothing of";
    } catch (const ds::emit::NotImplemented& e) {
      EXPECT_EQ(e.document(), document);
      EXPECT_EQ(e.technology(), "text");
      EXPECT_EQ(std::string(e.what()), "the " + document + " is not written in text");
    }
  }
}

TEST_F(TestDescriptionFactory, ASinkThatCannotWriteWhatItIsGivenFailsNotImplementedThroughTheDescription) {
  for (const auto& [document, description] : every()) {
    Nothing nothing;
    EXPECT_THROW(description->describe(nothing), ds::emit::NotImplemented) << document;
  }
}

}  // namespace
