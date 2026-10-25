// System qualification: ds driven from outside, as a user drives it, one test
// per promise of the system requirements (design/requirements.ds). Nothing
// here links the engine: the binary, its arguments, its output, its exit code.
#include <gtest/gtest.h>
#include <sys/wait.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <map>
#include <set>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

namespace {

struct Outcome {
  std::string out;
  int code = -1;
};

Outcome ds(const std::string& args) {
  const std::string command = std::string(DRIFTSPEC_DS_BIN) + " " + args + " 2>/dev/null";
  Outcome run;
  FILE* p = ::popen(command.c_str(), "r");
  if (!p) return run;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof buffer, p)) > 0) run.out.append(buffer, n);
  const int status = ::pclose(p);
  run.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return run;
}

std::string examples(const std::string& corpus) { return std::string(DRIFTSPEC_EXAMPLES_DIR) + "/" + corpus; }

int widest(const std::string& text) {
  int w = 0, current = 0;
  for (unsigned char c : text) {
    if (c == '\n') {
      w = std::max(w, current);
      current = 0;
    } else if ((c & 0xC0) != 0x80) {
      ++current;  // one cell per code point
    }
  }
  return std::max(w, current);
}

class TestQualification : public ::testing::Test {};

// SREQ-00001-1: one model, one identity per entity, duplicates reported
TEST_F(TestQualification, AWorkspaceLoadsIntoOneModelWithOneIdentityPerEntity) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("static")).out);
  bool model = false, scopedCheck = false;
  for (const auto& e : exported["entities"]) {
    model |= e["id"] == "ds@ds::Model";
    scopedCheck |= e["id"] == "ds@ds::ScopedCheck" && e["scope"] == "ds@ds";
  }
  EXPECT_TRUE(model);
  EXPECT_TRUE(scopedCheck);
  const Outcome twice = ds("check " + examples("behaviour"));
  EXPECT_NE(twice.out.find("E-DUPLICATE-IDENTITY"), std::string::npos);  // the same scenario declared twice
  EXPECT_EQ(twice.code, 1);
}

// SREQ-00002-1: every incoherence is a finding with code, severity and location; a dangling reference never stops the load
TEST_F(TestQualification, EveryIncoherenceIsAFindingAtItsLocation) {
  const Outcome check = ds("check " + examples("artefacts"));
  const std::regex line(R"(^[^\n]+\.ds:[0-9]+:[0-9]+  [EW]-[A-Z-]+  .+$)", std::regex::multiline);
  EXPECT_TRUE(std::regex_search(check.out, line));
  EXPECT_NE(check.out.find("E-REF-UNPINNED"), std::string::npos);
  EXPECT_NE(check.out.find("W-REVISION-DRIFT"), std::string::npos);
  EXPECT_EQ(check.code, 1);  // an error among the findings
  const auto exported = nlohmann::json::parse(ds("export " + examples("static")).out);
  EXPECT_GT(exported["entities"].size(), 50u);  // loaded whole, with its W-REF-UNDECLARED
  bool undeclared = false;
  for (const auto& f : exported["findings"]) undeclared |= f["code"] == "W-REF-UNDECLARED" && f["severity"] == "warning";
  EXPECT_TRUE(undeclared);
}

// SREQ-00003-1: artefacts linked to the design by revision
TEST_F(TestQualification, ArtefactsAreLinkedByRevision) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("artefacts")).out);
  bool refines = false, verifies = false, realises = false;
  for (const auto& r : exported["relations"]) {
    refines |= r["kind"] == "refines" && r["from"] == "REQ-00001-2" && r["to"] == "SREQ-00001-2";
    verifies |= r["kind"] == "verifies" && r["from"] == "VER-00001-3" && r["to"] == "REQ-00001-2";
    realises |= r["kind"] == "requires" && r["from"] == "iris@projection::Projection" && r["to"] == "REQ-00001-2";
  }
  EXPECT_TRUE(refines);
  EXPECT_TRUE(verifies);
  EXPECT_TRUE(realises);
}

