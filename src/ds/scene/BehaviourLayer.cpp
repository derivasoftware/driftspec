#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "Shared.hpp"
#include "ds/query/Walk.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

BehaviourLayer::BehaviourLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

// every public behaviour, each written once, and the tours that replay them
void BehaviourLayer::decorate(SceneDocument& out_document) {
  index_.clear();  // each build writes its behaviours anew
  calls_.clear();
  for (const auto& r : model_.index().ofKind("calls")) calls_[r.from] = r.to;
  tours(out_document);
}

// the tours a viewer offers: every public behaviour along each of its scenarios
void BehaviourLayer::tours(SceneDocument& out_document) {
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
      const int b = behaviourOf(seq.id, scenario, out_document);
      if (out_document.behaviours[static_cast<size_t>(b)].calls.empty()) continue;
      out_document.tours.push_back({b, operation ? "interface" : "method"});
    }
  }
}

// Writes each behaviour once: a sequence along a scenario, its calls in order, each call naming
// the behaviour it goes on into (found or written the first time it is reached).
int BehaviourLayer::behaviourOf(const std::string& sequence, const std::string& scenario, SceneDocument& out_document) {
  const auto key = std::make_pair(sequence, scenario);
  const auto found = index_.find(key);
  if (found != index_.end()) return found->second;
  const int at = static_cast<int>(out_document.behaviours.size());
  index_[key] = at;  // before its calls: a behaviour that reaches itself names itself
  Behaviour b;
  b.subject = map_.land(model::subjectOf(sequence));
  b.scenario = scenario.empty() ? model::scenarioOf(sequence) : scenario;
  b.id = b.subject + (b.scenario.empty() ? "" : " / " + b.scenario);
  out_document.behaviours.push_back(b);
  std::vector<Call> found_calls;
  for (const query::Taken& t : query::stepsOf(model_, sequence, scenario)) {  // where each call goes on: DS-130
    const model::Entity* m = model_.find(t.message);
    const auto call = calls_.find(m->id);
    if (call == calls_.end()) continue;
    const std::string callee = map_.land(call->second);
    if (callee.empty()) continue;
    std::string args;
    for (const auto& a : m->args) args += (args.empty() ? "" : ", ") + a;
    const std::string op = fieldOf(*m, "op");
    Call c{callee, op.substr(op.rfind("::") == std::string::npos ? 0 : op.rfind("::") + 2) + "(" + args + ")", -1, fieldOf(*m, "result")};
    // a scenario written apart is a sequence of its own; one written as a branch is a path through the base
    if (!t.next.empty()) c.expands = behaviourOf(t.next, t.nextPath, out_document);
    found_calls.push_back(c);
  }
  out_document.behaviours[static_cast<size_t>(at)].calls = found_calls;
  return at;
}

}  // namespace scene
}  // namespace ds
