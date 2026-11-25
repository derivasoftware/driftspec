#include "ds/layout/StackedEngine.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ds {
namespace layout {

SequencePlacement StackedEngine::place(const SequenceGraph& graph) const {
  const SequenceSpacing& S = spacing_;
  const std::vector<SequenceMessage>& messages = graph.messages;
  SequencePlacement out;
  const size_t count = graph.lanes.size();
  if (count == 0) return out;
  std::vector<int> head(count), centre(count, 0);
  for (size_t i = 0; i < count; ++i)
    head[i] = static_cast<int>(graph.lanes[i].labelLength) * S.advance + (graph.lanes[i].actor ? S.actorPad : S.headPad);

  // ---- lanes: gaps wide enough for the heads and for the labels crossing them
  std::vector<int> gap(count, 0);
  auto half = [&](size_t i) { return (head[i] + 1) / 2; };
  for (size_t i = 0; i + 1 < count; ++i) gap[i] = half(i) + half(i + 1) + S.laneGap;
  auto need = [&](size_t a, size_t b, int width) {
    if (a >= b || b >= count) return;
    int have = 0;
    for (size_t j = a; j < b; ++j) have += gap[j];
    if (have >= width) return;
    const int deficit = width - have, each = deficit / static_cast<int>(b - a);
    for (size_t j = a; j < b; ++j) gap[j] += each;
    gap[b - 1] += deficit - each * static_cast<int>(b - a);
  };
  for (const SequenceMessage& m : messages) {
    const size_t a = m.from, b = m.to;
    const int textW = static_cast<int>(m.labelLength) * S.advance;
    if (a == b) {
      if (a + 1 < count) need(a, a + 1, S.selfWidth + S.selfLead + textW + S.selfTail + half(a + 1));
    } else {
      need(std::min(a, b), std::max(a, b), textW + S.crossPad);
    }
  }
  int x = S.marginX + half(0) + S.edge;
  for (size_t i = 0; i < count; ++i) {
    centre[i] = x;
    if (i + 1 < count) x += gap[i];
  }
  int width = centre.back() + half(count - 1) + S.edge + S.marginX;

  // ---- frames and rows
  struct Frame {
    const SequenceBlock* block;
    int first, last;
    int depth = 0;  // frames enclosing it
    int inner = 0;  // levels nested inside it
    int top = 0, bottom = 0, left = 0, right = 0;
  };
  std::vector<Frame> frames;
  for (const SequenceBlock& b : graph.blocks) frames.push_back({&b, b.first, b.last});
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
  const int rowsCount = messages.empty() ? 0 : messages.back().row + 1;
  std::vector<int> rowStart(rowsCount, 0), rowLine(rowsCount, 0), rowEnd(rowsCount, 0), dividersAt(rowsCount, 0);
  std::vector<std::vector<Frame*>> opensAt(rowsCount), closesAt(rowsCount);
  for (Frame& f : frames) {
    if (f.first < rowsCount) opensAt[f.first].push_back(&f);
    if (f.last < rowsCount) closesAt[f.last].push_back(&f);
    for (const int d : f.block->dividers) {
      if (d < rowsCount) ++dividersAt[d];
    }
  }
  for (auto& v : opensAt) std::sort(v.begin(), v.end(), [](Frame* a, Frame* b) { return a->depth < b->depth; });   // outer first
  for (auto& v : closesAt) std::sort(v.begin(), v.end(), [](Frame* a, Frame* b) { return a->depth > b->depth; });  // inner first
  int y = S.marginY + S.headHeight + S.headGap;
  for (const SequenceMessage& m : messages) {
    const int r = m.row;
    rowStart[r] = y;
    for (size_t k = 0; k < opensAt[r].size(); ++k) {
      opensAt[r][k]->top = y + S.frameGap;
      y += S.frameLead;
    }
    y += dividersAt[r] * S.dividerLead;
    rowLine[r] = y + S.lineOffset;
    y += m.from == m.to ? S.selfHeight : S.rowHeight;
    for (size_t k = 0; k < closesAt[r].size(); ++k) {
      closesAt[r][k]->bottom = y + S.frameBottom;
      y += S.frameGap;
    }
    rowEnd[r] = y;
  }
  const int bottomY = y + S.bottomPad;
  for (Frame& f : frames) {
    int left = width, right = 0;
    for (const SequenceMessage& m : messages) {
      if (m.row < f.first || m.row > f.last) continue;
      left = std::min({left, centre[m.from], centre[m.to]});
      right = std::max({right, centre[m.from], centre[m.to]});
      if (m.from == m.to)
        right = std::max(right, centre[m.to] + S.selfWidth + static_cast<int>(m.labelLength) * S.advance + S.selfFrameTail);
    }
    f.left = left - S.frameSide - S.frameInset * f.inner;
    f.right = right + S.frameSide + S.frameInset * f.inner;
    const int labelW = S.frameLabelLead + static_cast<int>(f.block->classifierLength) * S.smallAdvance + S.frameLabelGap +
                       static_cast<int>(f.block->labelLength) * S.smallAdvance + S.frameLabelTail;
    f.right = std::max(f.right, f.left + labelW);
    width = std::max(width, f.right + S.marginX);
  }

  // what was placed
  out.lanes.reserve(count);
  for (size_t i = 0; i < count; ++i) out.lanes.push_back({centre[i], head[i]});
  out.rows.reserve(rowsCount);
  for (int r = 0; r < rowsCount; ++r) out.rows.push_back({rowStart[r], rowLine[r], rowEnd[r]});
  out.blocks.reserve(frames.size());
  for (const Frame& f : frames) {
    BlockPlace placed{f.top, f.bottom, f.left, f.right, {}};
    for (size_t k = 0; k < f.block->dividers.size(); ++k) {
      const int d = f.block->dividers[k];
      if (d >= rowsCount) continue;
      // the divider sits in the lead reserved above its row
      placed.dividers.push_back({k, rowStart[d] + static_cast<int>(opensAt[d].size()) * S.frameLead + S.dividerOffset});
    }
    out.blocks.push_back(std::move(placed));
  }
  out.width = width;
  out.height = bottomY;
  out.headTop = S.marginY;
  out.headHeight = S.headHeight;
  out.selfWidth = S.selfWidth;
  return out;
}

}  // namespace layout
}  // namespace ds