// SREQ-00004-1: behaviour bound to its subject and checked against it
TEST_F(TestQualification, BehaviourIsBoundToItsSubjectAndChecked) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("behaviour")).out);
  bool bound = false, expands = false;
  for (const auto& r : exported["relations"]) {
    bound |= r["kind"] == "behaviour-of" && r["from"] == "ds@ds::Checker::run::sequence" && r["to"] == "ds@ds::Checker::run";
    expands |= r["kind"] == "expands-to" && r["to"] == "ds@ds::Checker::resolveReference::sequence";
  }
  EXPECT_TRUE(bound);
  EXPECT_TRUE(expands);
  const Outcome check = ds("check " + examples("behaviour"));
  EXPECT_NE(check.out.find("E-MSG-ARITY"), std::string::npos);
  EXPECT_NE(check.out.find("W-PARTICIPANT-UNDECLARED"), std::string::npos);
}

// SREQ-00005-1: any selection drawn as SVG and as text, adapting to a width
TEST_F(TestQualification, ASelectionIsDrawnAsSvgAndAsTextToAWidth) {
  const Outcome svg = ds("render " + examples("static") + " CL_Package_ds --svg");
  EXPECT_EQ(svg.out.rfind("<svg", 0), 0u);
  EXPECT_NE(svg.out.find("data-id=\"ds@ds::Model\""), std::string::npos);
  const Outcome wide = ds("render " + examples("static") + " CL_Package_ds --text");
  const Outcome narrow = ds("render " + examples("static") + " CL_Package_ds --text --width 80");
  EXPECT_GT(widest(wide.out), 80);
  EXPECT_LE(widest(narrow.out), 80);
  EXPECT_NE(narrow.out.find("Model"), std::string::npos);
}

// SREQ-00006-1: the model exported for other tools, the drawing as the family's IR
TEST_F(TestQualification, TheModelAndTheDrawingsAreExported) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("static")).out);
  EXPECT_TRUE(exported.contains("entities") && exported.contains("relations") && exported.contains("findings"));
  const auto ir = nlohmann::json::parse(ds("render " + examples("static") + " CL_Values").out);
  EXPECT_EQ(ir["ir"], 1);
  EXPECT_FALSE(ir["nodes"].empty());
}

// SREQ-00007-1: the difference between two versions, in the model's terms
TEST_F(TestQualification, TwoVersionsAreComparedByIdentity) {
  const Outcome same = ds("diff " + examples("static") + " " + examples("static"));
  EXPECT_NE(same.out.find("no design change"), std::string::npos);
  const std::string tmp = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ds-qualification-" + std::to_string(::getpid());
  std::system(("mkdir -p " + tmp + " && cp " + examples("static") + "/static.ds " + tmp + "/").c_str());
  std::ifstream in(tmp + "/static.ds");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  text.replace(text.find("class Vector <<==, +, *, []>>"), std::string("class Vector <<==, +, *, []>>").size(), "class Vec3 <<==, +, *, []>>");
  std::ofstream(tmp + "/static.ds") << text;
  const Outcome diff = ds("diff " + examples("static") + " " + tmp);
  EXPECT_NE(diff.out.find("> class ds@ds::Vec3   (Vector -> Vec3)"), std::string::npos);  // a rename, not a removal and an addition
  std::system(("rm -rf " + tmp).c_str());
}

// SREQ-00008-1: the grammar and its queries, for editors
TEST_F(TestQualification, TheGrammarShipsWithQueriesThatNameItsNodes) {
  std::ifstream types(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/src/node-types.json");
  ASSERT_TRUE(types.good());
  const auto nodes = nlohmann::json::parse(types);
  std::set<std::string> named;
  for (const auto& n : nodes) {
    if (n.value("named", false)) named.insert(n["type"].get<std::string>());
  }
  for (const char* query : {"highlights.scm", "folds.scm"}) {
    std::ifstream q(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/queries/" + query);
    ASSERT_TRUE(q.good()) << query;
    std::string raw((std::istreambuf_iterator<char>(q)), std::istreambuf_iterator<char>());
    std::string text;  // without the comment lines
    for (size_t at = 0; at < raw.size();) {
      const size_t end = raw.find('\n', at);
      const std::string line = raw.substr(at, end == std::string::npos ? std::string::npos : end - at);
      if (line.rfind(";", 0) != 0) text += line + "\n";
      at = end == std::string::npos ? raw.size() : end + 1;
    }
    const std::regex node(R"(\(([a-z_]+)[\s)])");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), node); it != std::sregex_iterator(); ++it) {
      EXPECT_TRUE(named.count((*it)[1].str())) << query << " names a node the grammar does not define: " << (*it)[1].str();
    }
  }
}

