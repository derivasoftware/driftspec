// System qualification: ds driven from outside, as a user drives it, one test
// per promise of the system requirements (design/requirements.ds). Nothing
// here links the engine: the binary, its arguments, its output, its exit code.
#include <gtest/gtest.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

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
  Outcome run;
#ifdef _WIN32
  // cmd.exe takes the whole line in one more pair of quotes; the exit code comes back as it is (DS-283)
  const std::string command = "\"\"" + std::string(DRIFTSPEC_DS_BIN) + "\" " + args + " 2>NUL\"";
  FILE* p = ::_popen(command.c_str(), "rb");
#else
  const std::string command = std::string(DRIFTSPEC_DS_BIN) + " " + args + " 2>/dev/null";
  FILE* p = ::popen(command.c_str(), "r");
#endif
  if (!p) return run;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof buffer, p)) > 0) run.out.append(buffer, n);
#ifdef _WIN32
  run.code = ::_pclose(p);
#else
  const int status = ::pclose(p);
  run.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  return run;
}

/// A fresh folder of the system's temporary directory, for one test: removed by the test when it is done.
std::string scratch(const std::string& name) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto dir = std::filesystem::temp_directory_path() / (name + "-" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir.generic_string();
}

std::string examples(const std::string& corpus) { return std::string(DRIFTSPEC_CORPUS_DIR) + "/" + corpus; }

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
  EXPECT_EQ(missing.code, 2);                                  // as bad usage (cli::check exit 2), not as a design with errors
  const Outcome twice = ds("check " + examples("behaviour"));
  EXPECT_NE(twice.out.find("E-DUPLICATE-IDENTITY"), std::string::npos);  // the same scenario declared twice
  EXPECT_EQ(twice.code, 1);
}

// SREQ-00002-1: every incoherence is a finding with code, severity and location; a dangling reference never stops the load
TEST_F(TestQualification, EveryIncoherenceIsAFindingAtItsLocation) {
  const Outcome check = ds("check " + examples("artefacts"));
  // a line of its own, read line by line: std::regex::multiline is not in every standard library (DS-283)
  const std::regex line(R"([^\n]+\.ds:[0-9]+:[0-9]+  [EW]-[A-Z-]+  .+)");
  bool found = false;
  std::istringstream lines(check.out);
  for (std::string l; std::getline(lines, l);) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    found |= std::regex_match(l, line);
  }
  EXPECT_TRUE(found);
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
// DS-120: ds check --json prints the one shape of a check, the one driftspec.check returns
TEST_F(TestQualification, TheCheckJsonSaysTheFindingsAndTheCounts) {
  const Outcome check = ds("check --json " + examples("artefacts"));
  const auto out = nlohmann::json::parse(check.out);
  for (const char* key : {"findings", "entities", "expected", "relations", "errors"}) EXPECT_TRUE(out.contains(key)) << key;
  EXPECT_GT(out["errors"].get<int>(), 0);
  EXPECT_EQ(check.code, 1);
}

TEST_F(TestQualification, ArtefactsAreLinkedByRevision) {
  const auto exported = nlohmann::json::parse(ds("export " + examples("artefacts")).out);
  bool refines = false, verifies = false, realises = false;
  for (const auto& r : exported["relations"]) {
    refines |= r["kind"] == "refines" && r["from"] == "REQ-00001-2" && r["to"] == "SREQ-00001-2";
    verifies |= r["kind"] == "verifies" && r["from"] == "VER-00001-3" && r["to"] == "REQ-00001-2";
    realises |= r["kind"] == "satisfies" && r["from"] == "iris@projection::Projection" && r["to"] == "REQ-00001-2";
  }
  EXPECT_TRUE(refines);
  EXPECT_TRUE(verifies);
  EXPECT_TRUE(realises);
  // an HLD box realises a requirement the same way (revision 2)
  const auto packages = nlohmann::json::parse(ds("export " + examples("packages")).out);
  bool box = false;
  for (const auto& r : packages["relations"]) box |= r["kind"] == "satisfies" && r["from"] == "app@core" && r["to"] == "SREQ-00001-1";
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
  std::map<std::string, size_t> column;  // the document names its columns
  for (size_t i = 0; i < scene["fields"]["node"].size(); ++i) column[scene["fields"]["node"][i]] = i;
  const auto& nodes = scene["nodes"];
  int nested = 0;
  for (const auto& n : nodes) {
    const int parent = n[column["parent"]];
    if (parent < 0 || n[column["kind"]] == "interface") continue;
    const auto& b = n[column["full"]];
    const auto& p = nodes[static_cast<size_t>(parent)][column["full"]];
    EXPECT_TRUE(b[0] >= p[0] && b[1] >= p[1] && b[0].get<double>() + b[2].get<double>() <= p[0].get<double>() + p[2].get<double>() + 1)
        << n[column["id"]];
    ++nested;
  }
  EXPECT_GT(nested, 10);
  EXPECT_EQ(scene["lean_world"].size(), 4u);
  EXPECT_TRUE(scene["links"].is_array() && scene["diagrams"].is_array());  // what joins the boxes, and where they are drawn
}

