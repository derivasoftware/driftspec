#include "ds/scene/Packer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ds {
namespace scene {

Packer::Packer(std::map<std::string, double>* widths, bool following) : widths_(widths), following_(following) {}

std::map<std::string, Box> Packer::pack(const Frame& root) const {
  std::map<std::string, Box> out;
  place(root, size(root), 0, 0, out);
  return out;
}

Packed Packer::size(const Frame& frame) const {
  switch (frame.arrangement) {
    case Frame::Arrangement::Shelves:
      return shelves(frame);
    case Frame::Arrangement::Tree:
      return tree(frame);
    case Frame::Arrangement::Fixed:
      break;
  }
  return fixed(frame);
}

Packed Packer::fixed(const Frame& frame) const {
  Packed p;
  p.w = frame.w;
  p.h = frame.h;
  p.at = frame.at;
  for (const Frame& k : frame.kids) p.kids.push_back(size(k));
  return p;
}

Packed Packer::shelves(const Frame& frame) const {
  Packed p;
  for (const Frame& k : frame.kids) p.kids.push_back(size(k));
  const auto used = shelf(p.kids, frame.gap, width(frame.id, target(p.kids, frame.atLeast, frame.factor)), p.at);
  if (!following_) (*widths_)[frame.id] = used.first;
  for (auto& a : p.at) a = {a.first + frame.left, a.second + frame.top};
  double y = frame.portTop;
  for (const Frame& port : frame.ports) {  // on the left border: half outside, half inside
    Packed s = size(port);
    p.at.push_back({-s.w / 2, y});
    y += s.h + frame.portGap;
    p.kids.push_back(std::move(s));
  }
  p.w = used.first + frame.left + frame.right;
  p.h = std::max(used.second + frame.top + frame.bottom, y + frame.bottom);
  return p;
}

Packed Packer::tree(const Frame& frame) const {
  Packed p;
  if (frame.kids.empty()) return p;
  Packed base = size(frame.kids.front());
  std::vector<Packed> row;
  std::vector<double> xs;
  double x = 0, rowH = 0;
  for (size_t i = 1; i < frame.kids.size(); ++i) {
    row.push_back(size(frame.kids[i]));
    xs.push_back(x);
    x += row.back().w + frame.gap;
    rowH = std::max(rowH, row.back().h);
  }
  const double rowW = x - frame.gap, baseW = base.w, baseH = base.h;
  p.w = std::max(rowW, baseW);
  p.at.push_back({(p.w - baseW) / 2, 0});
  p.kids.push_back(std::move(base));
  for (size_t i = 0; i < row.size(); ++i) {
    p.at.push_back({xs[i] + (p.w - rowW) / 2, baseH + frame.drop});
    p.kids.push_back(std::move(row[i]));
  }
  p.h = baseH + frame.drop + rowH;
  return p;
}

double Packer::width(const std::string& id, double computed) const {
  if (!following_) return computed;
  const auto kept = widths_->find(id);
  return kept == widths_->end() ? computed : kept->second;
}

std::pair<double, double> Packer::shelf(const std::vector<Packed>& kids, double gap, double target,
                                        std::vector<std::pair<double, double>>& at) {
  double x = 0, y = 0, rowH = 0, width = 0;
  for (const Packed& s : kids) {
    if (x > 0 && x + s.w > target) {
      x = 0;
      y += rowH + gap;
      rowH = 0;
    }
    at.push_back({x, y});
    x += s.w + gap;
    rowH = std::max(rowH, s.h);
    width = std::max(width, x - gap);
  }
  return {width, y + rowH};
}

double Packer::target(const std::vector<Packed>& kids, double atLeast, double factor) {
  double area = 0, widest = atLeast;
  for (const Packed& s : kids) {
    area += s.w * s.h;
    widest = std::max(widest, s.w);
  }
  return std::max(widest, std::sqrt(area) * factor);
}

void Packer::place(const Frame& frame, const Packed& packed, double x, double y, std::map<std::string, Box>& out) {
  out[frame.id] = {x, y, packed.w, packed.h};
  const size_t shelved = frame.kids.size();
  for (size_t i = 0; i < packed.kids.size(); ++i) {
    const Frame& k = i < shelved ? frame.kids[i] : frame.ports[i - shelved];
    place(k, packed.kids[i], x + packed.at[i].first, y + packed.at[i].second, out);
  }
}

}  // namespace scene
}  // namespace ds
