#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"

namespace {

/// A diagram opens the boxes it names and closes the rest, over tests/corpus/visibility.
class TestOpenBoxes : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/visibility")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + name, false));
  }

  static const nlohmann::json* node(const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"]) {
      if (n["id"] == id) return &n;
    }
    return nullptr;
  }

  static bool edge(const nlohmann::json& ir, const std::string& from, const std::string& to) {
    for (const auto& e : ir["edges"]) {
      if (e["from"] == from && e["to"] == to) return true;
    }
    return false;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestOpenBoxes, OpenAloneClosesEveryBox) {
  const auto ir = diagram("HLD_Shop");
  ASSERT_EQ(ir["nodes"].size(), 2U);  // store and front, and nothing of what is inside them
  EXPECT_EQ((*node(ir, "shop@store"))["kind"], "box");
  EXPECT_EQ((*node(ir, "shop@front"))["kind"], "box");
}

TEST_F(TestOpenBoxes, AClosedBoxShowsOnlyItsPublicChildren) {
  const auto ir = diagram("HLD_Shop");
  const std::string shown = (*node(ir, "shop@store"))["sections"].dump();
  EXPECT_NE(shown.find("stock"), std::string::npos);
  EXPECT_EQ(shown.find("internal"), std::string::npos);  // a private module
}

TEST_F(TestOpenBoxes, EdgesLiftToTheBoxesThatAreSeenAndOnlyOnceForEachBond) {
  const auto ir = diagram("HLD_Shop");
  EXPECT_TRUE(edge(ir, "shop@front", "shop@store"));
  ASSERT_EQ(ir["edges"].size(), 2U);  // what stays inside a closed box is not drawn, and the rest is said twice at most
  std::set<std::string> kinds;
  for (const auto& e : ir["edges"]) kinds.insert(e["kind"].get<std::string>());
  EXPECT_EQ(kinds, (std::set<std::string>{"association", "dependency"}));  // a structural bond and a use (DS-230)
}

TEST_F(TestOpenBoxes, OpeningABoxShowsTheWayToItAndClosesTheRest) {
  const auto ir = diagram("HLD_Store");
  EXPECT_EQ((*node(ir, "shop@store"))["kind"], "container");
  EXPECT_EQ((*node(ir, "shop@store@stock"))["kind"], "container");
  EXPECT_NE(node(ir, "shop@store@stock::Inventory"), nullptr);
  EXPECT_EQ((*node(ir, "shop@store@internal"))["kind"], "box");  // its sibling stays closed
  EXPECT_EQ((*node(ir, "shop@front"))["kind"], "box");
  EXPECT_EQ(node(ir, "shop@front@till::Till"), nullptr);
}

TEST_F(TestOpenBoxes, AnEdgeReachesInsideAnOpenBoxFromAClosedOne) {
  const auto ir = diagram("HLD_Store");
  EXPECT_TRUE(edge(ir, "shop@front", "shop@store@stock::Inventory"));
  EXPECT_TRUE(edge(ir, "shop@store@stock::Inventory", "shop@store@internal"));
}

TEST_F(TestOpenBoxes, ADiagramThatDoesNotSayOpenDrawsEverythingOpen) {
  ds::model::Model plain{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(plain, metamodel);
  for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(plain, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(plain, metamodel).render("diagram:CL_Model", false));
  EXPECT_EQ(ir["nodes"][0]["kind"], "container");
}

TEST(TestOpenBoxesBehaviour, ABehaviourThatCrossesBoxesIsAnEdgeOfTheClosedDrawing) {
  const ds::model::Metamodel metamodel;
  ds::model::Model shop{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(shop, metamodel);
  for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/interfaces")) binder.bind(f);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(shop, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(shop, metamodel).render("diagram:HLD_Shop", false));
  bool reached = false;
  for (const auto& e : ir["edges"]) reached = reached || (e["from"] == "shop@shell" && e["to"] == "shop@till");
  EXPECT_TRUE(reached);  // no type says it: only the sequence of shell::buy calls till::main
}

TEST_F(TestOpenBoxes, DepthStarIsEveryLevelAndNeverAnAbort) {
  const auto ir = diagram("SEQ_All");
  EXPECT_FALSE(ir["edges"].empty());
}

TEST_F(TestOpenBoxes, ADepthThatIsNoNumberIsAFindingAndStillDraws) {
  bool found = false;
  for (const auto& f : model.findings()) found = found || (f.code == "W-DIAGRAM-DEPTH" && f.message.find("lots") != std::string::npos);
  EXPECT_TRUE(found);
  EXPECT_FALSE(diagram("HLD_Bad")["edges"].empty());
}

TEST_F(TestOpenBoxes, APatternThatNamesWhatNothingDeclaresIsAFinding) {
  int found = 0;
  for (const auto& f : model.findings())
    found += f.code == "W-DIAGRAM-UNDECLARED" && f.message.find("shop@store@shelf::*") != std::string::npos;
  EXPECT_EQ(found, 1);
}

TEST(TestDiagramPatterns, ARequirementDiagramReadsAModulePatternAsAClassDiagramDoes) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  module store {\n"
                               "    requirement REQ-00001-1: The store shall keep its items\n"
                               "    class Cart {\n"
                               "      @requirement REQ-00001-1\n"
                               "    }\n"
                               "  }\n"
                               "  module empty { }\n"
                               "}\n"
                               "diagram REQ_Store {\n"
                               "  kind: requirement\n"
                               "  include: shop@store::*\n"
                               "}\n"
                               "diagram REQ_Empty {\n"
                               "  kind: requirement\n"
                               "  include: shop@empty::*\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:REQ_Store", false));
  bool requirement = false;
  for (const auto& n : ir["nodes"]) requirement = requirement || n["id"] == "REQ-00001-1";
  EXPECT_TRUE(requirement);
  for (const auto& f : model.findings()) EXPECT_NE(f.code, "W-DIAGRAM-UNDECLARED") << f.message;  // an empty box is no broken name
}

