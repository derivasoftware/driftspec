#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "Shared.hpp"
#include "ds/query/Walk.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

namespace {

/// Writes each behaviour once: a sequence along a scenario, its calls in order, each call naming
/// the behaviour it goes on into (found or written the first time it is reached).
struct Behaviours {
  const model::Model& model;
  const Map& map;
  std::map<std::string, std::string> calls;  // message -> the operation it calls
  std::map<std::pair<std::string, std::string>, int> index;
  std::vector<Behaviour>& out;

  int of(const std::string& sequence, const std::string& scenario) {
    const auto key = std::make_pair(sequence, scenario);
    const auto found = index.find(key);
    if (found != index.end()) return found->second;
    const int at = static_cast<int>(out.size());
    index[key] = at;  // before its calls: a behaviour that reaches itself names itself
    Behaviour b;
    b.subject = map.land(model::subjectOf(sequence));
    b.scenario = scenario.empty() ? model::scenarioOf(sequence) : scenario;
    b.id = b.subject + (b.scenario.empty() ? "" : " / " + b.scenario);
    out.push_back(b);
    std::vector<Call> found_calls;
    for (const query::Taken& t : query::stepsOf(model, sequence, scenario)) {  // where each call goes on: DS-130
      const model::Entity* m = model.find(t.message);
      const auto call = calls.find(m->id);
      if (call == calls.end()) continue;
      const std::string callee = map.land(call->second);
      if (callee.empty()) continue;
      std::string args;
      for (const auto& a : m->args) args += (args.empty() ? "" : ", ") + a;
      const std::string op = fieldOf(*m, "op");
      Call c{callee, op.substr(op.rfind("::") == std::string::npos ? 0 : op.rfind("::") + 2) + "(" + args + ")", -1, fieldOf(*m, "result")};
      // a scenario written apart is a sequence of its own; one written as a branch is a path through the base
      if (!t.next.empty()) c.expands = of(t.next, t.nextPath);
      found_calls.push_back(c);
    }
    out[static_cast<size_t>(at)].calls = found_calls;
    return at;
  }
};

}  // namespace

BehaviourLayer::BehaviourLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

// every public behaviour, each written once, and the tours that replay them
void BehaviourLayer::decorate(SceneDocument& out_document) {
  Behaviours behaviours{model_, map_, {}, {}, out_document.behaviours};
  for (const auto& r : model_.relations()) {
    if (r.kind == "calls") behaviours.calls[r.from] = r.to;
  }
  const auto visible = [](const model::Entity* e) { return e && fieldOf(*e, "visibility") != "-"; };
  for (const auto& pair : model_.entities()) {
    const model::Entity& seq = pair.second;
    if (seq.kind != model::Kind::Sequence) continue;
    const std::string subjectId = model::subjectOf(seq.id);
    if (subjectId.empty()) continue;
    const model::Entity* subject = model_.find(subjectId);
    if (!subject) continue;
    const bool operation = subject->kind == model::Kind::Operation;
    const bool member = subject->kind == model::Kind::Method && visible(subject) && visible(model_.find(subject->scope));
    const bool function = subject->kind == model::Kind::Function && visible(subject);
    if (!operation && !member && !function) continue;  // a private behaviour is replayed inside the public one that reaches it
    if (map_.land(subject->id).empty()) continue;
    // its scenarios: the default path and every branch it names, or the one it is written apart for
    std::vector<std::string> scenarios{""};
    if (model::scenarioOf(seq.id).empty()) {  // the default behaviour, not one written apart for a scenario
      std::set<std::string> named;
      for (const auto& f : model_.entities()) {
        if (f.second.kind != model::Kind::Frame || f.second.scope != seq.id) continue;
        for (const char* key : {"scenario", "else-scenario"}) {
          const std::string s = fieldOf(f.second, key);
          if (!s.empty() && named.insert(s).second) scenarios.push_back(s);
        }
      }
    }
    for (const auto& scenario : scenarios) {
      const int b = behaviours.of(seq.id, scenario);
      if (out_document.behaviours[static_cast<size_t>(b)].calls.empty()) continue;
      out_document.tours.push_back({b, operation ? "interface" : "method"});
    }
  }
}

}  // namespace scene
}  // namespace ds
