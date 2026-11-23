#include <gtest/gtest.h>

#include "ds/bind/BindContext.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// What binding one node is given (DS-354): the model and the notes it writes, the file's reader, the place it is in.
TEST(TestBindContext, AnInnerPlaceKeepsTheModelTheNotesAndTheReader) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::parse::ParsedFile file = parser.parseText("a.ds", "package p {\n}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::BindContext top{model, notes, reader, "", ""};
  const ds::bind::BindContext inner = top.within("p@", "p");
  EXPECT_EQ(inner.scope, "p@");
  EXPECT_EQ(inner.owner, "p");
  EXPECT_EQ(&inner.model, &model);
  EXPECT_EQ(&inner.notes, &notes);
  EXPECT_EQ(&inner.reader, &reader);
}

TEST(TestBindContext, ANodeIsLocatedAsTheModelKeepsIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::parse::ParsedFile file = parser.parseText("a.ds", "package p {\n}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::BindContext ctx{model, notes, reader, "", ""};
  const ds::model::Location at = ctx.locationOf(reader.root());
  const ds::parse::Span span = reader.spanOf(reader.root());
  EXPECT_EQ(at.file, "a.ds");
  EXPECT_EQ(at.line, span.line);
  EXPECT_EQ(at.column, span.column);
  EXPECT_EQ(at.endLine, span.endLine);
  EXPECT_EQ(at.endColumn, span.endColumn);
}

}  // namespace
