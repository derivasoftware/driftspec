#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/TextSink.hpp"

/// The text sink (DS-399): the presentation half as ds prints it, a paragraph as its line and a table as its rows of
/// cells joined by two spaces, unaligned and under no header; the structure skipped; and a description it accepted
/// nothing of said as NotImplemented, naming the document.
class TestTextSink : public ::testing::Test {
 protected:
  ds::emit::TextSink sink;
};

TEST_F(TestTextSink, AParagraphIsItsTextAndALineBreak) {
  sink.paragraph("4 entities · 0 expected").paragraph("");
  EXPECT_EQ(sink.result(), "4 entities · 0 expected\n\n");
}

TEST_F(TestTextSink, ATableIsItsRowsOfCellsJoinedByTwoSpacesUnderNoHeader) {
  sink.table({"kind", "id", "detail"}).row({"class", "a@b::C", "x"}).row({"package", "a"}).row({"", "", ""}).endTable();
  EXPECT_EQ(sink.result(), "class  a@b::C  x\npackage  a\n    \n");  // a row ends at its last cell; an empty cell stays
}

TEST_F(TestTextSink, AnEmptyTableIsAcceptedAndWritesNothing) {
  sink.table({"kind", "id", "detail"}).endTable().described("answers");  // no answers: nothing printed, and no failure
  EXPECT_EQ(sink.result(), "");
}

TEST_F(TestTextSink, TheStructureIsSkipped) {
  sink.beginRecord().field("entities").integer(4).field("findings").beginList().value("x").number(2.5).boolean(true).null();
  sink.endList().endRecord().paragraph("said");
  EXPECT_EQ(sink.result(), "said\n");
}

TEST_F(TestTextSink, ADocumentItAcceptedNothingOfFailsNotImplementedNamingIt) {
  sink.beginRecord().field("nodes").beginList().endList().endRecord();
  try {
    sink.described("scene");
    FAIL() << "a scene was handed over as an empty page";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.document(), "scene");
    EXPECT_EQ(e.technology(), "text");
    EXPECT_EQ(std::string(e.what()), "the scene is not written in text");
  }
}

TEST_F(TestTextSink, TheResultIsHandedOverAndTheNextDocumentStartsWithNothingAccepted) {
  sink.paragraph("one").described("report");
  EXPECT_EQ(sink.result(), "one\n");
  EXPECT_EQ(sink.result(), "");
  EXPECT_THROW(sink.described("report"), ds::emit::NotImplemented);  // what was accepted went with the result
}

TEST_F(TestTextSink, AVerbatimTextIsWrittenAsItIsOncePerDocument) {
  // a drawing already written for a terminal or a page (DS-474): as it is, no line break added, and accepted
  sink.verbatim("<svg/>").described("svg");
  EXPECT_EQ(sink.result(), "<svg/>");
  sink.verbatim("┌─┐\n└─┘\n");
  EXPECT_THROW(sink.verbatim("again"), std::logic_error);  // a text written whole is its document: a second is refused
  EXPECT_EQ(sink.result(), "┌─┐\n└─┘\n");
  sink.verbatim("").described("text");  // an empty drawing is still a document handed over, so no failure
  EXPECT_EQ(sink.result(), "");
  sink.verbatim("next");  // the result started the next document, which takes one
  EXPECT_EQ(sink.result(), "next");
}
