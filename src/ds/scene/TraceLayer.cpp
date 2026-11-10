#include <algorithm>
#include <map>
#include <set>

#include "Shared.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

TraceLayer::TraceLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

// requirements, constraints and use cases, what answers to them and what proves them
void TraceLayer::decorate(SceneDocument& out_document) {
  std::map<std::string, size_t> at;
  std::map<std::string, size_t> verificationAt;
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    if (e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint || e.kind == model::Kind::UseCase) {
      Requirement r;
      r.id = e.id;
      r.kind = e.kind == model::Kind::Requirement ? "requirement" : e.kind == model::Kind::Constraint ? "constraint" : "usecase";
      if (e.kind == model::Kind::UseCase) {  // its contract and its outcomes (DS-77)
        const std::string operation = fieldOf(e, "operation");
        if (!operation.empty()) {
          r.signature = operation + "(";
          for (size_t i = 0; i < e.params.size(); ++i)
            r.signature += (i ? ", " : "") + e.params[i].name + (e.params[i].type.empty() ? "" : ": " + e.params[i].type);
          r.signature += ")";
          const std::string returns = fieldOf(e, "returns");
          if (!returns.empty()) r.signature += " -> " + returns;
        }
        for (const auto& other : model_.entities())
          if (other.second.kind == model::Kind::Outcome && other.second.scope == e.id)
            r.outcomes.push_back({other.second.name, fieldOf(other.second, "returns"), fieldOf(other.second, "text")});
        std::stable_sort(r.outcomes.begin(), r.outcomes.end(),
                         [](const Outcome& a, const Outcome& b) { return a.name == "default" && b.name != "default"; });
      }
      r.title = fieldOf(e, "title");
      r.status = fieldOf(e, "status");
      r.text = fieldOf(e, "text");
      if (!e.scope.empty()) r.scope = map_.land(model::boxOf(e.scope));
      at[r.id] = out_document.requirements.size();
      out_document.requirements.push_back(r);
    } else if (e.kind == model::Kind::Verification) {
      Verification v;
      v.id = e.id;
      v.title = fieldOf(e, "title");
      v.status = fieldOf(e, "status");
      v.method = fieldOf(e, "method");
      std::string test;
      for (const char c : fieldOf(e, "tests") + ",") {  // the tests are written one per line or separated by commas
        if (c == ',' || c == '\n') {
          test.erase(0, test.find_first_not_of(" \t"));
          test.erase(test.find_last_not_of(" \t") + 1);
          if (!test.empty()) v.tests.push_back(test);
          test.clear();
        } else {
          test += c;
        }
      }
      verificationAt[v.id] = out_document.verifications.size();
      out_document.verifications.push_back(v);
    }
  }
  for (const auto& r : model_.relations()) {
    if ((r.kind == "refines" || r.kind == "inherits") && at.count(r.from)) {  // or the use case it specialises
      out_document.requirements[at[r.from]].parent = r.to;
    } else if (r.kind == "requires" && at.count(r.from) && out_document.requirements[at[r.from]].kind == "usecase") {
      out_document.requirements[at[r.from]].binds.push_back(r.to);
    } else if (r.kind == "realises" && at.count(r.to)) {  // an interface operation realises a use case
      const std::string node = map_.land(r.from);
      if (!node.empty()) out_document.requirements[at[r.to]].nodes.push_back(node);
    } else if (r.kind == "verifies" && verificationAt.count(r.from)) {
      out_document.verifications[verificationAt[r.from]].requirements.push_back(r.to);
    } else if ((r.kind == "requires" || r.kind == "constrained-by") && at.count(r.to)) {
      const std::string node = map_.land(r.from);
      auto& nodes = out_document.requirements[at[r.to]].nodes;
      if (!node.empty() && std::find(nodes.begin(), nodes.end(), node) == nodes.end()) nodes.push_back(node);
    }
  }
}

}  // namespace scene
}  // namespace ds
