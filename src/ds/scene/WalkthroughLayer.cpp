#include <string>
#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/Walkthroughs.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

WalkthroughLayer::WalkthroughLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

void WalkthroughLayer::decorate(SceneDocument& out_document) {
  const auto landed = [this](const std::vector<std::string>& ids) {
    std::vector<std::string> out;
    for (const auto& id : ids) {
      const std::string node = map_.land(id);
      if (!node.empty()) out.push_back(node);
    }
    return out;
  };
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
      step.focus = landed(s.focus);
      step.highlight = landed(s.highlight);
      step.at = s.at.empty() ? "" : map_.land(s.at);
      step.text = s.text;
      view.steps.push_back(step);
    }
    out_document.walkthroughs.push_back(view);
  }
}

}  // namespace scene
}  // namespace ds