// SREQ-00013-1: the viewer is one page ds writes, with the scene inside, that a browser opens as it is
TEST_F(TestQualification, TheViewerIsOnePageWithTheWholeModelInside) {
  const Outcome run = ds("render " + examples("static"));
  EXPECT_EQ(run.code, 0);
  EXPECT_EQ(run.out.rfind("<!doctype html>", 0), 0u);
  EXPECT_NE(run.out.find("static \u00b7 DriftSpec"), std::string::npos);
  const Outcome scene = ds("render " + examples("static") + " -f json");
  const auto at = run.out.find("const PACKED='");
  ASSERT_NE(at, std::string::npos);  // the scene travels packed inside the page
  EXPECT_GT(run.out.find('\'', at + 14), at + 100);
  EXPECT_LT(run.out.size(), scene.out.size() + 60u * 1024u);  // the whole viewer costs less than the scene unpacked plus its code
}

// SREQ-00013-1: the viewer embeds into another document as one frame, in the reader's colours
TEST_F(TestQualification, TheViewerEmbedsAsAFrameInTheReadersColours) {
  const std::string dir = scratch("ds-theme"), tmp = dir + "/theme.css";
  std::ofstream(tmp) << ":root{--bg:#0e1116}";
  const Outcome run = ds("render " + examples("static") + " --embed --height 420 --css " + tmp);
  std::filesystem::remove_all(dir);
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
  EXPECT_EQ(by["ds@ds::Check"]["fields"]["stereotypes"], "abstract");
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

// SREQ-00002-1: a path that is no workspace is bad usage on every command, the same way
TEST_F(TestQualification, EveryCommandTreatsAPathThatIsNoWorkspaceAsBadUsage) {
  const std::string tmp = scratch("ds-nowhere");
  std::filesystem::create_directories(tmp + "/empty");
  for (const std::string& path : {std::string("/nonexistent/workspace"), tmp + "/empty"}) {
    EXPECT_EQ(ds("check " + path).code, 2) << path;
    EXPECT_EQ(ds("export " + path).code, 2) << path;
    EXPECT_EQ(ds("render " + path).code, 2) << path;
    EXPECT_EQ(ds("query " + path + " diagrams").code, 2) << path;
  }
  std::filesystem::remove_all(tmp);
}

// SREQ-00002-1: a path that is no workspace is a command line to correct, not a design with errors
TEST_F(TestQualification, CheckingAPathThatIsNoWorkspaceIsBadUsage) {
  EXPECT_EQ(ds("check /nonexistent/workspace").code, 2);
  EXPECT_EQ(ds("check " + examples("static")).code, 1);  // its oracle carries errors: the design fails, exit 1
}

// SREQ-00018-2: input the tool cannot use is said on stderr with the code of bad input, never an abort; and a
// design nested far deeper than any written by hand is still read (DS-279)
TEST_F(TestQualification, InputThatCannotBeReadIsBadUsageAndDeepNestingIsRead) {
  const std::string tmp = scratch("ds-input");
  std::filesystem::create_directories(tmp + "/unreadable");
  std::filesystem::create_directories(tmp + "/deep");
  std::ofstream(tmp + "/unreadable/a.ds") << "package p { }\n";
  std::filesystem::permissions(tmp + "/unreadable/a.ds", std::filesystem::perms::none);
  // as root every file can be read, and on Windows no permission takes reading away: then nothing to try
  if (!std::ifstream(tmp + "/unreadable/a.ds").good()) {
    EXPECT_EQ(ds("check " + tmp + "/unreadable").code, 2);
  }
  std::ofstream deep(tmp + "/deep/a.ds");
  deep << "package q {\n";
  for (int i = 0; i < 3000; ++i) deep << "requirement REQ-" << 10000 + i << "-1 {\n title: The x shall y\n";
  for (int i = 0; i < 3000; ++i) deep << "}\n";
  deep << "}\n";
  deep.close();
  EXPECT_EQ(ds("check " + tmp + "/deep").code, 0);
  std::filesystem::permissions(tmp + "/unreadable/a.ds", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
  std::filesystem::remove_all(tmp);
}

// SREQ-00011-1: an identity the model does not declare is no empty answer: it fails, and names what was meant
TEST_F(TestQualification, AnIdentityTheModelDoesNotDeclareIsAnError) {
  const Outcome unknown = ds("query " + examples("artefacts") + " relations Projection");
  EXPECT_EQ(unknown.code, 1);
  EXPECT_EQ(ds("query " + examples("artefacts") + " relations").code, 1);  // the identity is not optional
  const Outcome hint = ds("query " + examples("artefacts") + " relations Projection --json");
  const auto answer = nlohmann::json::parse(hint.out);
  ASSERT_EQ(answer.size(), 1u);
  EXPECT_EQ(answer[0]["kind"], "error");
  EXPECT_NE(answer[0]["detail"].get<std::string>().find("iris@projection::Projection"), std::string::npos);
  EXPECT_EQ(nlohmann::json::parse(ds("query " + examples("artefacts") + " weather --json").out)[0]["kind"], "error");
}

// SREQ-00014-1: the tool carries its manual: how each kind is written, what each code means, the exit codes
// SREQ-00018-2: the tool says its version, and the version is the one its export carries (DS-278)
TEST_F(TestQualification, TheToolSaysItsVersion) {
  const Outcome said = ds("--version");
  EXPECT_EQ(said.code, 0);
  ASSERT_EQ(said.out.rfind("ds ", 0), 0U) << said.out;
  const std::string version = said.out.substr(3, said.out.find_last_not_of("\n") - 2);
  const Outcome exported = ds("export " + examples("states"));
  EXPECT_EQ(nlohmann::json::parse(exported.out)["version"], version);
}

TEST_F(TestQualification, TheToolCarriesItsManual) {
  const Outcome index = ds("manual");
  EXPECT_EQ(index.code, 0);
  EXPECT_NE(index.out.find("Kinds you write"), std::string::npos);
  const Outcome kind = ds("manual sequence --json");
  EXPECT_EQ(kind.code, 0);
  EXPECT_NE(nlohmann::json::parse(kind.out)["kind"]["example"].get<std::string>().find("sequence "), std::string::npos);
  EXPECT_NE(ds("manual E-SYNTAX").out.find("What to do:"), std::string::npos);
  EXPECT_EQ(ds("manual weather").code, 1);
  for (const char* command : {"check", "export", "render", "query"})
    EXPECT_NE(ds(std::string(command) + " --help").out.find("Exit codes:"), std::string::npos) << command;
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
  EXPECT_NE(artefacts.out.find("satisfies  REQ-00001-2"), std::string::npos);
  EXPECT_NE(artefacts.out.find("verified-by  VER-"), std::string::npos);
  const Outcome shows = ds("query " + examples("artefacts") + " shows CL_Projection");
  EXPECT_NE(shows.out.find("class  iris@projection::Projection"), std::string::npos);
  EXPECT_EQ(ds("query " + examples("artefacts") + " weather").code, 1);
}

// SREQ-00012-1: the language is formal: every oracle corpus means exactly the findings it marks with // !, and
// what is not yet written exactly the coverage it marks with // ? (DS-216)
// SREQ-00012-1: what examples/ shows a newcomer is a design written as it should be: no finding (DS-282)
TEST_F(TestQualification, EveryExampleANewcomerIsShownChecksClean) {
  int examples = 0;
  for (const auto& dir : std::filesystem::directory_iterator(std::string(DRIFTSPEC_EXAMPLES_DIR))) {
    if (!dir.is_directory()) continue;
    ++examples;
    const Outcome checked = ds("check " + dir.path().string());
    EXPECT_EQ(checked.code, 0) << dir.path();
    EXPECT_NE(checked.out.find(" 0 findings"), std::string::npos) << dir.path() << "\n" << checked.out;
  }
  EXPECT_GE(examples, 1);
}

TEST_F(TestQualification, EveryCorpusMeansExactlyTheFindingsItMarks) {
  const std::regex code(R"([EW]-[A-Z][A-Z-]+)");
  int corpora = 0;
  for (const auto& dir : std::filesystem::directory_iterator(std::string(DRIFTSPEC_CORPUS_DIR))) {
    if (!dir.is_directory()) continue;
    ++corpora;
    std::set<std::string> marked, reported, markedCoverage, reportedCoverage;
    for (const auto& f : std::filesystem::recursive_directory_iterator(dir.path())) {
      if (f.path().extension() != ".ds") continue;
      std::ifstream in(f.path());
      for (std::string line; std::getline(in, line);) {
        for (const auto& [sign, into] :
             {std::make_pair(std::string("// !"), &marked), std::make_pair(std::string("// ?"), &markedCoverage)}) {
          const auto mark = line.find(sign);
          if (mark == std::string::npos) continue;
          for (std::sregex_iterator it(line.begin() + static_cast<long>(mark), line.end(), code), end; it != end; ++it)
            into->insert(it->str());
        }
      }
    }
    const Outcome asked = ds("query " + dir.path().string() + " coverage");
    std::istringstream coverage(asked.out);
    for (std::string line; std::getline(coverage, line);) {
      std::smatch m;
      if (std::regex_search(line, m, code) && m.position(0) == 0) reportedCoverage.insert(m.str());
    }
    EXPECT_EQ(reportedCoverage, markedCoverage) << dir.path().filename() << " (coverage)";
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
  const std::string tmp = scratch("ds-perf");
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
  std::filesystem::remove_all(tmp);
}

}  // namespace
