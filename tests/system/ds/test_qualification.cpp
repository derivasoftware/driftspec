// System qualification: ds driven from outside, as a user drives it, one test
// per promise of the system requirements (design/requirements.ds). Nothing
// here links the engine: the binary, its arguments, its output, its exit code.
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <sstream>
#include <string>

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
  const Outcome missing = ds("check " + examples("nowhere"));  // a workspace that is not a directory is refused, never a crash
  EXPECT_EQ(missing.code, 1);
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

// SREQ-00003-2: artefacts linked to the design by revision, boxes and classes alike
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
  // an HLD box realises a requirement the same way (revision 2)
  const auto packages = nlohmann::json::parse(ds("export " + examples("packages")).out);
  bool box = false;
  for (const auto& r : packages["relations"]) box |= r["kind"] == "requires" && r["from"] == "app@core" && r["to"] == "SREQ-00001-1";
  EXPECT_TRUE(box);
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
  const Outcome svg = ds("render " + examples("static") + " CL_Package_ds -f svg");
  EXPECT_EQ(svg.out.rfind("<svg", 0), 0u);
  EXPECT_NE(svg.out.find("data-id=\"ds@ds::Model\""), std::string::npos);
  const Outcome wide = ds("render " + examples("static") + " CL_Package_ds -f text");
  const Outcome narrow = ds("render " + examples("static") + " CL_Package_ds -f text -w 80");
  EXPECT_GT(widest(wide.out), 80);
  EXPECT_LE(widest(narrow.out), 80);
  EXPECT_NE(narrow.out.find("Model"), std::string::npos);
}

// SREQ-00006-1: the model exported for other tools, the drawing as the family's IR
TEST_F(TestQualification, TheModelAndTheDrawingsAreExported) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("static")).out);
  EXPECT_TRUE(exported.contains("entities") && exported.contains("relations") && exported.contains("findings"));
  const auto ir = nlohmann::json::parse(ds("render " + examples("static") + " CL_Values -f ir").out);
  EXPECT_EQ(ir["ir"], 1);
  EXPECT_FALSE(ir["nodes"].empty());
}

// SREQ-00013-1: the whole model laid out once, nested, in two variants, for a viewer that zooms
TEST_F(TestQualification, TheWholeModelIsLaidOutOnceForAViewer) {
  const Outcome run = ds("render " + examples("static") + " -f json");
  EXPECT_EQ(run.code, 0);
  const auto scene = nlohmann::json::parse(run.out);
  ASSERT_FALSE(scene["nodes"].empty());
  std::map<std::string, nlohmann::json> byId;
  for (const auto& n : scene["nodes"]) byId[n["id"].get<std::string>()] = n;
  int nested = 0;
  for (const auto& n : scene["nodes"]) {
    const std::string parent = n["parent"];
    if (parent.empty() || n["kind"] == "interface") continue;
    const auto& b = n["full"];
    const auto& p = byId.at(parent)["full"];
    EXPECT_TRUE(b[0] >= p[0] && b[1] >= p[1] && b[0].get<double>() + b[2].get<double>() <= p[0].get<double>() + p[2].get<double>() + 0.1)
        << n["id"];
    ++nested;
  }
  EXPECT_GT(nested, 10);
  EXPECT_EQ(scene["lean_world"].size(), 4u);
}

// SREQ-00013-1: the viewer is one page ds writes, with the scene inside, that a browser opens as it is
TEST_F(TestQualification, TheViewerIsOnePageWithTheWholeModelInside) {
  const Outcome run = ds("render " + examples("static"));
  EXPECT_EQ(run.code, 0);
  EXPECT_EQ(run.out.rfind("<!doctype html>", 0), 0u);
  EXPECT_NE(run.out.find("static \u00b7 DriftSpec"), std::string::npos);
  const auto scene = nlohmann::json::parse(ds("render " + examples("static") + " -f json").out);
  for (const auto& n : scene["nodes"]) EXPECT_NE(run.out.find("\"" + n["id"].get<std::string>() + "\""), std::string::npos) << n["id"];
}

// SREQ-00013-1: the viewer embeds into another document as one frame, in the reader's colours
TEST_F(TestQualification, TheViewerEmbedsAsAFrameInTheReadersColours) {
  const std::string tmp =
      std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ds-theme-" + std::to_string(::getpid()) + ".css";
  std::ofstream(tmp) << ":root{--bg:#0e1116}";
  const Outcome run = ds("render " + examples("static") + " --embed --height 420 --css " + tmp);
  std::remove(tmp.c_str());
  EXPECT_EQ(run.code, 0);
  EXPECT_EQ(run.out.rfind("<iframe ", 0), 0u);
  EXPECT_NE(run.out.find("height:420px"), std::string::npos);
  EXPECT_NE(run.out.find("--bg:#0e1116"), std::string::npos);
  EXPECT_EQ(ds("render " + examples("static") + " --css /no/such.css").code, 2);  // a stylesheet that is not there is bad usage
}

