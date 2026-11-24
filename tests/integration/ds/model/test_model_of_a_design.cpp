#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

using ds::model::Model;
using ds::model::Relation;

const char* kShop =
    "package shop {\n"
    "  module store {\n"
    "    class Item { + price: int }\n"
    "    class Stock : Item {\n"
    "      - refs first: Item\n"
    "      + take(n: int) -> Item\n"
    "    }\n"
    "  }\n"
    "  module till {\n"
    "    class Till {\n"
    "      - stock: store::Stock\n"
    "      + sell(n: int) -> int\n"
    "    }\n"
    "    sequence Till::sell {\n"
    "      self -> stock.take(n) -> item\n"
    "      return n\n"
    "    }\n"
    "    sequence store::Stock::take {\n"
    "      return first\n"
    "    }\n"
    "  }\n"
    "  requirement REQ-00001-2: The system shall sell\n"
    "}\n";

const char* kCrate = "package shop {\n  module store {\n    class Crate {\n      - stock: Stock\n    }\n  }\n}\n";

/// The model as the files of a design leave it: read, bound and checked by the real steps, never
/// built by hand (REQ-00001-1, REQ-00002-1).
class TestModelOfADesign : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder{model, metamodel};

  void bind(const std::string& path, const std::string& text) {
    const auto file = parser.parseText(path, text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
  }

  void check() {
    const auto steps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, steps).run(binder.notes());
  }

  int findings(const std::string& code) const {
    int n = 0;
    for (const auto& f : model.findings()) n += f.code == code;
    return n;
  }

  static std::vector<std::string> ends(const std::vector<Relation>& relations, bool to) {
    std::vector<std::string> out;
    for (const auto& r : relations) out.push_back(r.kind + " " + (to ? r.to : r.from));
    return out;
  }
};

TEST_F(TestModelOfADesign, WhatIsNamedBeforeItIsDeclaredIsOneEntityAndNotExpected) {
  bind("a.ds", "package p {\n  module m {\n    class B {\n      - a: A\n      - lost: Missing\n    }\n  }\n}\n");
  bind("b.ds", "package p {\n  module m {\n    class A { }\n  }\n}\n");
  check();
  ASSERT_NE(model.find("p@m::A"), nullptr);
  EXPECT_FALSE(model.find("p@m::A")->expected);  // the declaration stands, whatever file named it first
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 0);
  EXPECT_EQ(model.find("p@m::Missing"), nullptr);  // named in a type, declared nowhere: text, no entity (DS-18)
  EXPECT_EQ(findings("W-REF-UNDECLARED"), 0);
}

TEST_F(TestModelOfADesign, ASecondDeclarationIsReportedWithWhereTheFirstIs) {
  bind("a.ds", "package p {\n  module m {\n    class A { }\n  }\n}\n");
  bind("b.ds", "package p {\n  module m {\n    class A { }\n    class B { }\n  }\n}\n");  // the module reopens, the class does not
  check();
  ASSERT_EQ(findings("E-DUPLICATE-IDENTITY"), 1);
  for (const auto& f : model.findings()) {
    if (f.code == "E-DUPLICATE-IDENTITY") {
      EXPECT_NE(f.message.find("first at a.ds:3"), std::string::npos) << f.message;
    }
  }
  EXPECT_NE(model.find("p@m::B"), nullptr);
}

TEST_F(TestModelOfADesign, AnEntityKeepsTheFlagsItWasDeclaredWith) {
  bind("shop.ds", kShop);
  check();
  EXPECT_TRUE(model.find("shop@store::Stock::first")->has("refs"));
  EXPECT_FALSE(model.find("shop@store::Stock::first")->has("owns"));
  EXPECT_TRUE(model.find("shop@till::Till::stock")->has("owns"));
}

TEST_F(TestModelOfADesign, ABehaviourWrittenInAnotherModuleIsRenamedOntoItsSubjectWithItsStepsAndRelations) {
  bind("shop.ds", kShop);
  check();
  const std::string seq = "shop@store::Stock::take::sequence";
  EXPECT_EQ(model.find("shop@till::store::Stock::take::sequence"), nullptr);  // the identity it was written with
  ASSERT_NE(model.find(seq), nullptr);
  ASSERT_NE(model.find(seq + "::m0"), nullptr);
  EXPECT_EQ(model.find(seq + "::m0")->scope, seq);
  EXPECT_EQ(ends(model.index().from(seq), true),
            (std::vector<std::string>{"contains " + seq + "::m0", "behaviour-of shop@store::Stock::take"}));
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 0);
}

TEST_F(TestModelOfADesign, ARenameOntoADeclaredIdentityIsRefused) {
  std::string twice = kShop;
  const std::string store = "  module till {";
  twice.insert(twice.find(store), "  module store {\n    sequence Stock::take {\n      return first\n    }\n  }\n");
  bind("shop.ds", twice);
  check();
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 1);
  EXPECT_NE(model.find("shop@store::Stock::take::sequence"), nullptr);        // the one written beside its class
  EXPECT_NE(model.find("shop@till::store::Stock::take::sequence"), nullptr);  // the other stays where it was written
}

/// The questions the model answers by identity, asked of a design the real steps bound (REQ-00001-1,
/// CST-00008-2): the order stated, every change followed, answers that are copies.
class TestIndexOfADesign : public TestModelOfADesign {
 protected:
  void SetUp() override {
    bind("shop.ds", kShop);
    check();
  }
};

