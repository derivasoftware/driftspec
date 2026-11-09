#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The evaluation of structural rules over the model (REQ-00096-1): Datalog with stratified negation
/// over a finite model, so every rule is decided; a rule is broken by each binding that breaks it.
class TestRuleEvaluation : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  std::vector<std::string> messages(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
};

const char* const kBoxes =
    "package p {\n"
    "  requirement REQ-00001-1: The core shall not depend on render\n"
    "  module render {\n"
    "    class Canvas { }\n"
    "  }\n"
    "  module core {\n"
    "    class Model {\n"
    "      + canvas: p@render::Canvas\n"
    "      + draw() -> none {\n"
    "        @requirement REQ-00001-1\n"
    "      }\n"
    "      + size() -> int\n"
    "    }\n"
    "    class Clean { }\n"
    "    definition depends(a, b) {\n"
    "      if m in a, m owns b\n"
    "    }\n"
    "    definition traced(x) {\n"
    "      if x requires r\n"
    "    }\n";

TEST_F(TestRuleEvaluation, ARuleIsBrokenByEveryBindingThatBreaksIt) {
  load(std::string(kBoxes) +
       "    rule blind {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@core\n"
       "      never depends(c, x), x in p@render\n"
       "    }\n"
       "    rule owned {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@core\n"
       "      holds m in c\n"
       "    }\n"
       "    rule small {\n"
       "      @requirement REQ-00001-1\n"
       "      never c is class, c in p@render, m in c\n"
       "    }\n"
       "  }\n"
       "}\n");
  // never: the binding and the answer that breaks it
  EXPECT_EQ(messages("E-RULE-BROKEN"),
            (std::vector<std::string>{"p@core::blind is broken for c = p@core::Model: x = p@render::Canvas",
                                      "p@core::owned is broken for c = p@core::Clean"}));  // holds: the binding with no answer
}