// SREQ-00002-1: the findings, for a machine: each with its code, severity and location, then the counts
TEST_F(TestQualification, TheFindingsAreJsonForAMachine) {
  const Outcome run = ds("check " + examples("artefacts") + " --json");
  EXPECT_EQ(run.code, 1);  // the same exit as the text form: an error among the findings
  const auto doc = nlohmann::json::parse(run.out);
  ASSERT_FALSE(doc["findings"].empty());
  for (const auto& f : doc["findings"]) {
    EXPECT_TRUE(f.contains("code") && f.contains("severity") && f["location"].contains("line")) << f;
  }
  EXPECT_GT(doc["entities"].get<int>(), 0);
}

// SREQ-00013-1: a diagram named, the viewer opens framed on what it shows
TEST_F(TestQualification, TheViewerOpensFramedOnTheDiagramNamed) {
  const Outcome run = ds("render " + examples("static") + " CL_Values");
  EXPECT_EQ(run.code, 0);
  const auto start = run.out.find("const FOCUS=") + std::string("const FOCUS=").size();
  const auto focus = nlohmann::json::parse(run.out.substr(start, run.out.find(';', start) - start));
  EXPECT_FALSE(focus.empty());
  EXPECT_EQ(ds("render " + examples("static") + " NoSuchDiagram").code, 1);
}

// SREQ-00007-1: the difference between two versions, in the model's terms
TEST_F(TestQualification, TwoVersionsAreComparedByIdentity) {
  const Outcome same = ds("diff " + examples("static") + " " + examples("static"));
  EXPECT_NE(same.out.find("no design change"), std::string::npos);
  const std::string tmp =
      std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ds-qualification-" + std::to_string(::getpid());
  std::system(("mkdir -p " + tmp + " && cp " + examples("static") + "/static.ds " + tmp + "/").c_str());
  std::ifstream in(tmp + "/static.ds");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  text.replace(text.find("class Vector <<==, +, *, []>>"), std::string("class Vector <<==, +, *, []>>").size(),
               "class Vec3 <<==, +, *, []>>");
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

// SREQ-00009-2: the static metamodel, with what is derived derived
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
  // packages nest, every level adding its name with @, and the flat form reopens by path
  const auto packages = nlohmann::json::parse(ds("export " + examples("packages")).out);
  std::set<std::string> ids;
  for (const auto& e : packages["entities"]) ids.insert(e["id"]);
  EXPECT_TRUE(ids.count("app@core@model::Entity"));
  EXPECT_TRUE(ids.count("app@render@svg::Svg"));
  const auto diag = nlohmann::json::parse(ds("export " + examples("diag")).out);
  bool interface = false;
  for (const auto& e : diag["entities"]) interface |= e["id"] == "ds@cli" && e["kind"] == "interface";
  EXPECT_TRUE(interface);
}

// SREQ-00010-1: a diagram kind per view: class, requirement, sequence and state
TEST_F(TestQualification, EveryKindOfDiagramIsAViewOfTheModel) {
  const auto cl = nlohmann::json::parse(ds("render " + examples("static") + " CL_Model -f ir").out);
  EXPECT_EQ(cl["nodes"][0]["kind"], "container");
  const auto req = nlohmann::json::parse(ds("render " + examples("artefacts") + " REQ_Tracker -f ir").out);
  bool requirement = false;
  for (const auto& n : req["nodes"]) requirement |= n["classifier"] == "requirement";
  EXPECT_TRUE(requirement);
  const auto seq = nlohmann::json::parse(ds("render " + examples("behaviour") + " SEQ_Run -f ir").out);
  bool lifeline = false;
  for (const auto& n : seq["nodes"]) lifeline |= n["kind"] == "lifeline";
  EXPECT_TRUE(lifeline);
  const auto state = nlohmann::json::parse(ds("render " + examples("behaviour") + " STM_Checker -f ir").out);
  bool stateBox = false;
  for (const auto& n : state["nodes"]) stateBox |= n["classifier"] == "state";
  EXPECT_TRUE(stateBox);
  EXPECT_FALSE(state["edges"].empty());
}

