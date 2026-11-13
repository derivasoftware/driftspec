#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/diff/Diff.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

ds::model::Model load(const std::string& text) {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  binder.bind(parser.parseText("a.ds", text));
  const auto steps = ds::check::steps();
  ds::check::Checker(model, steps).run(binder);
  return model;
}

const char* const kBefore = "package p {\n  module m {\n    class Till {\n      + sell(n: int) -> int\n    }\n    class Old { }\n  }\n}\n";
const char* const kAfter =
    "package p {\n  module m {\n    class Till {\n      + sell(n: int, cash: int) -> int\n    }\n    class New { }\n  }\n}\n";

/// Two versions in the model's terms (DS-186): what was added, removed and changed, by identity.

TEST(TestDiff, WhatWasAddedRemovedAndChangedByIdentity) {
  const auto result = ds::diff::Diff::compare(load(kBefore), load(kAfter));
  bool added = false, removed = false, changed = false;
  for (const auto& c : result.changes) {
    added |= c.kind == "added" && c.id == "p@m::New";
    removed |= c.kind == "removed" && c.id == "p@m::Old";
    changed |= c.kind == "changed" && c.id == "p@m::Till::sell";
  }
  EXPECT_TRUE(added && removed && changed);
  EXPECT_TRUE(ds::diff::Diff::compare(load(kBefore), load(kBefore)).empty());  // nothing changed, nothing said
}

TEST(TestDiff, TheMarkedModelCarriesWhatChangedAndTheReportGroupsIt) {
  const auto a = load(kBefore);
  const auto b = load(kAfter);
  const auto result = ds::diff::Diff::compare(a, b);
  const auto marked = ds::diff::Diff::marked(a, b, result);
  ASSERT_NE(marked.find("p@m::Old"), nullptr);  // a removed entity is brought over to be drawn
  EXPECT_EQ(marked.find("p@m::Old")->field("change"), "removed");
  EXPECT_EQ(marked.find("p@m::New")->field("change"), "added");
  EXPECT_NE(ds::diff::Diff::report(result).find("p@m::New"), std::string::npos);
}

TEST(TestDiffResult, ItCountsEachKindOfChange) {
  ds::diff::DiffResult r;
  r.changes = {{"added", "entity", "a"}, {"added", "entity", "b"}, {"removed", "entity", "c"}, {"changed", "entity", "d"}};
  EXPECT_EQ(r.added(), 2);
  EXPECT_EQ(r.removed(), 1);
  EXPECT_EQ(r.changed(), 1);
  EXPECT_FALSE(r.empty());
}

}  // namespace
