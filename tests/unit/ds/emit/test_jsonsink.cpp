#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"

namespace {

/// The JSON sink (DS-397): every value it writes, its two layouts, the escapes, the doubles, the order check and
/// a document kept whole until it is taken, as nlohmann::json 3.11.3's dump() writes them.
class TestJsonSink : public ::testing::Test {
 protected:
  /// One value written alone into a checked compact sink, as text.
  static std::string number(double value) {
    ds::emit::JsonSink sink(false, true);
    sink.number(value);
    return sink.result();
  }
  static std::string text(const std::string& value) {
    ds::emit::JsonSink sink(false, true);
    sink.value(value);
    return sink.result();
  }
  /// A record, a list inside it and a record inside that, in either layout.
  static std::string nested(bool pretty) {
    ds::emit::JsonSink sink(pretty, true);
    sink.beginRecord().field("a").integer(1).field("b").beginList().value("x").beginRecord().field("c").null().endRecord();
    sink.beginList().endList().beginRecord().endRecord().endList().field("d").boolean(false).endRecord();
    return sink.result();
  }
};

TEST_F(TestJsonSink, EveryKindOfValueIsWrittenAsJson) {
  EXPECT_EQ(text("hola"), "\"hola\"");
  EXPECT_EQ(text(""), "\"\"");
  ds::emit::JsonSink sink(false, true);
  sink.beginList().integer(0).integer(-42).integer(std::numeric_limits<std::int64_t>::max());
  sink.integer(std::numeric_limits<std::int64_t>::min()).number(2.5).boolean(true).boolean(false).null().endList();
  EXPECT_EQ(sink.result(), "[0,-42,9223372036854775807,-9223372036854775808,2.5,true,false,null]");
}

TEST_F(TestJsonSink, AnEmptyRecordAndAnEmptyListAreWrittenOnTheirOwnInBothLayouts) {
  for (bool pretty : {false, true}) {
    ds::emit::JsonSink sink(pretty, true);
    sink.beginRecord().endRecord();
    EXPECT_EQ(sink.result(), "{}");
    sink.beginList().endList();
    EXPECT_EQ(sink.result(), "[]");
  }
}

TEST_F(TestJsonSink, ANestedDocumentIsCompactOrIndentedByTwoAsNlohmannWritesIt) {
  EXPECT_EQ(nested(false), R"({"a":1,"b":["x",{"c":null},[],{}],"d":false})");
  EXPECT_EQ(nested(true),
            "{\n"
            "  \"a\": 1,\n"
            "  \"b\": [\n"
            "    \"x\",\n"
            "    {\n"
            "      \"c\": null\n"
            "    },\n"
            "    [],\n"
            "    {}\n"
            "  ],\n"
            "  \"d\": false\n"
            "}");
  const auto tree = nlohmann::json::parse(nested(false));
  EXPECT_EQ(nested(false), tree.dump());
  EXPECT_EQ(nested(true), tree.dump(2));
}

TEST_F(TestJsonSink, ATextIsEscapedAsStreamPySays) {
  // the seven short escapes; every other control character as \u00xx in lower case; DEL, '/' and what is not
  // ASCII written as they are
  EXPECT_EQ(text("a\b\t\n\f\r\"\\/\x7f\x01\x1f \xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"),
            "\"a\\b\\t\\n\\f\\r\\\"\\\\/\x7f\\u0001\\u001f \xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"");
  std::string controls;
  for (char c = 1; c < 0x20; ++c) controls += c;
  EXPECT_EQ(text(controls), nlohmann::json(controls).dump());
  ds::emit::JsonSink sink(false, true);
  sink.beginRecord().field("a\"b\n").value("c").endRecord();  // a field is escaped as a value is
  EXPECT_EQ(sink.result(), R"({"a\"b\n":"c"})");
}

TEST_F(TestJsonSink, ATextThatIsNotUtf8IsRefusedWithNlohmannsMessage) {
  for (const std::string bad : {"\xff", "a\xc3", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "ok \xe2\x82"}) {
    std::string expected;
    try {
      (void)nlohmann::json(bad).dump();
    } catch (const std::exception& e) {
      expected = e.what();
    }
    ASSERT_FALSE(expected.empty());
    try {
      (void)text(bad);
      ADD_FAILURE() << "accepted " << bad;
    } catch (const std::runtime_error& e) {
      EXPECT_EQ(e.what(), expected);
    }
  }
}

TEST_F(TestJsonSink, ADoubleIsWrittenAsTheShortestDigitsThatReadBackInNlohmannsLayout) {
  EXPECT_EQ(number(2.0), "2.0");  // a whole number keeps its point, as ds has always written 2.0
  EXPECT_EQ(number(100.0), "100.0");
  EXPECT_EQ(number(100000000000000.0), "100000000000000.0");
  EXPECT_EQ(number(1e15), "1e+15");  // from 1e15 up, an exponent of two digits at least
  EXPECT_EQ(number(1e21), "1e+21");
  EXPECT_EQ(number(1e100), "1e+100");
  EXPECT_EQ(number(1.7976931348623157e308), "1.7976931348623157e+308");
  EXPECT_EQ(number(12345678901234567890.0), "1.2345678901234567e+19");
  EXPECT_EQ(number(999999999999999.9), "999999999999999.9");
  EXPECT_EQ(number(0.5), "0.5");
  EXPECT_EQ(number(0.1), "0.1");
  EXPECT_EQ(number(0.1 + 0.2), "0.30000000000000004");
  EXPECT_EQ(number(123.456), "123.456");
  EXPECT_EQ(number(-1.5), "-1.5");
  EXPECT_EQ(number(-0.001), "-0.001");
  EXPECT_EQ(number(0.0001), "0.0001");
  EXPECT_EQ(number(1e-5), "1e-05");  // below 1e-4, an exponent
  EXPECT_EQ(number(3e-10), "3e-10");
  EXPECT_EQ(number(-2.5e-7), "-2.5e-07");
  EXPECT_EQ(number(5e-324), "5e-324");
  EXPECT_EQ(number(0.0), "0.0");
  EXPECT_EQ(number(-0.0), "-0.0");          // negative zero keeps its sign
  EXPECT_EQ(number(std::nan("")), "null");  // what is not finite is no number in JSON
  EXPECT_EQ(number(std::numeric_limits<double>::infinity()), "null");
  EXPECT_EQ(number(-std::numeric_limits<double>::infinity()), "null");
}

TEST_F(TestJsonSink, ItsFieldsAreInByteOrderOrItRefusesThem) {
  const auto refused = [](std::initializer_list<const char*> fields) {
    ds::emit::JsonSink sink(false, true);
    sink.beginRecord();
    try {
      for (const char* f : fields) sink.field(f).null();
    } catch (const std::logic_error&) {
      return true;
    }
    return false;
  };
  EXPECT_TRUE(refused({"b", "a"}));
  EXPECT_TRUE(refused({"a", "a"}));  // repeated
  EXPECT_TRUE(refused({"kind", "keptBy"}));
  EXPECT_FALSE(refused({"keptBy", "kind"}));  // byte order, not the alphabet: 'e' comes before 'i'
  EXPECT_FALSE(refused({"Z", "a"}));          // upper case before lower case
  EXPECT_FALSE(refused({"a", "\xc3\xb1"}));   // a byte over 0x7f after every ASCII one
  EXPECT_FALSE(refused({"else", "else_scenario", "first"}));
  ds::emit::JsonSink sink(false, true);  // each record keeps its own order
  sink.beginRecord()
      .field("a")
      .beginRecord()
      .field("z")
      .null()
      .endRecord()
      .field("b")
      .beginRecord()
      .field("a")
      .null()
      .endRecord()
      .endRecord();
  EXPECT_EQ(sink.result(), R"({"a":{"z":null},"b":{"a":null}})");
}

TEST_F(TestJsonSink, UncheckedItWritesTheFieldsInTheOrderGiven) {
  ds::emit::JsonSink sink(false, false);
  sink.beginRecord().field("b").integer(1).field("a").integer(2).endRecord();
  EXPECT_EQ(sink.result(), R"({"b":1,"a":2})");
}

TEST_F(TestJsonSink, CheckedItRefusesADocumentThatIsNoJson) {
  const auto refused = [](void (*write)(ds::emit::ISink&)) {
    ds::emit::JsonSink sink(false, true);
    try {
      write(sink);
      (void)sink.result();
    } catch (const std::logic_error&) {
      return true;
    }
    return false;
  };
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginList().field("a"); }));                // a field in a list
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginRecord().value("a"); }));              // a value without its field
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginRecord().field("a").field("b"); }));   // a field without its value
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginRecord().field("a").endRecord(); }));  // closed without the value
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginRecord().endList(); }));               // a record closed as a list
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.endRecord(); }));                           // nothing open
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.beginList(); }));                           // the result of an open list
  EXPECT_TRUE(refused([](ds::emit::ISink& s) { s.null().null(); }));                         // two documents in one
  EXPECT_TRUE(refused([](ds::emit::ISink&) {}));                                             // nothing written
  EXPECT_FALSE(refused([](ds::emit::ISink& s) { s.beginRecord().field("a").null().endRecord(); }));
}

