#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

std::string DiagramCheck::name() const { return "diagrams"; }

std::vector<std::string> DiagramCheck::needs() const {
  // what ContractCheck needed: the expansions behaviour typing made, over the links the linker made, and what
  // escapes each operation
  return {"linker", "behaviour", "failures"};
}

DiagramCheck::DiagramCheck(const Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> DiagramCheck::check() {
  found_.clear();
  checkDiagrams();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// whether a pattern of a diagram names something the workspace declares: an identity, a prefix of one, a
// template written without its arity, the subject of a scenario (DS-212)
bool DiagramCheck::namesSomething(const std::string& written) const {
  const model::Model& model = *model_;
  std::string pattern = written;
  const auto slash = pattern.find(" / ");
  if (slash != std::string::npos) pattern = pattern.substr(0, slash);  // a scenario: its behaviour's subject
  if (pattern.empty()) return true;
  std::string base = pattern;
  if (base.back() == '*') base.pop_back();
  while (!base.empty() && (base.back() == '@' || base.back() == ':')) base.pop_back();
  if (base.empty() || model.find(base)) return true;
  for (const auto& pair : model.entities()) {
    if (pair.second.expected) continue;
    const std::string& id = pair.first;
    if (pattern.back() == '*' && id.rfind(base, 0) == 0) return true;                              // a prefix of something declared
    if (id.size() > base.size() && id.rfind(base, 0) == 0 && id[base.size()] == '<') return true;  // a template
  }
  return false;
}

void DiagramCheck::checkDiagrams() {
  const model::Paths paths(*model_);
  for (const auto& pair : model_->entities()) {
    if (pair.second.kind != model::Kind::Diagram) continue;
    // a sequence diagram that asks for a scenario: it is a behaviour of its own, or a named branch of one
    const auto include = pair.second.fields.find("include");
    if (include != pair.second.fields.end() && pair.second.fields.count("kind") &&
        (pair.second.fields.at("kind") == "sequence" || pair.second.fields.at("kind") == "tour")) {
      for (const auto& pattern : splitTrimmed(include->second, ',')) {
        const auto slash = pattern.find(" / ");
        if (slash == std::string::npos) continue;
        const std::string behaviour = model::Identity(pattern.substr(0, slash)).sequence().text();
        const std::string scenario = pattern.substr(slash + 3);
        if (!model_->find(behaviour) || model_->find(model::Identity(behaviour).withScenario(scenario).text()) ||
            paths.hasScenario(behaviour, scenario))
          continue;
        found_.push_back(context_->metamodel().finding(model::Code::W_SCENARIO_UNDECLARED,
                                                       pair.second.name + " asks for " + pattern + ", a scenario nobody declares",
                                                       pair.second.location));
      }
    }
    // a pattern that names what nothing declares is a broken reference, as in the model (DS-212); a box that
    // exists and is empty is not: a level with no children is valid
    for (const char* key : {"include", "exclude", "open", "close", "unfold", "fold"}) {
      for (const auto& pattern : splitTrimmed(pair.second.field(key), ',')) {
        // a cut may name a kind of box instead of a box (DS-229)
        const bool kind = (key == std::string("open") || key == std::string("close")) &&
                          (pattern == metamodel_->nameOf(model::Kind::Module) || pattern == metamodel_->nameOf(model::Kind::Package));
        if (kind || namesSomething(pattern)) continue;
        found_.push_back(context_->metamodel().finding(
            model::Code::W_DIAGRAM_UNDECLARED, pair.second.name + " " + key + "s " + pattern + ", which the workspace declares nowhere",
            pair.second.location));
      }
    }
    // the diagram a cut is taken from is a reference like any other
    const std::string lender = pair.second.field("cut");
    const model::Entity* lent = lender.empty() ? nullptr : model_->find("diagram:" + lender);
    if (!lender.empty() && (!lent || lent->kind != model::Kind::Diagram)) {
      found_.push_back(context_->metamodel().finding(
          model::Code::W_DIAGRAM_UNDECLARED,
          pair.second.name + " takes the cut of " + lender + ", a diagram the workspace declares nowhere", pair.second.location));
    }
    // a number field too large for an int is no number either, and a finding where it used to abort (DS-337);
    // neighbours: is read as depth: is, with no * of its own, and each has a code of its own (DS-362)
    for (const char* key : {"depth", "neighbours"}) {
      const bool depth = key == std::string("depth");
      const auto written = pair.second.fields.find(key);
      int read = 0;
      if (written == pair.second.fields.end() || (depth && written->second == "*") || pair.second.number(key, read)) continue;
      found_.push_back(context_->metamodel().finding(depth ? model::Code::W_DIAGRAM_DEPTH : model::Code::W_DIAGRAM_NEIGHBOURS,
                                                     pair.second.name + " asks for " + key + " '" + written->second +
                                                         "': " + (depth ? "a number, or * for every level" : "a number of steps"),
                                                     pair.second.location));
    }
  }
}

}  // namespace check
}  // namespace ds
