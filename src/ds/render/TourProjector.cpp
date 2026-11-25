#include "ds/render/TourProjector.hpp"

#include "ds/query/Filter.hpp"
#include "ds/render/ProjectorFactory.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::join;
using text::splitTrimmed;

IrDocument TourProjector::project(const model::Entity& tour) const {
  // the sequence, as a sequence diagram of the same fields draws it
  const IrDocument sequence = ProjectorFactory::of("sequence", model_, metamodel_)->project(tour);
  // who takes part, drawn as boxes on the same cut: each lifeline is one box, with the relations between them
  const std::set<std::string> who =
      query::SequenceFilter(metamodel_, splitTrimmed(tour.field("include"), ','), query::SequenceFilter::levelsOf(tour), tour.id)
          .select(model_);
  model::Entity boxes = tour;
  boxes.fields["kind"] = "class";
  std::vector<std::string> taken, closed;
  for (const auto& id : who) {
    taken.push_back(id);
    const model::Entity* e = model_.find(id);
    if (!e || (e->kind != model::Kind::Package && e->kind != model::Kind::Module)) continue;
    // a box takes part as one box, with everything it holds: what it holds is not drawn, and its relations are
    // lifted to it. That is so whether the cut closes it or a scenario written between boxes names it
    closed.push_back(id);
    if (e->kind == model::Kind::Package) taken.push_back(id + "@*");
  }
  boxes.fields["include"] = join(taken, ", ");
  boxes.fields.erase("open");
  boxes.fields.erase("cut");
  boxes.fields.erase("close");
  if (!closed.empty()) boxes.fields["close"] = join(closed, ", ");
  if (!tour.fields.count("hide") && !tour.fields.count("show")) boxes.fields["hide"] = "fields, methods";
  const IrDocument drawn = ProjectorFactory::of("class", model_, metamodel_)->project(boxes);
  // one document: its parts named in the order they are drawn, each node and edge saying which one it is of
  IrDocument out;
  out.parts = {"boxes", "sequence"};
  for (const auto& part : {std::make_pair("boxes", &drawn), std::make_pair("sequence", &sequence)}) {
    for (IrNode node : part.second->nodes) {
      node.part = part.first;
      out.nodes.push_back(node);
    }
  }
  for (const auto& part : {std::make_pair("boxes", &drawn), std::make_pair("sequence", &sequence)}) {
    for (IrEdge edge : part.second->edges) {
      edge.part = part.first;
      out.edges.push_back(edge);
    }
  }
  out.notice = sequence.notice;
  return out;
}

}  // namespace render
}  // namespace ds
