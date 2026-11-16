#include <gtest/gtest.h>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/diff/Diff.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The semantic diff: identities, not lines.
class TestDiff : public ::testing::Test {
 protected:
  ds::model::Model load(const std::string& text) {
    ds::model::Model model;
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText("t.ds", text));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    return model;
  }

  static const ds::diff::Change* find(const ds::diff::DiffResult& d, const std::string& kind, const std::string& idPart) {
    for (const auto& c : d.changes) {
      if (c.kind == kind && c.id.find(idPart) != std::string::npos) return &c;
    }
    return nullptr;
  }

  const std::string before = R"(package p {
  module m {
    class Layout { }
    class Svg {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Text {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cli {
      - owns text: Text
      + run(a: int) -> none
    }
    requirement R-0001-0 {
      title: The system shall draw
    }
  }
})";
};

TEST_F(TestDiff, AMovedOrReorderedDeclarationIsNoChange) {
  const std::string reordered = R"(package p {
  module m {
    class Cli {
      + run(a: int) -> none
      - owns text: Text
    }
    requirement R-0001-0 {
      title: The system shall draw
    }
    class Text {
      + render(doc: string) -> string
      - owns layout: Layout
    }
    class Svg {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Layout { }
  }
})";
  const auto d = ds::diff::Diff::compare(load(before), load(reordered));
  EXPECT_TRUE(d.empty()) << ds::diff::Diff::report(d);
}

TEST_F(TestDiff, AddedRemovedAndChangedByIdentity) {
  const std::string after = R"(package p {
  module m {
    class Layout { }
    class Svg {
      - shares layout: Layout
      + render(doc: string) -> string
    }
    class Text {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cli {
      - owns text: Text
      + run() -> none
      + stop() -> none
    }
    requirement R-0001-0 {
      title: The system shall draw
    }
  }
})";
  const auto d = ds::diff::Diff::compare(load(before), load(after));
  ASSERT_NE(find(d, "changed", "p@m::Svg::layout"), nullptr);
  EXPECT_NE(find(d, "changed", "p@m::Svg::layout")->detail.find("owns -> shares"), std::string::npos);
  ASSERT_NE(find(d, "changed", "p@m::Cli::run"), nullptr);
  EXPECT_NE(find(d, "added", "p@m::Cli::stop"), nullptr);
  EXPECT_NE(find(d, "removed", "owns p@m::Svg::layout -> p@m::Layout"), nullptr);
  EXPECT_NE(find(d, "added", "shares p@m::Svg::layout -> p@m::Layout"), nullptr);
  EXPECT_EQ(find(d, "removed", "p@m::Text"), nullptr);
}

TEST_F(TestDiff, ARenameIsRecognisedByShapeAndByWhoFollows) {
  const std::string after = R"(package p {
  module m {
    class Layout { }
    class Svg {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Terminal {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cells {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cli {
      - owns text: Terminal
      + run(a: int) -> none
    }
    requirement R-0001-0 {
      title: The system shall draw
    }
  }
})";
  const auto d = ds::diff::Diff::compare(load(before), load(after));
  const auto* renamed = find(d, "renamed", "p@m::Terminal");
  ASSERT_NE(renamed, nullptr);
  EXPECT_EQ(renamed->from, "p@m::Text");
  EXPECT_NE(find(d, "added", "p@m::Cells"), nullptr);
  EXPECT_EQ(find(d, "removed", "p@m::Text::layout"), nullptr);      // members travel with the rename
  EXPECT_EQ(find(d, "changed", "p@m::Terminal::render"), nullptr);  // and a move is not a change
}

TEST_F(TestDiff, AStepInsertedInABehaviourIsOneStepAddedAndMovesNoOther) {
  const std::string shape = R"(package p {
  module m {
    class Stock {
      + has(item: string) -> bool
      + reserve(item: string) -> none
      + log(item: string) -> none
    }
    class Cart {
      - stock: Stock
      + add(item: string) -> bool
    }
    sequence Cart::add {
STEPS    }
  }
})";
  const auto with = [&shape](const std::string& steps) { return std::string(shape).replace(shape.find("STEPS"), 5, steps); };
  const std::string steps = "      self -> stock.has(item) -> ok\n      self -> stock.reserve(item)\n      return ok\n";
  const auto d = ds::diff::Diff::compare(load(with(steps)), load(with("      self -> stock.log(item)\n" + steps)));
  int added = 0, removed = 0, changed = 0;
  for (const auto& c : d.changes) {
    if (c.what != "entity") continue;
    added += c.kind == "added";
    removed += c.kind == "removed";
    changed += c.kind == "changed";
  }
  EXPECT_EQ(added, 1);
  EXPECT_EQ(removed, 0);
  EXPECT_EQ(changed, 0);
  EXPECT_NE(find(d, "added", "calls p@m::Cart::add::sequence::m0 -> p@m::Stock::log"), nullptr);
  EXPECT_EQ(find(d, "removed", "calls"), nullptr);  // has and reserve are still called, by the steps they were
}

