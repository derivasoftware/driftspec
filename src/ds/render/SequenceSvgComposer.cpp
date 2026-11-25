#include "ds/render/SequenceSvgComposer.hpp"

#include <utility>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/SequencePlacement.hpp"
#include "ds/layout/SequenceSpacing.hpp"
#include "ds/render/SequenceLayout.hpp"
#include "ds/render/SequenceProjection.hpp"

namespace ds {
namespace render {

// The sequence projected into the layout's graph and placed by its sequence engine with the SVG's measures (DS-464);
// what each place stands for is read back from the projection, in the same order.
Figure SequenceSvgComposer::compose(const IrDocument& input, int) const {
  Figure figure;
  figure.source = &input;  // drawn as it is: the layout points into it
  const SequenceProjection projection(input);
  SequenceLayout s;
  if (projection.lanes().empty()) {
    figure.sequence = std::move(s);
    return figure;
  }
  const layout::SequenceSpacing spacing = layout::SequenceSpacing::of(metrics_.CHAR_W, metrics_.LINE_H, metrics_.SMALL_W);
  const layout::SequencePlacement placed = layout::EngineFactory::sequence(spacing)->place(projection.graph());
  for (size_t i = 0; i < projection.lanes().size(); ++i) {
    s.laneOf[projection.lanes()[i]->id] = i;
    s.lanes.push_back({projection.lanes()[i], placed.lanes[i].centre, placed.lanes[i].head});
  }
  s.messages = projection.messages();
  for (const layout::RowPlace& r : placed.rows) s.rows.push_back({r.top, r.line, r.bottom});
  for (size_t b = 0; b < placed.blocks.size(); ++b) {
    const IrNode* frame = projection.blocks()[b];
    const layout::BlockPlace& at = placed.blocks[b];
    SequenceFrame f{frame, frame->span.first, frame->span.second, at.top, at.bottom, at.left, at.right, {}};
    for (const layout::DividerPlace& d : at.dividers)
      f.dividers.push_back({frame->dividers[d.index].first, d.y, frame->dividers[d.index].second});
    s.frames.push_back(std::move(f));
  }
  s.width = placed.width;
  s.height = placed.height;
  s.margin = placed.headTop;
  s.headHeight = placed.headHeight;
  s.selfWidth = placed.selfWidth;
  figure.sequence = std::move(s);
  return figure;
}

}  // namespace render
}  // namespace ds
