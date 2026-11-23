#include <gtest/gtest.h>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
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
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {});
  const ds::bind::BindContext top{model, notes, reader, metamodel, dispatch, "", ""};
  const ds::bind::BindContext inner = top.within("p@", "p");
  EXPECT_EQ(inner.scope, "p@");
  EXPECT_EQ(inner.owner, "p");
  EXPECT_EQ(&inner.model, &model);
  EXPECT_EQ(&inner.notes, &notes);
  EXPECT_EQ(&inner.reader, &reader);
  EXPECT_EQ(&inner.metamodel, &metamodel);
  EXPECT_EQ(&inner.dispatch, &dispatch);
}

TEST(TestBindContext, ANodeIsLocatedAsTheModelKeepsIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::parse::ParsedFile file = parser.parseText("a.ds", "package p {\n}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {});
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
  const ds::model::Location at = ctx.locationOf(reader.root());
  const ds::parse::Span span = reader.spanOf(reader.root());
  EXPECT_EQ(at.file, "a.ds");
  EXPECT_EQ(at.line, span.line);
  EXPECT_EQ(at.column, span.column);
  EXPECT_EQ(at.endLine, span.endLine);
  EXPECT_EQ(at.endColumn, span.endColumn);
}

TEST(TestBindContext, ItReadsTheFieldsAndTheAnnotationsOfANode) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::parse::ParsedFile file = parser.parseText("a.ds",
                                                      "package p {\n"
                                                      "  module m {\n"
                                                      "    include: a\n"
                                                      "    include: b\n"
                                                      "    @requirement REQ-00001-1\n"
                                                      "    @wishes REQ-00002-1\n"
                                                      "  }\n"
                                                      "}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {});
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "p@", "p"};
  const TSNode module = ts_node_named_child(ts_node_named_child(reader.root(), 0), 1);
  ds::model::Entity entity;
  ctx.fields(module, entity);
  EXPECT_EQ(entity.fields.at("include"), "a, b");  // a repeated key accumulates
  ctx.annotations(module, "p@m", "p@m");
  ASSERT_EQ(notes.references.size(), 2U);
  EXPECT_EQ(notes.references[0].target, "REQ-00001-1");
  EXPECT_EQ(notes.references[0].from, "p@m");
  bool unknown = false;  // a word the language does not know is kept as a link, and said
  for (const auto& f : model.findings()) unknown |= f.code == "W-ANNOTATION-UNKNOWN";
  EXPECT_TRUE(unknown);
  EXPECT_FALSE(ds::bind::BindContext::annotationsOnly(module));  // it writes fields too
}

TEST(TestBindContext, AnExtensionExpectsWhatNothingDefinedYet) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  const ds::parse::ParsedFile file =
      parser.parseText("a.ds", "package p {\n  module m {\n    class A {\n      @requirement REQ-00001-1\n    }\n  }\n}\n");
  const ds::parse::TreeReader reader(file);
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {});
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "p@", "p"};
  const TSNode module = ts_node_named_child(ts_node_named_child(reader.root(), 0), 1);
  const TSNode cls = ts_node_named_child(module, 1);
  const TSNode body = ds::parse::TreeReader::child(cls, ds::parse::Field::body);
  EXPECT_TRUE(ds::bind::BindContext::annotationsOnly(body));
  ctx.extend(body, "p@m::A", ds::model::Kind::Class, "A", "p@m", ctx.locationOf(cls));
  const ds::model::Entity* expected = model.find("p@m::A");
  ASSERT_NE(expected, nullptr);
  EXPECT_TRUE(expected->expected);
  ASSERT_EQ(notes.references.size(), 1U);
  EXPECT_EQ(notes.references[0].from, "p@m::A");
}

}  // namespace