TEST_F(TestDiff, AnArtefactChangedWithoutANewRevisionIsAFinding) {
  const std::string after = R"(package p {
  module m {
    class Layout { }
    class Svg {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Text {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cli {
      - owns text: Text
      + run(a: int) -> none
    }
    requirement R-0001-0 {
      title: The system shall draw on every surface
    }
  }
})";
  const auto d = ds::diff::Diff::compare(load(before), load(after));
  ASSERT_EQ(d.findings.size(), 1u);
  EXPECT_EQ(d.findings[0].code, "W-REVISION-STALE");
  EXPECT_NE(d.findings[0].message.find("R-0001-0"), std::string::npos);
}

TEST_F(TestDiff, WhatTheCheckDerivesIsNoChangeOfItsOwn) {
  // the attribute's type changes; the message's derived type follows it and is not a second change (DS-114)
  const std::string shop = R"(package p {
  module m {
    class Item {
      + price() -> int
    }
    class Product {
      + price() -> int
      + abstract name() -> string
    }
    class Cart {
      - owns items: list<Item>
      + total() -> int
    }
    sequence Cart::total {
      self -> items.price() -> p
      return p
    }
  }
})";
  std::string after = shop;
  after.replace(after.find("list<Item>"), 10, "list<Product>");
  const auto d = ds::diff::Diff::compare(load(shop), load(after));
  ASSERT_NE(find(d, "changed", "Cart::items"), nullptr);
  EXPECT_EQ(find(d, "changed", "Cart::total::sequence::m"), nullptr);  // its type is derived from the attribute
  EXPECT_EQ(find(d, "changed", "Product"), nullptr);                   // nor is a stereotype the check reads
}

TEST_F(TestDiff, AUseCaseChangedWithoutANewRevisionIsAFinding) {
  // a use case is an artefact like a requirement: its text moves its revision (DS-116)
  const std::string uc = R"(package p {
  usecase UC-00001-1 greet(name: string) -> string {
    title: Greet a person by name
  }
})";
  std::string after = uc;
  after.replace(after.find("by name"), 7, "by their name");
  const auto d = ds::diff::Diff::compare(load(uc), load(after));
  ASSERT_EQ(d.findings.size(), 1u);
  EXPECT_EQ(d.findings[0].code, "W-REVISION-STALE");
  EXPECT_NE(d.findings[0].message.find("UC-00001-1"), std::string::npos);
}

TEST_F(TestDiff, TheMarkedModelCarriesTheChangesForADrawing) {
  const std::string after = R"(package p {
  module m {
    class Layout { }
    class Svg {
      - owns layout: Layout
      + render(doc: string) -> string
    }
    class Cli {
      - owns svg: Svg
      + run(a: int) -> none
    }
    requirement R-0001-0 {
      title: The system shall draw
    }
  }
})";
  const auto a = load(before);
  const auto b = load(after);
  const auto d = ds::diff::Diff::compare(a, b);
  const auto marked = ds::diff::Diff::marked(a, b, d);
  ASSERT_NE(marked.find("p@m::Text"), nullptr);  // the removed class comes over as a ghost
  EXPECT_EQ(marked.find("p@m::Text")->fields.at("change"), "removed");
  EXPECT_EQ(marked.find("p@m::Cli::svg")->fields.at("change"), "added");
  bool removedEdge = false, addedEdge = false;
  for (const auto& r : marked.index().all()) {
    removedEdge |= r.kind == "owns" && r.change == "removed" && r.from == "p@m::Cli::text";
    addedEdge |= r.kind == "owns" && r.change == "added" && r.from == "p@m::Cli::svg";
  }
  EXPECT_TRUE(removedEdge);
  EXPECT_TRUE(addedEdge);
}

}  // namespace