// SREQ-00009-1: the static metamodel, with what is derived derived
TEST_F(TestQualification, TheStaticMetamodelIsModelledAndClassifiedByDerivation) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("static")).out);
  std::map<std::string, nlohmann::json> by;
  for (const auto& e : exported["entities"]) by[e["id"]] = e;
  EXPECT_EQ(by["ds@ds::Check"]["fields"]["stereotypes"], "interface");
  EXPECT_EQ(by["ds@ds::ScopedCheck"]["fields"]["stereotypes"], "abstract");
  EXPECT_EQ(by["ds@ds::Location"]["fields"]["stereotypes"], "value, equatable, ordered, printable");
  EXPECT_EQ(by["ds@ds::Repository<1>"]["fields"]["bound.T"], "Entity");
  EXPECT_EQ(by["ds@ds::Vector"]["fields"]["operators"], "==, +, *, []");
  bool owns = false, refs = false;
  for (const auto& r : exported["relations"]) {
    owns |= r["kind"] == "owns" && r["from"] == "ds@ds::Finding::location" && r.value("multiplicity", "") == "1";
    refs |= r["kind"] == "refs" && r["from"] == "ds@ds::ScopedCheck::scopes" && r.value("multiplicity", "") == "*";
  }
  EXPECT_TRUE(owns);
  EXPECT_TRUE(refs);
  const auto diag = nlohmann::json::parse(ds("export " + examples("diag")).out);
  bool boundary = false;
  for (const auto& e : diag["entities"]) boundary |= e["id"] == "ds@cli" && e["kind"] == "boundary";
  EXPECT_TRUE(boundary);
}

// SREQ-00010-1: a diagram kind per view: class, requirement, sequence (state: not yet drawn)
TEST_F(TestQualification, ClassRequirementAndSequenceDiagramsAreViewsOfTheModel) {
  const auto cl = nlohmann::json::parse(ds("render " + examples("static") + " CL_Model").out);
  EXPECT_EQ(cl["nodes"][0]["kind"], "container");
  const auto req = nlohmann::json::parse(ds("render " + examples("artefacts") + " REQ_Tracker").out);
  bool requirement = false;
  for (const auto& n : req["nodes"]) requirement |= n["classifier"] == "requirement";
  EXPECT_TRUE(requirement);
  const auto seq = nlohmann::json::parse(ds("render " + examples("behaviour") + " SEQ_Run").out);
  bool lifeline = false;
  for (const auto& n : seq["nodes"]) lifeline |= n["kind"] == "lifeline";
  EXPECT_TRUE(lifeline);
}

// CST-00001-1: every command under a hundred milliseconds on a hundred classes
TEST_F(TestQualification, AHundredClassesCheckAndDrawInUnderAHundredMilliseconds) {
  const std::string tmp = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ds-perf-" + std::to_string(::getpid());
  std::system(("mkdir -p " + tmp).c_str());
  std::ofstream ws(tmp + "/big.ds");
  ws << "package big {\n  module m {\n";
  for (int i = 0; i < 100; ++i) {
    ws << "    class C" << i << (i > 0 ? " : C" + std::to_string((i - 1) / 3) : "") << " {\n      - owns next: C" << (i + 1) % 100 << "\n      + run(a: int) -> C" << (i + 7) % 100 << "\n    }\n";
  }
  ws << "  }\n}\ndiagram CL_Big {\n  kind: class\n  include: big@*\n}\n";
  ws.close();
  for (const char* command : {"check", "export", "render"}) {
    const std::string args = std::string(command) + " " + tmp + (std::string(command) == "render" ? " CL_Big --svg" : "");
    const auto start = std::chrono::steady_clock::now();
    const Outcome run = ds(args);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(ms, 100) << command;
    EXPECT_FALSE(run.out.empty()) << command;
  }
  std::system(("rm -rf " + tmp).c_str());
}

}  // namespace
