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
  // the finished model: every field and relation the enriching steps write
  return {"linker", "behaviour", "derivation"};
}

std::vector<model::Finding> StructureCheck::check(const Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  found_.clear();
  checkOwnershipCycles();
  checkArtefacts();
  checkExtensions();
  checkFields();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// A owns B and B owns A: nobody can create either (R16). Self-composition (a tree) is fine.
void StructureCheck::checkOwnershipCycles() {
  struct Edge {
    std::string to;
    model::Location location;
  };
  std::map<std::string, std::vector<Edge>> owns;
  for (const auto& r : model_->index().ofKind("owns")) {
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
          found_.push_back(
              {"E-OWNERSHIP-CYCLE", "error", "composition cycle: " + join(cycle, " owns ") + " owns " + edge.to, edge.location});
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
  for (const auto& r : model_->index().ofKind("verifies")) verified.insert(r.to);
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    const auto status = e.fields.find("status");
    if (e.kind == model::Kind::Requirement && status != e.fields.end() && status->second == "approved" && !verified.count(e.id)) {
      found_.push_back({"W-REQ-NO-VERIFICATION", "warning", e.id + " is approved and nothing verifies it", e.location});
    }
    const auto method = e.fields.find("method");
    if (e.kind == model::Kind::Verification && method != e.fields.end() && method->second == "test") {
      const auto tests = e.fields.find("tests");
      if (tests == e.fields.end() || tests->second.empty())
        found_.push_back({"W-VER-NO-TEST", "warning", e.id + " verifies by test but names no tests", e.location});
    }
  }
}

// approved requirements are verified; a test verification names its tests
void StructureCheck::checkExtensions() {
  std::set<std::string> said;  // the linker may have said it already; a check only reads what it said
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected && e.has("extension") && !context_->reported().count(e.id) && said.insert(e.id).second) {
      found_.push_back(
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
      found_.push_back({"W-FIELD-UNKNOWN", "warning",
                        std::string(model::kindName(e.kind)) + " '" + e.id + "' has a field '" + f.first + "' that a " +
                            model::kindName(e.kind) + " does not declare",
                        e.location});
    }
  }
}

}  // namespace check
}  // namespace ds
