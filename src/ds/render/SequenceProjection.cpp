#include "ds/render/SequenceProjection.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace ds {
namespace render {

SequenceProjection::SequenceProjection(const IrDocument& doc) {
  std::map<std::string, size_t>& laneOf = laneOf_;
  for (const auto& n : doc.nodes) {
    if (n.kind != "lifeline") continue;
    laneOf[n.id] = lanes_.size();
    lanes_.push_back(&n);
    graph_.lanes.push_back({n.label.size(), n.classifier == "actor"});
  }
  for (const auto& e : doc.edges) {
    if (e.kind == "message" && laneOf.count(e.from) && laneOf.count(e.to)) messages_.push_back(&e);
  }
  std::sort(messages_.begin(), messages_.end(), [](const IrEdge* a, const IrEdge* b) { return a->order < b->order; });
  graph_.messages.reserve(messages_.size());
  for (const IrEdge* m : messages_) graph_.messages.push_back({laneOf.at(m->from), laneOf.at(m->to), m->label.size(), m->order});
  for (const auto& n : doc.nodes) {
    if (n.kind != "frame" || n.span.first < 0) continue;
    blocks_.push_back(&n);
    layout::SequenceBlock b{n.span.first, n.span.second, n.classifier.size(), n.label.size(), {}};
    for (const auto& d : n.dividers) b.dividers.push_back(d.first);
    graph_.blocks.push_back(b);
  }
}

}  // namespace render
}  // namespace ds
