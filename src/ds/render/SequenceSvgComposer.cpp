#include "ds/render/SequenceSvgComposer.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/render/Lane.hpp"
#include "ds/render/SequenceLayout.hpp"

namespace ds {
namespace render {

Figure SequenceSvgComposer::compose(const IrDocument& input, int) const {
  Figure figure;
  figure.source = &input;  // drawn as it is: the layout points into it
  const IrDocument& doc = input;
  const Metrics& M = metrics_;
  const int HEAD_H = 38, ROW_H = 30, SELF_H = 42, MARGIN = 20, SELF_W = 32;
  const int FRAME_LEAD = 30, FRAME_GAP = 8, DIVIDER_LEAD = 26, FRAME_INSET = 16;
  std::vector<Lane> lanes;
  std::map<std::string, size_t> laneOf;
  for (const auto& n : doc.nodes) {
    if (n.kind != "lifeline") continue;
    laneOf[n.id] = lanes.size();
    const bool actor = n.classifier == "actor";
    lanes.push_back({&n, 0, static_cast<int>(n.label.size()) * M.CHAR_W + (actor ? 8 : 2 * M.PAD_X)});
  }
  std::vector<const IrEdge*> messages;
  for (const auto& e : doc.edges) {
    if (e.kind == "message" && laneOf.count(e.from) && laneOf.count(e.to)) messages.push_back(&e);
  }
  std::sort(messages.begin(), messages.end(), [](const IrEdge* a, const IrEdge* b) { return a->order < b->order; });
  if (lanes.empty()) {
    figure.sequence = SequenceLayout();
    return figure;
  }

  // ---- lanes: gaps wide enough for the heads and for the labels crossing them
  const size_t count = lanes.size();
  std::vector<int> gap(count, 0);
  auto half = [&](size_t i) { return (lanes[i].headW + 1) / 2; };
  for (size_t i = 0; i + 1 < count; ++i) gap[i] = half(i) + half(i + 1) + 24;
  auto need = [&](size_t a, size_t b, int width) {
    if (a >= b || b >= count) return;
    int have = 0;
    for (size_t j = a; j < b; ++j) have += gap[j];
    if (have >= width) return;
    const int deficit = width - have, each = deficit / static_cast<int>(b - a);
    for (size_t j = a; j < b; ++j) gap[j] += each;
    gap[b - 1] += deficit - each * static_cast<int>(b - a);
  };
  for (const IrEdge* m : messages) {
    const size_t a = laneOf[m->from], b = laneOf[m->to];
    const int textW = static_cast<int>(m->label.size()) * M.CHAR_W;
    if (a == b) {
      if (a + 1 < count) need(a, a + 1, SELF_W + 6 + textW + 8 + half(a + 1));
    } else {
      need(std::min(a, b), std::max(a, b), textW + 32);
    }
  }
  int x = MARGIN + half(0) + 16;
  for (size_t i = 0; i < count; ++i) {
    lanes[i].cx = x;
    if (i + 1 < count) x += gap[i];
  }
  int width = lanes.back().cx + half(count - 1) + 16 + MARGIN;

  // ---- frames and rows
  struct Frame {
    const IrNode* node;
    int first, last;
    int depth = 0;  // frames enclosing it
    int inner = 0;  // levels nested inside it
    int top = 0, bottom = 0, left = 0, right = 0;
  };
  std::vector<Frame> frames;
  for (const auto& n : doc.nodes) {
    if (n.kind == "frame" && n.span.first >= 0) frames.push_back({&n, n.span.first, n.span.second});
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    for (size_t j = 0; j < frames.size(); ++j) {
      if (i == j) continue;
      const bool inside = frames[j].first <= frames[i].first && frames[i].last <= frames[j].last &&
                          (frames[j].first < frames[i].first || frames[i].last < frames[j].last);
      if (inside) ++frames[i].depth;
    }
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    for (size_t j = 0; j < frames.size(); ++j) {
      if (i != j && frames[i].first <= frames[j].first && frames[j].last <= frames[i].last && frames[j].depth > frames[i].depth) {
        frames[i].inner = std::max(frames[i].inner, frames[j].depth - frames[i].depth);
      }
    }
  }
  const int rowsCount = messages.empty() ? 0 : messages.back()->order + 1;
  std::vector<int> rowStart(rowsCount, 0), rowLine(rowsCount, 0), rowEnd(rowsCount, 0);
  std::vector<std::vector<Frame*>> opensAt(rowsCount), closesAt(rowsCount);
  std::vector<std::vector<std::pair<Frame*, std::string>>> dividersAt(rowsCount);
  for (Frame& f : frames) {
    if (f.first < rowsCount) opensAt[f.first].push_back(&f);
    if (f.last < rowsCount) closesAt[f.last].push_back(&f);
    for (const auto& d : f.node->dividers) {
      if (d.first < rowsCount) dividersAt[d.first].push_back({&f, d.second});
    }
  }
  for (auto& v : opensAt) std::sort(v.begin(), v.end(), [](Frame* a, Frame* b) { return a->depth < b->depth; });   // outer first
  for (auto& v : closesAt) std::sort(v.begin(), v.end(), [](Frame* a, Frame* b) { return a->depth > b->depth; });  // inner first
  int y = MARGIN + HEAD_H + 16;
  for (const IrEdge* m : messages) {
    const int r = m->order;
    rowStart[r] = y;
    for (size_t k = 0; k < opensAt[r].size(); ++k) {
      opensAt[r][k]->top = y + FRAME_GAP;
      y += FRAME_LEAD;
    }
    for (size_t k = 0; k < dividersAt[r].size(); ++k) y += DIVIDER_LEAD;
    rowLine[r] = y + 20;
    y += m->from == m->to ? SELF_H : ROW_H;
    for (size_t k = 0; k < closesAt[r].size(); ++k) {
      closesAt[r][k]->bottom = y + 2;
      y += FRAME_GAP;
    }
    rowEnd[r] = y;
  }
  const int bottomY = y + 16;
  for (Frame& f : frames) {
    int left = width, right = 0;
    for (const IrEdge* m : messages) {
      if (m->order < f.first || m->order > f.last) continue;
      left = std::min({left, lanes[laneOf[m->from]].cx, lanes[laneOf[m->to]].cx});
      right = std::max({right, lanes[laneOf[m->from]].cx, lanes[laneOf[m->to]].cx});
      if (m->from == m->to) right = std::max(right, lanes[laneOf[m->to]].cx + SELF_W + static_cast<int>(m->label.size()) * M.CHAR_W + 12);
    }
    f.left = left - 44 - FRAME_INSET * f.inner;
    f.right = right + 44 + FRAME_INSET * f.inner;
    const int labelW = 8 + static_cast<int>(f.node->classifier.size()) * 7 + 10 + static_cast<int>(f.node->label.size()) * 7 + 20;
    f.right = std::max(f.right, f.left + labelW);
    width = std::max(width, f.right + MARGIN);
  }

  // what was placed, kept for the drawing
  SequenceLayout s;
  s.lanes = std::move(lanes);
  s.laneOf = std::move(laneOf);
  s.messages = std::move(messages);
  for (int r = 0; r < rowsCount; ++r) s.rows.push_back({rowStart[r], rowLine[r], rowEnd[r]});
  for (const Frame& f : frames) {
    SequenceFrame placed{f.node, f.first, f.last, f.top, f.bottom, f.left, f.right, {}};
    for (const auto& d : f.node->dividers) {
      if (d.first >= rowsCount) continue;
      // the divider sits in the lead reserved above its row
      int dy = rowStart[d.first];
      for (size_t k = 0; k < opensAt[d.first].size(); ++k) dy += FRAME_LEAD;
      dy += 6;
      placed.dividers.push_back({d.first, dy, d.second});
    }
    s.frames.push_back(std::move(placed));
  }
  s.width = width;
  s.height = bottomY;
  s.margin = MARGIN;
  s.headHeight = HEAD_H;
  s.selfWidth = SELF_W;
  figure.sequence = std::move(s);
  return figure;
}

}  // namespace render
}  // namespace ds
