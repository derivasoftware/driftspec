#include <string>
#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/Walkthroughs.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

WalkthroughLayer::WalkthroughLayer(const model::Model& model, const Map& /*map*/) : model_(model) {}

void WalkthroughLayer::decorate(SceneDocument& out_document) {
  for (const auto& w : model::Walkthroughs::of(model_)) {
    WalkView view{w.name, w.title, {}};
    for (const auto& s : w.steps) {
      WalkStepView step;
      step.title = s.title;
      step.diagram = s.diagram.empty() ? s.show : model::Identity(s.diagram).diagramName();
      // a step that shows a sequence replays its behaviour, along the scenario it names
      const model::Entity* shown = s.diagram.empty() ? nullptr : model_.find(s.diagram);
      if (shown && shown->field("kind") == "sequence" && !shown->field("include").empty())
        step.behaviour = model::Identity(shown->field("include")).sequence(s.scenario).text();
      // what it lights by identity, as its drawing names it (data-id), not landed on the map: a requirement is no
      // node of the map and is lit all the same (DS-470)
      step.focus = s.focus;
      step.highlight = s.highlight;
      step.at = s.at;
      step.text = s.text;
      view.steps.push_back(step);
    }
    out_document.walkthroughs.push_back(view);
  }
}

}  // namespace scene
}  // namespace ds