// SREQ-00011-1: questions about the model answered by identity, so one answer leads to the next
TEST_F(TestQualification, AQuestionAboutTheModelIsAnsweredByIdentity) {
  const Outcome diagrams = ds("query " + examples("artefacts") + " diagrams");
  EXPECT_EQ(diagrams.code, 0);
  EXPECT_NE(diagrams.out.find("diagram  REQ_Tracker  requirement"), std::string::npos);
  const Outcome relations = ds("query " + examples("artefacts") + " relations SREQ-00001-2 --json");
  bool refinedBy = false;
  for (const auto& a : nlohmann::json::parse(relations.out)) refinedBy |= a["kind"] == "refines" && a["detail"] == "<-";
  EXPECT_TRUE(refinedBy);
  // the answer names an identity that the next question takes
  const Outcome artefacts = ds("query " + examples("artefacts") + " artefacts iris@projection::Projection");
  EXPECT_NE(artefacts.out.find("requires  REQ-00001-2"), std::string::npos);
  EXPECT_NE(artefacts.out.find("verified-by  VER-"), std::string::npos);
  const Outcome shows = ds("query " + examples("artefacts") + " shows CL_Projection");
  EXPECT_NE(shows.out.find("class  iris@projection::Projection"), std::string::npos);
  EXPECT_EQ(ds("query " + examples("artefacts") + " weather").code, 1);
}

// SREQ-00012-1: the language is formal: every oracle corpus means exactly the findings it marks
TEST_F(TestQualification, EveryCorpusMeansExactlyTheFindingsItMarks) {
  const std::regex code(R"([EW]-[A-Z][A-Z-]+)");
  int corpora = 0;
  for (const auto& dir : std::filesystem::directory_iterator(std::string(DRIFTSPEC_EXAMPLES_DIR))) {
    if (!dir.is_directory()) continue;
    ++corpora;
    std::set<std::string> marked, reported;
    for (const auto& f : std::filesystem::recursive_directory_iterator(dir.path())) {
      if (f.path().extension() != ".ds") continue;
      std::ifstream in(f.path());
      for (std::string line; std::getline(in, line);) {
        const auto mark = line.find("// !");
        if (mark == std::string::npos) continue;
        for (std::sregex_iterator it(line.begin() + static_cast<long>(mark), line.end(), code), end; it != end; ++it)
          marked.insert(it->str());
      }
    }
    const Outcome check = ds("check " + dir.path().string());
    std::istringstream out(check.out);
    for (std::string line; std::getline(out, line);) {
      std::smatch m;
      if (line.find(":") != std::string::npos && std::regex_search(line, m, code) && line.find("  " + m.str() + "  ") != std::string::npos)
        reported.insert(m.str());
    }
    EXPECT_EQ(reported, marked) << dir.path().filename();
  }
  EXPECT_GE(corpora, 5);
}

// CST-00001-1: every command under a hundred milliseconds on a hundred classes
TEST_F(TestQualification, AHundredClassesCheckAndDrawInUnderAHundredMilliseconds) {
  const std::string tmp = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ds-perf-" + std::to_string(::getpid());
  std::system(("mkdir -p " + tmp).c_str());
  std::ofstream ws(tmp + "/big.ds");
  ws << "package big {\n  module m {\n";
  for (int i = 0; i < 100; ++i) {
    ws << "    class C" << i << (i > 0 ? " : C" + std::to_string((i - 1) / 3) : "") << " {\n      - owns next: C" << (i + 1) % 100
       << "\n      + run(a: int) -> C" << (i + 7) % 100 << "\n    }\n";
  }
  ws << "  }\n}\ndiagram CL_Big {\n  kind: class\n  include: big@*\n}\n";
  ws.close();
  for (const std::string& args : {"check " + tmp, "export " + tmp, "render " + tmp + " CL_Big -f svg", "render " + tmp}) {
    const std::string command = args.substr(0, args.find(' ')) + (args.find("CL_Big") != std::string::npos ? " (svg)" : " (html)");
    const auto start = std::chrono::steady_clock::now();
    const Outcome run = ds(args);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    // the budget is the product's (Release); a Debug ds runs the same commands under a looser bound, so the test still exercises them
    const bool release = std::string(DRIFTSPEC_BUILD_TYPE) == "Release";
    EXPECT_LT(ms, release ? 100 : 300) << command << (release ? "" : " (Debug build: the 100 ms budget is measured on Release)");
    EXPECT_FALSE(run.out.empty()) << command;
  }
  std::system(("rm -rf " + tmp).c_str());
}

}  // namespace
