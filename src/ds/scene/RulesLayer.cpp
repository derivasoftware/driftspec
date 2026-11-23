#include <algorithm>
#include <set>

#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

RulesLayer::RulesLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

// The rules and the definitions, each placed in the node it is written in, with what breaks it placed on
// the map: a call lands on the member whose behaviour writes it, an entity on the nearest node that
// stands for it (DS-108).
// where an identity a rule names lands: on the map, or outward through its packages
std::string RulesLayer::place(std::string id) const {
  for (;;) {
    if (const std::string node = map_.land(id); !node.empty()) return node;
    if (!model::Identity(id).isAbsolute()) return std::string();
    id = model::Scope(model::Scope(id).package().text()).box().text();  // a package's own entities: outward through the packages
  }
}

void RulesLayer::decorate(SceneDocument& out_document) {
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected || (e.kind != model::Kind::Rule && e.kind != model::Kind::Definition)) continue;
    RuleView r;
    r.id = e.id;
    r.kind = model::kindName(e.kind);
    r.name = e.name;
    r.title = e.field("title");
    r.scope = place(e.scope.empty() || e.scope.back() != '@' ? e.scope : e.scope.substr(0, e.scope.size() - 1));
    for (const char* key : {"for", "where", "holds", "never", "if"}) {
      std::string text = e.field(key) + "; ";
      for (auto at = text.find("; "); at != std::string::npos; at = text.find("; ")) {  // a clause written twice is kept twice
        if (at) r.lines.push_back({key, text.substr(0, at)});
        text.erase(0, at + 2);
      }
    }
    for (const auto& rel : model_.index().fromOfKind(e.id, "satisfies")) r.requirements.push_back(rel.to);
    const auto broken = model_.breaches().find(e.id);
    if (broken != model_.breaches().end())
      for (const auto& b : broken->second) {
        BreachView v;
        v.node = b.bindings.empty() ? r.scope : place(b.bindings.front().second);
        for (const auto& w : b.witness)
          if (v.to.empty() && model_.find(w.second)) v.to = place(w.second);
        v.bindings = b.bindings;
        v.witness = b.witness;
        v.file = b.location.file;
        v.line = b.location.line;
        r.breaches.push_back(v);
      }
    r.file = e.location.file;
    r.line = e.location.line;
    out_document.rules.push_back(r);
  }
}

}  // namespace scene
}  // namespace ds