TEST(TestEveryBox, ABoxIsDrawnWithOrWithoutChildren) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  package sales {\n"
                               "    @consumes shop@warehouse@counter\n"
                               "  }\n"
                               "  package warehouse {\n"
                               "    interface counter {\n"
                               "      kind: library\n"
                               "      operation take(item: string) -> bool\n"
                               "    }\n"
                               "  }\n"
                               "  module empty { }\n"
                               "}\n"
                               "diagram HLD_Shop {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:HLD_Shop", false));
  std::map<std::string, std::string> parentOf;
  for (const auto& n : ir["nodes"]) parentOf[n["id"]] = n.contains("parent") ? n["parent"].get<std::string>() : "";
  EXPECT_TRUE(parentOf.count("shop@sales"));                        // a box with nothing inside
  EXPECT_TRUE(parentOf.count("shop@empty"));                        // a module with nothing inside
  EXPECT_EQ(parentOf["shop@warehouse@counter"], "shop@warehouse");  // an interface sits in its box
}

TEST(TestDependencyDrawing, OneDependencyIsDrawnAlikeWithOrWithoutOpenAndADeclaredUseIsDrawn) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  package front {\n"
                               "    @consumes shop@back@orders\n"
                               "    module web {\n"
                               "      class Page {\n"
                               "        + show(id: string) -> none\n"
                               "      }\n"
                               "      sequence Page::show {\n"
                               "        local ledger: shop@back@store::Ledger\n"
                               "        self -> ledger.find(id) -> order\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "  package back {\n"
                               "    interface orders {\n"
                               "      kind: library\n"
                               "      operation find(id: string) -> string\n"
                               "    }\n"
                               "    module store {\n"
                               "      class Ledger {\n"
                               "        + find(id: string) -> string\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "}\n"
                               "diagram CL_Open {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "}\n"
                               "diagram CL_Named {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "  open: shop@front, shop@front@web, shop@back, shop@back@store\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto edges = [&model, &metamodel](const std::string& d) {
    std::set<std::string> out;
    const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + d, false));
    for (const auto& e : ir["edges"]) out.insert(e["from"].get<std::string>() + " -> " + e["to"].get<std::string>());
    return out;
  };
  const auto open = edges("CL_Open");
  EXPECT_EQ(open, edges("CL_Named"));
  EXPECT_TRUE(open.count("shop@front@web::Page -> shop@back@store::Ledger"));  // the subject, not its module
  EXPECT_FALSE(open.count("shop@front@web -> shop@back@store::Ledger"));
  EXPECT_TRUE(open.count("shop@front -> shop@back@orders"));  // the declared @consumes
}

