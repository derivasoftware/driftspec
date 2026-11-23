#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

using ds::model::Kind;

/// The metamodel states what the language does (REQ-00066-1, DS-121): where each kind may be written is
/// where the grammar lets it be written, and every flag the binder sets is one its kind declares. The
/// export publishes the metamodel, so a reader that trusts it is not misled.
class TestMetamodelTruth : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  // each rule of the grammar and the rules it names: `name: $ => ... $.child ...`
  static std::map<std::string, std::set<std::string>> grammar() {
    std::ifstream in(std::string(DRIFTSPEC_SOURCE_DIR) + "/grammar/grammar.js");
    std::stringstream s;
    s << in.rdbuf();
    const std::string text = s.str();
    const std::regex head("\\n    (_?[a-z_]+): \\$ =>");
    std::vector<std::pair<std::string, std::size_t>> starts;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), head); it != std::sregex_iterator(); ++it)
      starts.push_back({(*it)[1], static_cast<std::size_t>(it->position())});
    std::map<std::string, std::set<std::string>> rules;
    const std::regex child("\\$\\.(_?[a-z_]+)");
    for (std::size_t i = 0; i < starts.size(); ++i) {
      const std::size_t end = i + 1 < starts.size() ? starts[i + 1].second : text.size();
      const std::string body = text.substr(starts[i].second, end - starts[i].second);
      for (auto it = std::sregex_iterator(body.begin() + 1, body.end(), child); it != std::sregex_iterator(); ++it)
        rules[starts[i].first].insert((*it)[1]);
    }
    return rules;
  }

  // hidden item lists and bodies are not where a thing is declared: what holds them is
  static bool through(const std::string& rule) {
    return rule[0] == '_' || (rule.size() > 5 && rule.compare(rule.size() - 5, 5, "_body") == 0);
  }

  // a loop, an alt, an opt, an attempt and its handlers are written by their own rules and are each a
  // frame of the model (DS-172, DS-187)
  static std::string kindOfRule(const std::string& rule) {
    return rule == "alt" || rule == "opt" || rule == "loop" || rule == "attempt" || rule == "handler" ? "frame" : rule;
  }

  // the rules a kind is written in, seen through the lists, bodies and frames between them
  static std::set<std::string> holders(const std::map<std::string, std::set<std::string>>& rules, const std::string& name,
                                       std::set<std::string> seen = {}) {
    std::set<std::string> out;
    if (!seen.insert(name).second) return out;
    for (const auto& r : rules) {
      if (!r.second.count(name)) continue;
      if (!through(r.first)) {
        out.insert(kindOfRule(r.first));
        continue;
      }
      for (const auto& h : holders(rules, r.first, seen)) out.insert(h);
    }
    return out;
  }
};

TEST_F(TestMetamodelTruth, EveryKindIsDeclaredInWhatTheGrammarLetsItBeWrittenIn) {
  const auto rules = grammar();
  for (const auto& spec : metamodel.kinds()) {
    const std::string name = metamodel.nameOf(spec.kind);
    if (!rules.count(name)) continue;  // laid out by a behaviour (message, frame): no rule of its own
    std::set<std::string> parents, top;
    for (const auto& h : holders(rules, name)) {
      if (h == "source_file") {
        top.insert(h);
        continue;
      }
      parents.insert(h);
    }
    std::set<std::string> declared;
    for (Kind p : spec.parents) declared.insert(metamodel.nameOf(p));
    EXPECT_EQ(declared, parents) << name << ": the metamodel's parents against the grammar";
    EXPECT_EQ(spec.top, !top.empty()) << name << ": at the top of a file";
  }
}

TEST_F(TestMetamodelTruth, EveryFlagTheBinderSetsIsOneItsKindDeclares) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  for (const char* dir : {"/examples", "/design"})
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + dir)) binder.bind(f);
  // what the corpus does not exercise: a verification and a constraint extended by their annotations
  binder.bind(parser.parseText("x.ds",
                               "package x {\n  verification VER-09001-1 {\n    @requirement REQ-09001-1\n  }\n"
                               "  constraint CST-09001-1 {\n    @requirement REQ-09001-1\n  }\n}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder);
  std::size_t flagged = 0;
  for (const auto& pair : model.entities()) {
    const auto& e = pair.second;
    const auto& allowed = metamodel.specOf(e.kind).flags;
    for (const auto& flag : e.flags) {
      ++flagged;
      EXPECT_NE(std::find(allowed.begin(), allowed.end(), flag), allowed.end())
          << metamodel.nameOf(e.kind) << " " << e.id << " carries the flag " << flag << ", which a " << metamodel.nameOf(e.kind)
          << " does not declare";
    }
  }
  EXPECT_GT(flagged, 50u);  // the corpus exercises the flags
}

}  // namespace