/// DS_JSON_CHECK says whether a sink its builder does not say of is checked, whatever the build (DS-461): CI's Release
/// tests run with it set to 1, so the contract holds there too; 0 turns the check off. A sink that says is as it says.
TEST_F(TestJsonSink, DsJsonCheckSaysWhetherASinkIsCheckedWhateverTheBuild) {
  const auto refusesNothingWritten = [](std::optional<bool> checked) {
    ds::emit::JsonSink sink = checked ? ds::emit::JsonSink(false, *checked) : ds::emit::JsonSink(false);
    try {
      (void)sink.result();
    } catch (const std::logic_error&) {
      return true;
    }
    return false;
  };
  const char* before = std::getenv("DS_JSON_CHECK");
  const std::string kept = before ? before : "";
  setenv("DS_JSON_CHECK", "1", 1);
  EXPECT_TRUE(refusesNothingWritten(std::nullopt));
  EXPECT_FALSE(refusesNothingWritten(false));  // said by its builder
  setenv("DS_JSON_CHECK", "0", 1);
  EXPECT_FALSE(refusesNothingWritten(std::nullopt));
  EXPECT_TRUE(refusesNothingWritten(true));
  if (before) {
    setenv("DS_JSON_CHECK", kept.c_str(), 1);
  } else {
    unsetenv("DS_JSON_CHECK");
  }
}

