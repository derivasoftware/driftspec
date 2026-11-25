#include "ds/render/SequenceTextComposer.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/render/SeqLine.hpp"
#include "ds/render/SequenceLayout.hpp"

namespace ds {
namespace render {

Figure SequenceTextComposer::compose(const IrDocument& input, int) const {
  Figure figure;
  figure.document = input;
  const IrDocument& doc = figure.document;  // what the layout points into
  std::vector<SeqLine> lines;
  std::map<std::string, size_t> index;
  std::vector<const IrNode*> nodeOf;  // each line's lifeline
  for (const auto& n : doc.nodes) {
    if (n.kind != "lifeline") continue;
    index[n.id] = lines.size();
    lines.push_back({n.id, n.label, 0, static_cast<int>(n.label.size()) + 4});
    nodeOf.push_back(&n);
  }
  if (lines.empty()) {
    figure.sequence = SequenceLayout();
    return figure;
  }
  std::vector<const IrEdge*> messages;
  for (const auto& e : doc.edges) {
    if (e.kind == "message" && index.count(e.from) && index.count(e.to)) messages.push_back(&e);
  }
  std::sort(messages.begin(), messages.end(), [](const IrEdge* a, const IrEdge* b) { return a->order < b->order; });
  // lanes: wide enough for the heads and for the longest label that crosses each gap
  std::vector<int> gap(lines.size(), 0);
  for (size_t i = 0; i + 1 < lines.size(); ++i) gap[i] = (lines[i].width + lines[i + 1].width) / 2 + 2;
  for (const IrEdge* m : messages) {
    const size_t a = std::min(index[m->from], index[m->to]), b = std::max(index[m->from], index[m->to]);
    const int need = static_cast<int>(m->label.size()) + 6;
    if (a == b) {
      if (a + 1 < lines.size()) gap[a] = std::max(gap[a], need + lines[a].width / 2);
      continue;
    }
    int have = 0;
    for (size_t i = a; i < b; ++i) have += gap[i];
    if (have < need) gap[b - 1] += need - have;
  }
  int x = lines[0].width / 2 + 1;
  for (size_t i = 0; i < lines.size(); ++i) {
    lines[i].centre = x;
    if (i + 1 < lines.size()) x += gap[i];
  }
  const int width = x + lines.back().width / 2 + 24;
  // frames, by the row range they cover
  struct FrameAt {
    int first, last;
    std::string label;
    std::vector<std::pair<int, std::string>> dividers;
    const IrNode* node = nullptr;
  };
  std::vector<FrameAt> frames;
  for (const auto& n : doc.nodes) {
    if (n.kind != "frame") continue;
    FrameAt f{n.span.first, n.span.second, n.label, {}, &n};
    for (const auto& d : n.dividers) f.dividers.push_back(d);
    frames.push_back(f);
  }
  // rows: 3 for the heads, then per message 2 rows (label, arrow), plus frame rows
  const size_t count = messages.empty() ? 0 : static_cast<size_t>(messages.back()->order) + 1;
  std::vector<int> rowOf(count, 0), heightOf(count, 2);
  int rows = 4;
  std::vector<int> opens(count, 0), closes(count, 0), divs(count, 0);
  std::vector<int> depthOf(frames.size(), 0);  // how many frames enclose each frame
  for (size_t i = 0; i < frames.size(); ++i) {
    for (size_t j = 0; j < frames.size(); ++j) {
      if (i != j && frames[j].first <= frames[i].first && frames[i].last <= frames[j].last &&
          (frames[j].first < frames[i].first || frames[i].last < frames[j].last)) {
        ++depthOf[i];
      }
    }
    if (frames[i].first < static_cast<int>(count)) ++opens[frames[i].first];
    if (frames[i].last < static_cast<int>(count)) ++closes[frames[i].last];
    for (const auto& d : frames[i].dividers) {
      if (d.first < static_cast<int>(count)) ++divs[d.first];
    }
  }
  for (const IrEdge* m : messages) {
    rows += opens[m->order] + divs[m->order];
    rowOf[m->order] = rows;
    heightOf[m->order] = m->from == m->to ? 3 : 2;
    rows += heightOf[m->order] + closes[m->order];
  }
  rows += 1;
  // what was placed, kept for the drawing
  SequenceLayout s;
  for (size_t i = 0; i < lines.size(); ++i) s.lanes.push_back({nodeOf[i], lines[i].centre, lines[i].width});
  for (size_t r = 0; r < count; ++r) s.rows.push_back({rowOf[r], rowOf[r] + 1, rowOf[r] + heightOf[r]});
  // frames: a box from the row above the first message to the row below the last
  for (size_t fi = 0; fi < frames.size(); ++fi) {
    const FrameAt& f = frames[fi];
    if (f.first >= static_cast<int>(count) || f.last >= static_cast<int>(count)) continue;
    int left = width, right = 0;
    for (const IrEdge* m : messages) {
      if (m->order < f.first || m->order > f.last) continue;
      left = std::min({left, lines[index[m->from]].centre, lines[index[m->to]].centre});
      right = std::max({right, lines[index[m->from]].centre, lines[index[m->to]].centre});
    }
    // an outer frame wraps its inner ones: three cells per level
    int outer = 0;
    for (size_t j = 0; j < frames.size(); ++j) {
      if (j != fi && frames[fi].first <= frames[j].first && frames[j].last <= frames[fi].last && depthOf[j] > depthOf[fi])
        outer = std::max(outer, depthOf[j] - depthOf[fi]);
    }
    left -= 3 + 3 * outer;
    right += static_cast<int>(f.label.size()) + 6 + 3 * outer;
    // frames that open (or close) at the same message stack: one row further out per frame inside
    // this one that opens (or closes) there too, as the SVG does (DS-13)
    int innerOpening = 0, innerClosing = 0;
    for (size_t j = 0; j < frames.size(); ++j) {
      if (j == fi || depthOf[j] <= depthOf[fi]) continue;
      if (frames[j].first == f.first) ++innerOpening;
      if (frames[j].last == f.last) ++innerClosing;
    }
    const int top = rowOf[f.first] - 1 - innerOpening;
    const int bottom = rowOf[f.last] + heightOf[f.last] + innerClosing;
    SequenceFrame placed{f.node, f.first, f.last, top, bottom, left, right, {}};
    for (const auto& d : f.dividers) {
      if (d.first >= static_cast<int>(rowOf.size())) continue;
      placed.dividers.push_back({d.first, rowOf[d.first] - 1, d.second});
    }
    s.frames.push_back(std::move(placed));
  }
  s.laneOf = std::move(index);
  s.messages = std::move(messages);
  s.width = width;
  s.height = rows;
  s.headHeight = 3;  // a head is a box of three rows: its border, its name, its border
  figure.sequence = std::move(s);
  return figure;
}

}  // namespace render
}  // namespace ds
