#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/parse/TreeReader.hpp"

namespace {

/// A binder that keeps the kinds of the nodes it is given, in order.
class Recorder : public ds::bind::INodeBinder {
 public:
  explicit Recorder(std::vector<std::string> kinds) : kinds_(std::move(kinds)) {}
  std::vector<std::string> kinds() const override { return kinds_; }
  void bind(TSNode node, const ds::bind::BindContext&) const override { seen.push_back(ts_node_type(node)); }
  mutable std::vector<std::string> seen;

 private:
  std::vector<std::string> kinds_;
};

/// Which binder binds a node (DS-355): found by the node's symbol; a node no binder takes is skipped.
class TestDispatch : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Notes notes;
  ds::parse::Parser parser;
  ds::parse::ParsedFile file =
      parser.parseText("a.ds", "package p {\n  requirement REQ-00001-1 {\n    title: The shop shall sell\n  }\n  module m {\n  }\n}\n");
  ds::parse::TreeReader reader{file};
  TSNode package() const { return ts_node_named_child(reader.root(), 0); }
};

TEST_F(TestDispatch, EachNodeGoesToTheBinderOfItsKind) {
  const Recorder artefacts({"requirement"});
  const Recorder boxes({"module"});
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {&artefacts, &boxes});
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
  ctx.children(package());
  EXPECT_EQ(artefacts.seen, std::vector<std::string>{"requirement"});
  EXPECT_EQ(boxes.seen, std::vector<std::string>{"module"});
}

TEST_F(TestDispatch, ANodeNoBinderTakesIsSkipped) {
  const Recorder artefacts({"requirement"});
  const ds::bind::Dispatch dispatch(ds::parse::Parser::language(), {&artefacts});
  const ds::bind::BindContext ctx{model, notes, reader, metamodel, dispatch, "", ""};
  ctx.children(package());  // the package's name and its module go to no binder
  EXPECT_EQ(artefacts.seen, std::vector<std::string>{"requirement"});
  EXPECT_EQ(dispatch.of(package()), nullptr);
  EXPECT_EQ(dispatch.of(ts_node_named_child(package(), 1)), &artefacts);
}

TEST_F(TestDispatch, AKindTheGrammarLacksOrTwoBindersTakeIsRefused) {
  const Recorder unknown({"no_such_node"});
  EXPECT_THROW(ds::bind::Dispatch(ds::parse::Parser::language(), {&unknown}), std::logic_error);
  const Recorder first({"requirement"});
  const Recorder second({"requirement"});
  EXPECT_THROW(ds::bind::Dispatch(ds::parse::Parser::language(), {&first, &second}), std::logic_error);
}

}  // namespace