TEST_F(TestIndexOfADesign, WhatLeavesAndWhatReachesAnEntityComeInTheOrderStated) {
  EXPECT_EQ(
      ends(model.index().from("shop@store::Stock"), true),
      (std::vector<std::string>{"contains shop@store::Stock::first", "contains shop@store::Stock::take", "inherits shop@store::Item"}));
  EXPECT_EQ(ends(model.index().to("shop@store::Item"), false),
            (std::vector<std::string>{"contains shop@store", "inherits shop@store::Stock", "refs shop@store::Stock::first",
                                      "uses shop@store::Stock::take"}));
  EXPECT_TRUE(model.index().from("shop@store::Nobody").empty());
}

TEST_F(TestIndexOfADesign, EveryRelationOfAKindAndOfSeveralKindsComesInTheOrderStated) {
  EXPECT_EQ(ends(model.index().ofKind("behaviour-of"), true),
            (std::vector<std::string>{"behaviour-of shop@till::Till::sell", "behaviour-of shop@store::Stock::take"}));
  EXPECT_EQ(ends(model.index().ofKinds({"owns", "refs", "inherits"}), true),  // the order asked does not matter
            (std::vector<std::string>{"inherits shop@store::Item", "refs shop@store::Item", "owns shop@store::Stock"}));
  EXPECT_TRUE(model.index().ofKind("verifies").empty());
  EXPECT_EQ(model.index().all().size(), model.index().count());
}

TEST_F(TestIndexOfADesign, WhatTouchesAnEntityComesOnce) {
  const auto touching = model.index().touching("shop@store::Stock");
  std::set<std::string> seen;
  for (const auto& r : touching) EXPECT_TRUE(seen.insert(r.kind + " " + r.from + " " + r.to).second);
  EXPECT_EQ(touching.size(), model.index().from("shop@store::Stock").size() + model.index().to("shop@store::Stock").size());
  EXPECT_TRUE(model.index().touching("shop@store::Nobody").empty());
}

TEST_F(TestIndexOfADesign, WhatHoldsWhatIsATreeAndAScopeNamesWhatItDeclares) {
  EXPECT_EQ(model.index().parent("shop@store"), "shop");
  EXPECT_EQ(model.index().parent("shop@store::Stock::take"), "shop@store::Stock");
  EXPECT_EQ(model.index().parent("shop"), "");
  EXPECT_EQ(model.index().children("shop@store"), (std::vector<std::string>{"shop@store::Item", "shop@store::Stock"}));
  EXPECT_EQ(model.index().members("shop@store::Stock"), (std::vector<std::string>{"shop@store::Stock::first", "shop@store::Stock::take"}));
  EXPECT_TRUE(model.index().members("shop@none").empty());
}

TEST_F(TestIndexOfADesign, AnArtefactIsFoundByAnyRevisionOrNone) {
  EXPECT_EQ(model.index().revisionOf("REQ-00001-1"), "REQ-00001-2");
  EXPECT_EQ(model.index().revisionOf("REQ-00001"), "REQ-00001-2");
  EXPECT_EQ(model.index().revisionOf("REQ-00009-1"), "");
}

TEST_F(TestIndexOfADesign, WhatAFileBoundLaterDeclaresIsAnsweredAtOnce) {
  ASSERT_EQ(model.index().children("shop@store").size(), 2u);
  bind("crate.ds", kCrate);  // no check in between: the index follows the model, not a pass over it
  EXPECT_EQ(model.index().children("shop@store").back(), "shop@store::Crate");
  EXPECT_EQ(ends(model.index().from("shop@store::Crate"), true), (std::vector<std::string>{"contains shop@store::Crate::stock"}));
  EXPECT_EQ(model.index().parent("shop@store::Crate::stock"), "shop@store::Crate");
}

TEST_F(TestIndexOfADesign, ARenameMovesEverythingIndexedByTheOldIdentity) {
  const std::string was = "shop@till::store::Stock::take::sequence";
  const std::string now = "shop@store::Stock::take::sequence";
  EXPECT_TRUE(model.index().from(was).empty());
  EXPECT_TRUE(model.index().members(was).empty());
  EXPECT_EQ(model.index().members(now), std::vector<std::string>{now + "::m0"});
  EXPECT_EQ(model.index().parent(now + "::m0"), now);
  EXPECT_EQ(
      ends(model.index().to(now), false),
      (std::vector<std::string>{"contains shop@till", "expands-to shop@till::Till::sell::sequence::m0"}));  // what pointed at it follows
}

TEST_F(TestIndexOfADesign, AnAnswerIsACopyThatALaterChangeLeavesAsItWas) {
  const std::vector<Relation> held = model.index().all();
  const std::vector<std::string> children = model.index().children("shop@store");
  bind("crate.ds", kCrate);
  EXPECT_LT(held.size(), model.index().count());
  EXPECT_EQ(held.front().kind, "contains");
  EXPECT_EQ(held.front().to, "shop@store");
  EXPECT_EQ(children.size(), 2u);
}

TEST_F(TestIndexOfADesign, ACopyOfTheModelAnswersForItself) {
  Model copy = model;
  ds::bind::Binder other(copy, metamodel);
  other.bind(parser.parseText("crate.ds", kCrate));
  EXPECT_EQ(copy.index().children("shop@store").size(), 3u);
  EXPECT_EQ(model.index().children("shop@store").size(), 2u);
  EXPECT_EQ(model.find("shop@store::Crate"), nullptr);
}

}  // namespace
