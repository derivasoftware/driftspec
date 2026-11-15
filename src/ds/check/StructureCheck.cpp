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
  checkCycles("refines", "E-REFINES-CYCLE", "refines");
  checkCycles("inherits", "E-INHERITANCE-CYCLE", "derives from");
  checkRefinesUpward();
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

// A requirement that refines itself through others, a class that derives from itself: a chain of the relation
// that comes back to where it starts is a contradiction between written things (DS-214). Reported once per cycle.
void StructureCheck::checkCycles(const std::string& relation, const std::string& code, const std::string& verb) {
  std::map<std::string, std::vector<std::pair<std::string, model::Location>>> next;
  for (const auto& r : model_->index().ofKind(relation)) {
    const model::Entity* to = model_->find(r.to);
    if (!to || to->expected || r.from == r.to) continue;
    next[r.from].push_back({r.to, r.location});
  }
  std::set<std::string> reported, done, onPath;
  std::vector<std::string> path;
  std::function<void(const std::string&)> walk = [&](const std::string& node) {
    path.push_back(node);
    onPath.insert(node);
    for (const auto& edge : next[node]) {
      if (onPath.count(edge.first)) {
        std::vector<std::string> cycle(std::find(path.begin(), path.end(), edge.first), path.end());
        std::vector<std::string> key = cycle;
        std::sort(key.begin(), key.end());
        if (reported.insert(join(key, "|")).second)
          found_.push_back({code, "error", "a cycle: " + join(cycle, " " + verb + " ") + " " + verb + " " + edge.first, edge.second});
      } else if (!done.count(edge.first)) {
        walk(edge.first);
      }
    }
    onPath.erase(node);
    path.pop_back();
    done.insert(node);
  };
  for (const auto& pair : next)
    if (!done.count(pair.first)) walk(pair.first);
}

// A requirement refines one written further down the containment axis: the box of the refined requirement lies
// inside the box of the one that refines it, the detail above what it details (DS-214).
void StructureCheck::checkRefinesUpward() {
  const auto inside = [](const std::string& inner, const std::string& outer) {
    if (inner.size() <= outer.size() || inner.compare(0, outer.size(), outer) != 0) return false;
    const char after = inner[outer.size()];
    return outer.back() == '@' || after == '@' || after == ':';
  };
  for (const auto& r : model_->index().ofKind("refines")) {
    const model::Entity* child = model_->find(r.from);
    const model::Entity* parent = model_->find(r.to);
    if (!child || !parent || parent->expected || child->scope.empty() || parent->scope.empty()) continue;
    if (!inside(parent->scope, child->scope)) continue;
    found_.push_back({"W-REFINES-UPWARD", "warning",
                      child->id + " (in " + model::Scope(child->scope).box().text() + ") refines " + parent->id +
                          ", which is written inside it (in " + model::Scope(parent->scope).box().text() +
                          "): the detail stands above what it details",
                      r.location});
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
      if (const model::FieldSpec* spec = model::fieldOf(e.kind, f.first)) {
        // a closed field takes one of its values; an empty one is left to the field's own check (DS-211)
        if (!spec->values.empty() && !f.second.empty() &&
            std::find(spec->values.begin(), spec->values.end(), f.second) == spec->values.end())
          found_.push_back({"W-FIELD-VALUE", "warning",
                            std::string(model::kindName(e.kind)) + " '" + e.id + "' has " + f.first + ": " + f.second +
                                ", which is none of " + join(spec->values, ", "),
                            e.location});
        continue;
      }
      found_.push_back({"W-FIELD-UNKNOWN", "warning",
                        std::string(model::kindName(e.kind)) + " '" + e.id + "' has a field '" + f.first + "' that a " +
                            model::kindName(e.kind) + " does not declare",
                        e.location});
    }
  }
}

}  // namespace check
}  // namespace ds
