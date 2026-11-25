#include "ds/model/Walkthroughs.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace ds {
namespace model {

std::vector<Walkthrough> Walkthroughs::of(const Model& model) {
  std::vector<Walkthrough> out;
  for (const auto& pair : model.entities()) {  // a std::map: by identity
    const Entity& d = pair.second;
    if (d.kind != Kind::Diagram || d.field("kind") != "walkthrough") continue;
    Walkthrough w{d.id, d.name, d.field("title"), {}};
    for (const auto& c : model.index().fromOfKind(d.id, "contains")) {
      const Entity* s = model.find(c.to);
      if (!s || s->kind != Kind::Step) continue;
      WalkStep step;
      step.id = s->id;
      step.n = std::atoi(s->id.substr(s->id.rfind("::") + 2).c_str());
      step.title = s->name;
      step.show = s->field("show");
      step.scenario = s->field("scenario");
      step.text = s->field("text");
      for (const auto& r : model.index().fromOfKind(s->id, "shows"))
        if (model.find(r.to)) step.diagram = r.to;
      for (const auto& r : model.index().fromOfKind(s->id, "focuses"))
        if (model.find(r.to)) step.focus.push_back(r.to);
      for (const auto& r : model.index().fromOfKind(s->id, "highlights"))
        if (model.find(r.to)) step.highlight.push_back(r.to);
      for (const auto& r : model.index().fromOfKind(s->id, "points-at"))
        if (model.find(r.to)) step.at = r.to;
      w.steps.push_back(step);
    }
    std::sort(w.steps.begin(), w.steps.end(), [](const WalkStep& a, const WalkStep& b) { return a.n < b.n; });
    out.push_back(w);
  }
  return out;
}

}  // namespace model
}  // namespace ds
