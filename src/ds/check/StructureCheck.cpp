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
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

using model::outer;

std::string StructureCheck::name() const { return "structure"; }

std::vector<std::string> StructureCheck::needs() const {
  // today's order, kept so the output stays the same; relaxed when the derivations move (DS-138)
  return {"derivation"};
}

void StructureCheck::run(Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  checkOwnershipCycles();
  checkArtefacts();
  checkExtensions();
  checkFields();
  checkLanguages();
}

// A owns B and B owns A: nobody can create either (R16). Self-composition (a tree) is fine.
void StructureCheck::checkOwnershipCycles() {
  struct Edge {
    std::string to;
    model::Location location;
  };
  std::map<std::string, std::vector<Edge>> owns;
  for (const auto& r : model_->relations()) {
    if (r.kind != "owns") continue;
    const model::Entity* attribute = model_->find(r.from);
    const model::Entity* part = model_->find(r.to);
    if (!attribute || !part || part->expected) continue;
    const std::string whole = attribute->scope;
    if (whole.empty() || whole == r.to) continue;
    owns[whole].push_back({r.to, r.location});
  }
  std::set<std::string> reportedCycles;
  std::vector<std::string> path;
  std::set<std::string> onPath;
  std::set<std::string> done;
  std::function<void(const std::string&)> walk = [&](const std::string& node) {
    path.push_back(node);
    onPath.insert(node);
    for (const auto& edge : owns[node]) {
      if (onPath.count(edge.to)) {
        std::vector<std::string> cycle(std::find(path.begin(), path.end(), edge.to), path.end());
        std::vector<std::string> key = cycle;
        std::sort(key.begin(), key.end());
        if (reportedCycles.insert(join(key, "|")).second) {
          model_->report({"E-OWNERSHIP-CYCLE", "error", "composition cycle: " + join(cycle, " owns ") + " owns " + edge.to, edge.location});
        }
      } else if (!done.count(edge.to)) {
        walk(edge.to);
      }
    }
    onPath.erase(node);
    path.pop_back();
    done.insert(node);
  };
  for (const auto& pair : owns) {
    if (!done.count(pair.first)) walk(pair.first);
  }
}

void StructureCheck::checkArtefacts() {
  std::set<std::string> verified;
  for (const auto& r : model_->relations()) {
    if (r.kind == "verifies") verified.insert(r.to);
  }
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    const auto status = e.fields.find("status");
    if (e.kind == model::Kind::Requirement && status != e.fields.end() && status->second == "approved" && !verified.count(e.id)) {
      model_->report({"W-REQ-NO-VERIFICATION", "warning", e.id + " is approved and nothing verifies it", e.location});
    }
    const auto method = e.fields.find("method");
    if (e.kind == model::Kind::Verification && method != e.fields.end() && method->second == "test") {
      const auto tests = e.fields.find("tests");
      if (tests == e.fields.end() || tests->second.empty())
        model_->report({"W-VER-NO-TEST", "warning", e.id + " verifies by test but names no tests", e.location});
    }
  }
}

// approved requirements are verified; a test verification names its tests
void StructureCheck::checkExtensions() {
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected && e.has("extension") && context_->reported().insert(e.id).second) {
      model_->report(
          {"W-REF-UNDECLARED", "warning",
           std::string(model::kindName(e.kind)) + " '" + e.id + "' is extended with annotations but defined nowhere in the workspace",
           e.location});
    }
  }
}

// a field its kind does not declare in the metamodel is a typo or a field the language does not know
void StructureCheck::checkFields() {
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    for (const auto& f : e.fields) {
      if (model::fieldOf(e.kind, f.first)) continue;
      model_->report({"W-FIELD-UNKNOWN", "warning",
                      std::string(model::kindName(e.kind)) + " '" + e.id + "' has a field '" + f.first + "' that a " +
                          model::kindName(e.kind) + " does not declare",
                      e.location});
    }
  }
}

// A package or a module says what its code is written in, or holds the language of the box around
// it: the field is filled down, so a reader of the model finds it on every package and module. A
// language ds does not know, or two at once (two declarations of a reopened box), is a finding.
void StructureCheck::checkLanguages() {
  static const std::set<std::string> kKnown{"cpp", "c", "python", "typescript", "lua"};
  std::map<std::string, std::string> parent;
  for (const auto& r : model_->relations()) {
    const model::Entity* from = model_->find(r.from);
    const model::Entity* to = model_->find(r.to);
    if (r.kind == "contains" && from && to && (to->kind == model::Kind::Package || to->kind == model::Kind::Module)) parent[r.to] = r.from;
  }
  std::vector<std::string> boxes;
  for (const auto& pair : model_->entities())
    if (pair.second.kind == model::Kind::Package || pair.second.kind == model::Kind::Module) boxes.push_back(pair.first);
  std::map<std::string, std::string> own;
  for (const auto& id : boxes) {
    model::Entity& e = *model_->find(id);
    const auto written = e.fields.find("language");
    if (written == e.fields.end() || written->second.empty()) continue;
    std::set<std::string> said;
    for (const auto& item : split(written->second, ',')) said.insert(item);
    const std::string first = *said.begin();
    if (said.size() > 1) {
      model_->report(
          {"W-LANGUAGE-UNKNOWN", "warning", e.id + " says two languages (" + written->second + "): one box, one language", e.location});
    } else if (!kKnown.count(first)) {
      model_->report({"W-LANGUAGE-UNKNOWN", "warning",
                      e.id + " is written in '" + first + "', which is none of cpp, c, python, typescript, lua", e.location});
    }
    own[e.id] = first;
  }
  for (const auto& id : boxes) {
    model::Entity& e = *model_->find(id);
    std::set<std::string> seen;
    for (std::string at = e.id; !at.empty() && seen.insert(at).second; at = parent.count(at) ? parent.at(at) : "") {
      const auto it = own.find(at);
      if (it == own.end()) continue;
      e.fields["language"] = it->second;
      break;
    }
  }
}

}  // namespace check
}  // namespace ds