TEST_F(TestRuleEvaluation, EveryBreachIsKeptWithItsBindingsItsAnswerAndWhereItLands) {
  load(std::string(kBoxes) +
       "    rule blind {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@core\n"
       "      never depends(c, x), x in p@render\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto& all = model.breaches();
  ASSERT_EQ(all.count("p@core::blind"), 1u);
  const auto& b = all.at("p@core::blind");
  ASSERT_EQ(b.size(), 1u);
  EXPECT_EQ(b[0].bindings, (std::vector<std::pair<std::string, std::string>>{{"c", "p@core::Model"}}));
  EXPECT_EQ(b[0].witness, (std::vector<std::pair<std::string, std::string>>{{"x", "p@render::Canvas"}}));
  EXPECT_EQ(b[0].location.line, 7);  // on the class that breaks it
}

TEST_F(TestRuleEvaluation, ANegationTestsWhatIsAlreadyBound) {
  load(std::string(kBoxes) +
       "    rule traced-methods {\n"
       "      @requirement REQ-00001-1\n"
       "      for method m in p@core\n"
       "      never not traced(m)\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_EQ(messages("E-RULE-BROKEN"), (std::vector<std::string>{"p@core::traced-methods is broken for m = p@core::Model::size"}));
}

TEST_F(TestRuleEvaluation, ADefinitionRecursUntilItsFixpoint) {
  const std::string chain =
      "package p {\n"
      "  requirement REQ-00001-1: Ownership shall not loop\n"
      "  module m {\n"
      "    class A {\n"
      "      + b: p@m::B\n"
      "    }\n"
      "    class B {\n"
      "      + c: p@m::C\n"
      "    }\n"
      "    class C {\n"
      "      + %s\n"
      "    }\n"
      "    definition owns-through(a, b) {\n"
      "      if x in a, x owns b\n"
      "      if x in a, x owns c, owns-through(c, b)\n"
      "    }\n"
      "    rule no-loop {\n"
      "      @requirement REQ-00001-1\n"
      "      for class c in p@m\n"
      "      never owns-through(c, c)\n"
      "    }\n"
      "  }\n"
      "}\n";
  char text[1024];
  std::snprintf(text, sizeof text, chain.c_str(), "n: int");
  load(text);
  EXPECT_TRUE(messages("E-RULE-BROKEN").empty());
  model = ds::model::Model();
  std::snprintf(text, sizeof text, chain.c_str(), "a: p@m::A");  // C owns A: the loop closes three steps away
  load(text);
  EXPECT_EQ(messages("E-RULE-BROKEN").size(), 3u);
}

TEST_F(TestRuleEvaluation, WhatARuleNamesMustExistAndItsVariablesBeBound) {
  load(std::string(kBoxes) +
       "    rule wrong {\n"
       "      @requirement REQ-00001-1\n"
       "      for klass c in p@nowhere\n"
       "      holds c frobs y, depends(c), unknown(c), y != z, not traced(w)\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_EQ(messages("E-RULE-UNKNOWN").size(), 4u);  // klass, p@nowhere, frobs, unknown
  EXPECT_EQ(messages("E-RULE-ARITY").size(), 1u);
  EXPECT_EQ(messages("E-RULE-UNSAFE").size(), 2u);  // z and w; y is bound by frobs as written
  EXPECT_TRUE(messages("E-RULE-BROKEN").empty());   // a rule with an error is not evaluated
}

const char* const kLink =
    "package p {\n"
    "  requirement REQ-00001-1: A message shall fit in a frame\n"
    "  module m {\n"
    "    class Link {\n"
    "      + send(msg: int, tag: string) -> bool {\n"
    "        rule in-range {\n"
    "          @requirement REQ-00001-1\n"
    "          for message k\n"
    "          where k calls p@m::Link::send, arg(k, msg, v)\n"
    "          holds v > 0, v < 100\n"
    "        }\n"
    "        rule tagged {\n"
    "          @requirement REQ-00001-1\n"
    "          for message k\n"
    "          where k calls p@m::Link::send, arg(k, tag, t)\n"
    "          never t = \"\"\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    class Client {\n"
    "      - refs link: Link\n"
    "      + run(n: int) -> none\n"
    "    }\n"
    "    sequence Client::run {\n"
    "      -> link.send(50, \"a\")\n"
    "      -> link.send(150, \"b\")\n"
    "      -> link.send(n, \"\")\n"
    "    }\n"
    "  }\n"
    "}\n";

TEST_F(TestRuleEvaluation, ARuleOfAMemberIsKeptOrBrokenByTheLiteralsItsCallsPass) {
  load(kLink);
  std::vector<ds::model::Finding> broken;
  for (const auto& f : model.findings())
    if (f.code == "E-RULE-BROKEN") broken.push_back(f);
  // 50 keeps in-range, 150 breaks it, n is a name: left out, undecided in the design;
  // the empty tag breaks tagged although its message passes a name for msg
  ASSERT_EQ(broken.size(), 2u);
  EXPECT_EQ(broken[0].message, "p@m::Link::send::in-range is broken for k = p@m::Client::run::sequence::m1, v = 150");
  EXPECT_EQ(broken[0].location.line, 26);  // on the call that breaks it, in its sequence
  EXPECT_EQ(broken[1].message, "p@m::Link::send::tagged is broken for k = p@m::Client::run::sequence::m2, t = \"\"");
}

TEST_F(TestRuleEvaluation, ValuesCompareNumbersAsNumbersAndTheRestAsWritten) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: Values compare\n"
      "  module m {\n"
      "    class C { }\n"
      "    rule numbers {\n"
      "      @requirement REQ-00001-1\n"
      "      holds 10 > 9, 2.5 <= 2.50, 100 = 100.0, \"b\" != \"a\", \"x\" = \"x\"\n"
      "    }\n"
      "    rule text-has-no-order {\n"
      "      @requirement REQ-00001-1\n"
      "      holds \"b\" > \"a\"\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(messages("E-RULE-BROKEN"), (std::vector<std::string>{"p@m::text-has-no-order is broken"}));
}

TEST_F(TestRuleEvaluation, ANegationThroughRecursionIsRejected) {
  load(std::string(kBoxes) +
       "    definition odd(a) {\n"
       "      if a is class, not odd(a)\n"
       "    }\n"
       "    rule uses-odd {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c\n"
       "      never odd(c)\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_EQ(messages("E-RULE-UNSTRATIFIED"),
            (std::vector<std::string>{"p@core::odd negates itself through recursion: no order of evaluation decides it"}));
  EXPECT_TRUE(messages("E-RULE-BROKEN").empty());
}

}  // namespace