TEST_F(TestJsonSink, TheDocumentIsKeptWholeUntilItIsTakenSoAFailureWritesNothing) {
  // as a door uses it: the sink is lent to the writer, and the one who built it writes the result once taken
  std::ostringstream out;
  const auto door = [&out](const std::string& last) {
    ds::emit::JsonSink sink(true, true);
    sink.beginRecord().field("a").beginList();
    for (int i = 0; i < 100000; ++i) sink.value("some text that makes the document long");
    sink.endList().field("b").value(last).endRecord();
    out << sink.result();
  };
  EXPECT_THROW(door("\xff"), std::runtime_error);  // the last text is not UTF-8: refused after megabytes written
  EXPECT_TRUE(out.str().empty());                  // and nothing of the document went out
  door("fine");
  EXPECT_EQ(nlohmann::json::parse(out.str())["b"], "fine");
}

TEST_F(TestJsonSink, TheResultIsHandedOverAndTheSinkWritesTheNextDocument) {
  ds::emit::JsonSink sink(false, true);
  sink.beginRecord().field("a").integer(1).endRecord();
  EXPECT_EQ(sink.result(), R"({"a":1})");
  sink.beginRecord().field("a").integer(2).endRecord();  // the same field again: a new document, a new record
  EXPECT_EQ(sink.result(), R"({"a":2})");
  sink.beginList().value("x").endList();
  EXPECT_EQ(sink.result(), R"(["x"])");
}

TEST_F(TestJsonSink, ThePresentationIsSkipped) {
  // a description says its document in both halves (DS-399); JSON writes the structure and nothing of the rest
  for (bool pretty : {false, true}) {
    ds::emit::JsonSink sink(pretty, true);
    sink.table({"kind", "id"}).row({"class", "a@b::C"}).endTable().paragraph("1 answer");
    sink.beginList().value("x").endList().paragraph("after").table({}).endTable().described("answers");
    EXPECT_EQ(sink.result(), pretty ? "[\n  \"x\"\n]" : R"(["x"])");
  }
}

TEST_F(TestJsonSink, ADocumentItAcceptedNothingOfFailsNotImplementedNamingIt) {
  ds::emit::JsonSink sink(false, true);
  sink.paragraph("only prose").table({"a"}).row({"b"}).endTable();
  try {
    sink.described("essay");
    FAIL() << "a document of prose alone was handed over as an empty JSON";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.document(), "essay");
    EXPECT_EQ(e.technology(), "json");
    EXPECT_EQ(std::string(e.what()), "the essay is not written in json");
  }
  sink.beginRecord().endRecord().described("empty");  // an empty record is written: accepted, so no failure
  EXPECT_EQ(sink.result(), "{}");
}

TEST_F(TestJsonSink, AVerbatimTextIsNotImplemented) {
  // a drawing in svg or text is no JSON (DS-474): the sink says it cannot write it, rather than skip it
  ds::emit::JsonSink sink(false, true);
  try {
    sink.verbatim("<svg/>");
    FAIL() << "a drawing was skipped by the JSON sink";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.technology(), "json");
    EXPECT_EQ(std::string(e.what()), "the verbatim text is not written in json");
  }
  sink.beginList().endList();  // nothing of it was written: the sink writes the next document as before
  EXPECT_EQ(sink.result(), "[]");
}

}  // namespace