/// A depth written with more digits than an int holds (DS-337): std::stoi threw out_of_range and ds stopped with
/// "ds: stoi" while ds check said nothing. It is no number, as a word is: the same finding, and drawn the same.
class TestDepthTooLarge : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("deep.ds",
                                       "package p {\n"
                                       "  module m {\n"
                                       "    class A {\n"
                                       "      + run() -> void\n"
                                       "      + go() -> void\n"
                                       "    }\n"
                                       "    sequence A::run {\n"
                                       "      -> go()\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n"
                                       "diagram SEQ_Deep {\n"
                                       "  kind: sequence\n"
                                       "  include: p@m::A::run\n"
                                       "  depth: 99999999999\n"
                                       "}\n"
                                       "diagram SEQ_Lots {\n"
                                       "  kind: sequence\n"
                                       "  include: p@m::A::run\n"
                                       "  depth: lots\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  /// The W-DIAGRAM-DEPTH findings that name a diagram.
  std::vector<ds::model::Finding> depthOf(const std::string& diagram) const {
    std::vector<ds::model::Finding> out;
    for (const auto& f : model.findings()) {
      if (f.code == "W-DIAGRAM-DEPTH" && f.message.rfind(diagram + " ", 0) == 0) out.push_back(f);
    }
    return out;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestDepthTooLarge, ADepthTooLargeForAnIntIsTheFindingOfADepthThatIsNoNumber) {
  const auto deep = depthOf("SEQ_Deep");
  const auto lots = depthOf("SEQ_Lots");
  ASSERT_EQ(deep.size(), 1U);
  ASSERT_EQ(lots.size(), 1U);
  EXPECT_EQ(deep[0].severity, lots[0].severity);
  EXPECT_EQ(deep[0].message, "SEQ_Deep asks for depth '99999999999': a number, or * for every level");
  // where the diagram is written, as for a depth that is a word
  const ds::model::Location& at = model.find("diagram:SEQ_Deep")->location;
  EXPECT_EQ(deep[0].location.file, "deep.ds");
  EXPECT_EQ(deep[0].location.line, at.line);
  EXPECT_EQ(deep[0].location.column, at.column);
  EXPECT_EQ(deep[0].location.line, 12);
}

TEST_F(TestDepthTooLarge, ADepthTooLargeForAnIntDrawsAsADepthThatIsNoNumberAndNeverAnAbort) {
  const ds::render::IrExport ir(model, metamodel);
  std::string deep;
  EXPECT_NO_THROW(deep = ir.render("diagram:SEQ_Deep", false));
  ASSERT_FALSE(deep.empty());
  const auto drawn = nlohmann::json::parse(deep);
  const auto word = nlohmann::json::parse(ir.render("diagram:SEQ_Lots", false));
  EXPECT_FALSE(drawn["edges"].empty());
  EXPECT_EQ(drawn["nodes"], word["nodes"]);
  EXPECT_EQ(drawn["edges"], word["edges"]);
}

/// A diagram drawn as ds render -f svg draws it, from the model to the page: what it draws and how long it takes.
struct Drawn {
  std::string ir;
  std::string svg;
  long long ms = 0;
};

/// A workspace of one file, read by the real steps, whose diagrams are drawn and timed.
class TestPastTheReach : public ::testing::Test {
 protected:
  void load(const std::string& file, const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto parsed = parser.parseText(file, text);
    ASSERT_FALSE(parsed.hasSyntaxErrors) << parsed.errorDetail;
    binder.bind(parsed);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  Drawn draw(const std::string& diagram) const {
    Drawn out;
    const auto start = std::chrono::steady_clock::now();
    out.ir = ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false);
    out.svg = ds::render::Svg().render(ds::render::IrDocument::parse(out.ir));
    out.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    return out;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

/// A neighbours: an int holds but past what the selection reaches (DS-362): the walk took every step it was asked
/// for, each one over every relation, so neighbours: 2000000000 kept ds render busy for good while ds check said
/// nothing. A step that adds nothing ends it. A neighbours: that is no number is a finding, as a depth is.
class TestNeighboursPastTheReach : public TestPastTheReach {
 protected:
  void SetUp() override {
    load("near.ds",
         "package p {\n"
         "  module m {\n"
         "    class A {\n"
         "      - b: B\n"
         "    }\n"
         "    class B {\n"
         "      - c: C\n"
         "    }\n"
         "    class C\n"
         "  }\n"
         "}\n"
         "\n"
         "diagram CL_N {\n"
         "  kind: class\n"
         "  include: p@m::A\n"
         "  neighbours: 2000000000\n"
         "}\n"
         "diagram CL_Lots {\n"
         "  kind: class\n"
         "  include: p@m::A\n"
         "  neighbours: lots\n"
         "}\n"
         "diagram CL_Wide {\n"
         "  kind: class\n"
         "  include: p@m::A\n"
         "  neighbours: 99999999999\n"
         "}\n");
  }

  /// The W-DIAGRAM-NEIGHBOURS findings that name a diagram.
  std::vector<ds::model::Finding> neighboursOf(const std::string& diagram) const {
    std::vector<ds::model::Finding> out;
    for (const auto& f : model.findings()) {
      if (f.code == "W-DIAGRAM-NEIGHBOURS" && f.message.rfind(diagram + " ", 0) == 0) out.push_back(f);
    }
    return out;
  }

  static std::set<std::string> nodes(const std::string& ir) {
    std::set<std::string> out;
    const auto drawn = nlohmann::json::parse(ir);  // held: a range over a member of a temporary would dangle
    for (const auto& n : drawn["nodes"]) out.insert(n["id"].get<std::string>());
    return out;
  }
};

TEST_F(TestNeighboursPastTheReach, NeighboursPastTheReachDrawTheReachAndEndPromptly) {
  const Drawn n = draw("CL_N");
  EXPECT_LT(n.ms, 1000);  // it never ended; three classes take a few milliseconds, in a Debug build too
  EXPECT_NE(n.svg.find("<svg"), std::string::npos);
  // A, and what it relates to two steps away, in the module around them: the reach, and nothing else
  EXPECT_EQ(nodes(n.ir), (std::set<std::string>{"p@m", "p@m::A", "p@m::B", "p@m::C"}));
  EXPECT_TRUE(neighboursOf("CL_N").empty());  // a number an int holds, however large, is no finding
}

TEST_F(TestNeighboursPastTheReach, NeighboursThatAreNoNumberAreAFindingAtTheDiagramAsADepthIs) {
  const auto lots = neighboursOf("CL_Lots");
  const auto wide = neighboursOf("CL_Wide");
  ASSERT_EQ(lots.size(), 1U);
  ASSERT_EQ(wide.size(), 1U);
  EXPECT_EQ(lots[0].severity, "warning");
  EXPECT_EQ(lots[0].message, "CL_Lots asks for neighbours 'lots': a number of steps");
  EXPECT_EQ(wide[0].message, "CL_Wide asks for neighbours '99999999999': a number of steps");  // too large for an int
  // where the diagram is written, as for a depth
  const ds::model::Location& at = model.find("diagram:CL_Lots")->location;
  EXPECT_EQ(lots[0].location.file, "near.ds");
  EXPECT_EQ(lots[0].location.line, at.line);
  EXPECT_EQ(lots[0].location.column, at.column);
  EXPECT_EQ(lots[0].location.line, 18);
  // and drawn with no neighbours, never an abort
  std::string drawn;
  EXPECT_NO_THROW(drawn = draw("CL_Lots").ir);
  EXPECT_EQ(nodes(drawn), (std::set<std::string>{"p@m", "p@m::A"}));
}

/// A depth an int holds but past what the calls reach (DS-362): the walk goes down only into a behaviour a call
/// enters, once on each way down, so it ends where the calls end, as * does, whatever the number.
class TestDepthPastTheReach : public TestPastTheReach {
 protected:
  void SetUp() override {
    load("calls.ds",
         "package p {\n"
         "  module m {\n"
         "    class A {\n"
         "      + run() -> void\n"
         "      + go() -> void\n"
         "      + stop() -> void\n"
         "    }\n"
         "    sequence A::run {\n"
         "      -> go()\n"
         "    }\n"
         "    sequence A::go {\n"
         "      -> stop()\n"
         "    }\n"
         "    sequence A::stop {\n"
         "      -> run()\n"
         "    }\n"
         "  }\n"
         "}\n"
         "diagram SEQ_Huge {\n"
         "  kind: sequence\n"
         "  include: p@m::A::run\n"
         "  depth: 2147483647\n"
         "}\n"
         "diagram SEQ_All {\n"
         "  kind: sequence\n"
         "  include: p@m::A::run\n"
         "  depth: *\n"
         "}\n"
         "diagram SEQ_One {\n"
         "  kind: sequence\n"
         "  include: p@m::A::run\n"
         "  depth: 1\n"
         "}\n");
  }
};

TEST_F(TestDepthPastTheReach, ADepthPastTheCallsDrawsAsEveryLevelAndEndsPromptly) {
  const Drawn huge = draw("SEQ_Huge");
  EXPECT_LT(huge.ms, 1000);
  const auto drawn = nlohmann::json::parse(huge.ir);
  const auto all = nlohmann::json::parse(draw("SEQ_All").ir);
  const auto one = nlohmann::json::parse(draw("SEQ_One").ir);
  EXPECT_FALSE(drawn["edges"].empty());
  EXPECT_EQ(drawn["nodes"], all["nodes"]);
  EXPECT_EQ(drawn["edges"], all["edges"]);
  EXPECT_NE(drawn["edges"], one["edges"]);  // the depth is read: one level draws less
}

}  // namespace
