#include "driftspec/render/Text.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <vector>

namespace driftspec {
namespace render {

namespace {

const Metrics M = Metrics::text();

struct Cell {
  std::string glyph = " ";
  std::string color;  // ANSI SGR parameters, "" = default
  int line = 0;       // 1 horizontal, 2 vertical, 3 both: what an edge already drew here
};

/// The grid, with the origin shifted so negative coordinates still land on it.
class Grid {
 public:
  Grid(int minX, int minY, int w, int h) : minX_(minX), minY_(minY), w_(w), h_(h), cells_(static_cast<size_t>(w) * h) {}

  Cell* at(int x, int y) {
    x -= minX_;
    y -= minY_;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return nullptr;
    return &cells_[static_cast<size_t>(y) * w_ + x];
  }

  void put(int x, int y, const std::string& glyph, const std::string& color = "") {
    if (Cell* c = at(x, y)) {
      c->glyph = glyph;
      c->color = color;
      c->line = 0;
    }
  }

  void text(int x, int y, const std::string& s, const std::string& color = "") {
    int col = x;
    for (size_t i = 0; i < s.size();) {
      // one UTF-8 code point per cell
      size_t len = 1;
      const unsigned char c = static_cast<unsigned char>(s[i]);
      if (c >= 0xF0) len = 4;
      else if (c >= 0xE0) len = 3;
      else if (c >= 0xC0) len = 2;
      put(col++, y, s.substr(i, len), color);
      i += len;
    }
  }

  /// An edge segment cell: joins with what another edge already drew.
  void line(int x, int y, bool horizontal, bool dashed, const std::string& color) {
    Cell* c = at(x, y);
    if (!c) return;
    const int bit = horizontal ? 1 : 2;
    if (c->line && (c->line | bit) == 3 && c->line != bit) {
      c->glyph = "\xE2\x94\xBC";  // ┼
    } else if (c->line == bit) {
      return;
    } else {
      c->glyph = horizontal ? (dashed ? "\xE2\x94\x84" : "\xE2\x94\x80") : (dashed ? "\xE2\x94\x86" : "\xE2\x94\x82");  // ┄ ─ ┆ │
    }
    c->line |= bit;
    c->color = color;
  }

  /// base64 of u32 little-endian triplets per cell: codepoint, foreground, background
  std::string encode() const {
    static const std::map<std::string, uint32_t> rgb = {{"2", 0x64748b}, {"34", 0x2563eb}, {"32", 0x16a34a}, {"31", 0xdc2626}, {"33", 0xd97706}, {"35", 0x7c3aed}};
    std::string bytes;
    bytes.reserve(cells_.size() * 12);
    auto put32 = [&](uint32_t v) {
      for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    };
    for (const Cell& c : cells_) {
      uint32_t cp = 0x20;
      const unsigned char b0 = static_cast<unsigned char>(c.glyph[0]);
      if (b0 < 0x80) cp = b0;
      else if (b0 >= 0xF0 && c.glyph.size() >= 4) cp = ((b0 & 0x07u) << 18) | ((c.glyph[1] & 0x3fu) << 12) | ((c.glyph[2] & 0x3fu) << 6) | (c.glyph[3] & 0x3fu);
      else if (b0 >= 0xE0 && c.glyph.size() >= 3) cp = ((b0 & 0x0fu) << 12) | ((c.glyph[1] & 0x3fu) << 6) | (c.glyph[2] & 0x3fu);
      else if (b0 >= 0xC0 && c.glyph.size() >= 2) cp = ((b0 & 0x1fu) << 6) | (c.glyph[1] & 0x3fu);
      const auto colour = rgb.find(c.color);
      put32(cp);
      put32(colour == rgb.end() ? 0x01000000u : colour->second);
      put32(0x01000000u);
    }
    static const char* const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
      const uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) | (static_cast<unsigned char>(bytes[i + 1]) << 8) | static_cast<unsigned char>(bytes[i + 2]);
      out += alphabet[(n >> 18) & 63];
      out += alphabet[(n >> 12) & 63];
      out += alphabet[(n >> 6) & 63];
      out += alphabet[n & 63];
    }
    if (i < bytes.size()) {
      uint32_t n = static_cast<unsigned char>(bytes[i]) << 16;
      if (i + 1 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 1]) << 8;
      out += alphabet[(n >> 18) & 63];
      out += alphabet[(n >> 12) & 63];
      out += i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=';
      out += '=';
    }
    return out;
  }

  int columns() const { return w_; }
  int rows() const { return h_; }

  std::string dump(bool color) const {
    std::string out;
    for (int y = 0; y < h_; ++y) {
      std::string row;
      std::string current;
      for (int x = 0; x < w_; ++x) {
        const Cell& c = cells_[static_cast<size_t>(y) * w_ + x];
        if (color && c.color != current) {
          row += c.color.empty() ? "\x1b[0m" : "\x1b[" + c.color + "m";
          current = c.color;
        }
        row += c.glyph;
      }
      if (color && !current.empty()) row += "\x1b[0m";
      // trailing blanks are noise in a buffer
      const auto end = row.find_last_not_of(' ');
      out += (end == std::string::npos ? "" : row.substr(0, end + 1)) + "\n";
    }
    return out;
  }

 private:
  int minX_, minY_, w_, h_;
  std::vector<Cell> cells_;
};

// palette roles, as SGR parameters: the SVG tokens' nearest terminal colours
const char* const kMuted = "2";
const char* const kName = "1";
const char* const kType = "34";
const char* const kPublic = "32";
const char* const kPrivate = "31";
const char* const kProtected = "33";
const char* const kNote = "33";
const char* const kContainer = "2";
const char* const kEdge = "";

std::vector<std::string> linesOf(const std::string& label) {
  std::vector<std::string> out;
  std::string current;
  for (size_t i = 0; i < label.size(); ++i) {
    if (label[i] == '\n' || (label[i] == '\\' && i + 1 < label.size() && label[i + 1] == 'n')) {
      out.push_back(current);
      current.clear();
      if (label[i] == '\\') ++i;
    } else {
      current += label[i];
    }
  }
  out.push_back(current);
  return out;
}

void frame(Grid& g, const Rect& r, bool rounded, const std::string& color) {
  const char* tl = rounded ? "\xE2\x95\xAD" : "\xE2\x94\x8C";  // ╭ ┌
  const char* tr = rounded ? "\xE2\x95\xAE" : "\xE2\x94\x90";  // ╮ ┐
  const char* bl = rounded ? "\xE2\x95\xB0" : "\xE2\x94\x94";  // ╰ └
  const char* br = rounded ? "\xE2\x95\xAF" : "\xE2\x94\x98";  // ╯ ┘
  for (int x = r.x + 1; x < r.x + r.w - 1; ++x) {
    g.put(x, r.y, "\xE2\x94\x80", color);
    g.put(x, r.y + r.h - 1, "\xE2\x94\x80", color);
  }
  for (int y = r.y + 1; y < r.y + r.h - 1; ++y) {
    g.put(r.x, y, "\xE2\x94\x82", color);
    g.put(r.x + r.w - 1, y, "\xE2\x94\x82", color);
  }
  g.put(r.x, r.y, tl, color);
  g.put(r.x + r.w - 1, r.y, tr, color);
  g.put(r.x, r.y + r.h - 1, bl, color);
  g.put(r.x + r.w - 1, r.y + r.h - 1, br, color);
}

void clear(Grid& g, const Rect& r) {
  for (int y = r.y; y < r.y + r.h; ++y) {
    for (int x = r.x; x < r.x + r.w; ++x) g.put(x, y, " ");
  }
}

/// `+ abstract run(model: Model) -> list<Finding>`: visibility coloured, types coloured.
void member(Grid& g, int x, int y, const std::string& line) {
  static const std::regex head(R"(^\s*([+\-#~])?\s*(.*)$)");
  std::smatch m;
  std::string body = line;
  std::string vis;
  if (std::regex_match(line, m, head)) {
    vis = m[1].str();
    body = m[2].str();
  }
  if (!vis.empty()) {
    const std::string color = vis == "+" ? kPublic : vis == "-" ? kPrivate : kProtected;
    g.text(x, y, vis, color);
  }
  const int start = x + (vis.empty() ? 0 : M.DOT_W);
  // types follow ": " or "-> "; everything else is plain
  size_t i = 0;
  int col = start;
  while (i < body.size()) {
    size_t typeAt = std::string::npos;
    size_t skip = 0;
    const size_t colon = body.find(": ", i);
    const size_t arrow = body.find("-> ", i);
    if (colon != std::string::npos && (arrow == std::string::npos || colon < arrow)) {
      typeAt = colon;
      skip = 2;
    } else if (arrow != std::string::npos) {
      typeAt = arrow;
      skip = 3;
    }
    if (typeAt == std::string::npos) {
      g.text(col, y, body.substr(i));
      break;
    }
    g.text(col, y, body.substr(i, typeAt + skip - i));
    col += static_cast<int>(typeAt + skip - i);
    size_t end = typeAt + skip;
    int depth = 0;
    while (end < body.size()) {
      const char c = body[end];
      if (c == '<' || c == '(') ++depth;
      if (c == '>' || c == ')') {
        if (depth == 0) break;
        --depth;
      }
      if ((c == ',' || c == ' ' || c == '=') && depth == 0) break;
      ++end;
    }
    g.text(col, y, body.substr(typeAt + skip, end - typeAt - skip), kType);
    col += static_cast<int>(end - typeAt - skip);
    i = end;
  }
}

void box(Grid& g, const Placed& p) {
  const IrNode& n = *p.node;
  const Rect& r = p.rect;
  clear(g, r);
  if (n.kind == "note") {
    frame(g, r, false, kNote);
    const auto lines = linesOf(n.label);
    for (size_t i = 0; i < lines.size(); ++i) g.text(r.x + M.PAD_X, r.y + M.PAD_Y + static_cast<int>(i), lines[i], kNote);
    return;
  }
  frame(g, r, true, "");
  int y = r.y + M.PAD_Y;
  if (!n.stereotype.empty()) g.text(r.x + M.PAD_X, y++, "\xC2\xAB" + n.stereotype + "\xC2\xBB", kMuted);
  std::string badge = "C";
  if (n.abstract && n.classifier.empty()) badge = "A";
  else if (!n.classifier.empty()) badge = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(n.classifier[0]))));
  g.text(r.x + M.PAD_X, y, badge, n.classifier == "interface" ? kPublic : n.classifier == "enum" ? kProtected : "35");
  g.text(r.x + M.PAD_X + M.BADGE_W, y, n.label, kName);
  ++y;
  for (const auto& section : n.sections) {
    g.put(r.x, y, "\xE2\x94\x9C");  // ├
    for (int x = r.x + 1; x < r.x + r.w - 1; ++x) g.put(x, y, "\xE2\x94\x80");
    g.put(r.x + r.w - 1, y, "\xE2\x94\xA4");  // ┤
    ++y;
    for (const auto& line : section) member(g, r.x + M.PAD_X, y++, line);
  }
}

void container(Grid& g, const Placed& p) {
  const Rect& r = p.rect;
  frame(g, r, false, kContainer);
  g.text(r.x + 2, r.y, " " + p.node->label + " ", kContainer);
}

/// The marker glyph for an edge that enters its target moving in (dx, dy).
std::string marker(const std::string& kind, int dx, int dy) {
  if (kind == "inheritance" || kind == "realization") return dy < 0 ? "\xE2\x96\xB3" : dy > 0 ? "\xE2\x96\xBD" : dx < 0 ? "\xE2\x97\x81" : "\xE2\x96\xB7";  // △ ▽ ◁ ▷
  if (kind == "composition") return "\xE2\x97\x86";  // ◆
  if (kind == "aggregation") return "\xE2\x97\x87";  // ◇
  if (kind == "attachment") return "";
  return dy < 0 ? "\xE2\x96\xB2" : dy > 0 ? "\xE2\x96\xBC" : dx < 0 ? "\xE2\x97\x80" : "\xE2\x96\xB6";  // ▲ ▼ ◀ ▶
}

void edge(Grid& g, const IrEdge& e, Route route, const std::map<std::string, const Placed*>& byId) {
  if (route.points.size() < 2) return;
  const bool diamond = e.kind == "composition" || e.kind == "aggregation";
  const bool dashed = e.kind == "dependency" || e.kind == "realization" || e.kind == "attachment";
  // an anchor on a top or left border names the border cell itself: step outside it
  auto outside = [&](Point& p, const std::string& id, bool first) {
    const auto it = byId.find(id);
    if (it == byId.end()) return;
    const Rect& r = it->second->rect;
    if (p.y == r.y) p.y -= 1;
    if (p.x == r.x) p.x -= 1;
    (void)first;
  };
  outside(route.points.front(), e.from, true);
  outside(route.points.back(), e.to, false);
  std::vector<Point>& pts = route.points;
  for (size_t i = 1; i < pts.size(); ++i) {
    const Point a = pts[i - 1], b = pts[i];
    const bool horizontal = a.y == b.y;
    const int steps = horizontal ? std::abs(b.x - a.x) : std::abs(b.y - a.y);
    const int dx = horizontal ? (b.x > a.x ? 1 : -1) : 0;
    const int dy = horizontal ? 0 : (b.y > a.y ? 1 : -1);
    for (int s = 0; s <= steps; ++s) {
      const int x = a.x + dx * s, y = a.y + dy * s;
      const bool corner = (s == 0 && i > 1) || (s == steps && i + 1 < pts.size());
      if (corner) {
        // which way the line turns at this point
        const Point prev = s == 0 ? pts[i - 2] : a;
        const Point next = s == 0 ? b : pts[i + 1];
        const bool up = prev.y < y || next.y < y;
        const bool left = prev.x < x || next.x < x;
        const bool down = prev.y > y || next.y > y;
        const bool right = prev.x > x || next.x > x;
        std::string glyph = "\xE2\x94\xBC";
        if (up && right) glyph = "\xE2\x94\x94";        // └
        else if (up && left) glyph = "\xE2\x94\x98";    // ┘
        else if (down && right) glyph = "\xE2\x94\x8C"; // ┌
        else if (down && left) glyph = "\xE2\x94\x90";  // ┐
        g.put(x, y, glyph, kEdge);
      } else {
        g.line(x, y, horizontal, dashed, kEdge);
      }
    }
  }
  // the head: on the target end, or on the source end for a diamond (the whole holds it)
  const Point tail = pts.front(), head = pts.back();
  const Point before = pts[pts.size() - 2];
  if (diamond) {
    g.put(tail.x, tail.y, marker(e.kind, 0, 0), kEdge);
    const std::string m = marker("association", head.x - before.x, head.y - before.y);
    if (e.label.empty()) (void)m;
  } else {
    const std::string m = marker(e.kind, head.x - before.x, head.y - before.y);
    if (!m.empty()) g.put(head.x, head.y, m, kEdge);
  }
  if (!e.label.empty()) {
    size_t best = 0;
    int bestLen = -1;
    for (size_t i = 1; i < pts.size(); ++i) {
      const int len = std::abs(pts[i].x - pts[i - 1].x) + std::abs(pts[i].y - pts[i - 1].y);
      if (len > bestLen) {
        bestLen = len;
        best = i - 1;
      }
    }
    const Point a = pts[best], b = pts[best + 1];
    const auto lines = linesOf(e.label);
    if (a.y == b.y) {
      std::string joined;
      for (const auto& l : lines) joined += (joined.empty() ? "" : ", ") + l;
      g.text((a.x + b.x) / 2 - static_cast<int>(joined.size()) / 2, a.y - 1, joined, kMuted);
    } else {
      for (size_t i = 0; i < lines.size(); ++i) g.text(a.x + 2, (a.y + b.y) / 2 + static_cast<int>(i), lines[i], kMuted);
    }
  }
}

}  // namespace


// ---- sequences: lifelines as columns, messages as rows ----------------------------

namespace {

struct SeqLine {
  std::string id;
  std::string label;
  int centre = 0;
  int width = 0;
};

Grid sequenceGrid(const IrDocument& doc) {
  std::vector<SeqLine> lines;
  std::map<std::string, size_t> index;
  for (const auto& n : doc.nodes) {
    if (n.kind != "lifeline") continue;
    index[n.id] = lines.size();
    lines.push_back({n.id, n.label, 0, static_cast<int>(n.label.size()) + 4});
  }
  if (lines.empty()) return Grid(0, 0, 1, 1);
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
  };
  std::vector<FrameAt> frames;
  for (const auto& n : doc.nodes) {
    if (n.kind != "frame") continue;
    FrameAt f{n.span.first, n.span.second, n.label, {}};
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
  Grid g(0, 0, width, rows);
  // lifelines
  for (const auto& l : lines) {
    const int x0 = l.centre - l.width / 2;
    frame(g, Rect{x0, 0, l.width, 3}, true, "");
    g.text(x0 + 2, 1, l.label, kName);
    for (int y = 3; y < rows; ++y) g.put(l.centre, y, "\xE2\x94\x86", kMuted);  // ┆
  }
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
      if (j != fi && frames[fi].first <= frames[j].first && frames[j].last <= frames[fi].last && depthOf[j] > depthOf[fi]) outer = std::max(outer, depthOf[j] - depthOf[fi]);
    }
    left -= 3 + 3 * outer;
    right += static_cast<int>(f.label.size()) + 6 + 3 * outer;
    const int top = rowOf[f.first] - 1 - (opens[f.first] - 1 - static_cast<int>(depthOf[fi] > 0 ? 0 : 0)) * 0;
    const int bottom = rowOf[f.last] + heightOf[f.last] + (closes[f.last] > 1 ? (depthOf[fi] == 0 ? 1 : 0) : 0);
    for (int xx = left; xx <= right; ++xx) {
      g.put(xx, top, "\xE2\x94\x80", kMuted);
      g.put(xx, bottom, "\xE2\x94\x80", kMuted);
    }
    for (int yy = top + 1; yy < bottom; ++yy) {
      g.put(left, yy, "\xE2\x94\x82", kMuted);
      g.put(right, yy, "\xE2\x94\x82", kMuted);
    }
    g.put(left, top, "\xE2\x94\x8C", kMuted);
    g.put(right, top, "\xE2\x94\x90", kMuted);
    g.put(left, bottom, "\xE2\x94\x94", kMuted);
    g.put(right, bottom, "\xE2\x94\x98", kMuted);
    g.text(left + 1, top, " " + f.label + " ", kMuted);
    for (const auto& d : f.dividers) {
      if (d.first >= static_cast<int>(rowOf.size())) continue;
      const int yy = rowOf[d.first] - 1;
      for (int xx = left + 1; xx < right; ++xx) g.put(xx, yy, "\xE2\x94\x84", kMuted);
      g.text(left + 1, yy, " " + d.second + " ", kMuted);
    }
  }
  // messages
  for (const IrEdge* m : messages) {
    const int y = rowOf[m->order] + 1;
    const int a = lines[index[m->from]].centre, b = lines[index[m->to]].centre;
    const std::string dash = m->dashed ? "\xE2\x94\x84" : "\xE2\x94\x80";  // ┄ ─
    if (a == b) {
      g.text(a + 1, y, dash + dash + "\xE2\x94\x90", kEdge);        // ──┐
      g.text(a + 1, y + 1, "\xE2\x97\x80" + dash + "\xE2\x94\x98", kEdge);  // ◀─┘
      g.text(a + 5, y, m->label, kMuted);
      continue;
    }
    const int lo = std::min(a, b), hi = std::max(a, b);
    for (int xx = lo + 1; xx < hi; ++xx) g.put(xx, y, dash, kEdge);
    if (b > a) g.put(hi - 1, y, "\xE2\x96\xB6", kEdge);  // ▶
    else g.put(lo + 1, y, "\xE2\x97\x80", kEdge);        // ◀
    g.text((lo + hi) / 2 - static_cast<int>(m->label.size()) / 2, y - 1, m->label, kMuted);
  }
  return g;
}

}  // namespace

namespace {

Grid classGrid(const IrDocument& doc, const LayoutResult& lay) {
  int minX = 0, minY = 0, maxX = 1, maxY = 1;
  for (const auto& p : lay.placed) {
    minX = std::min(minX, p.rect.x - 1);
    minY = std::min(minY, p.rect.y - 1);
    maxX = std::max(maxX, p.rect.x + p.rect.w + 1);
    maxY = std::max(maxY, p.rect.y + p.rect.h + 1);
  }
  for (const auto& r : lay.routes) {
    for (const auto& pt : r.second.points) {
      minX = std::min(minX, pt.x - 1);
      minY = std::min(minY, pt.y - 2);
      maxX = std::max(maxX, pt.x + 2);
      maxY = std::max(maxY, pt.y + 2);
    }
  }
  Grid g(minX, minY, maxX - minX + 1, maxY - minY + 1);
  std::map<std::string, const Placed*> byId;
  for (const auto& p : lay.placed) byId[p.node->id] = &p;
  // outer containers first, then edges, then boxes over the edges, then the labels
  std::vector<const Placed*> frames;
  for (const auto& p : lay.placed) {
    if (p.node->kind == "container") frames.push_back(&p);
  }
  std::stable_sort(frames.begin(), frames.end(), [](const Placed* a, const Placed* b) { return a->rect.w * a->rect.h > b->rect.w * b->rect.h; });
  for (const Placed* f : frames) container(g, *f);
  for (size_t i = 0; i < doc.edges.size(); ++i) {
    const auto r = lay.routes.find(i);
    if (r != lay.routes.end()) edge(g, doc.edges[i], r->second, byId);
  }
  for (const auto& p : lay.placed) {
    if (p.node->kind != "container") box(g, p);
  }
  return g;
}

bool isSequence(const IrDocument& doc) {
  for (const auto& n : doc.nodes) {
    if (n.kind == "lifeline") return true;
  }
  return false;
}

}  // namespace

std::string Text::render(const IrDocument& doc, bool color) const {
  if (isSequence(doc)) return sequenceGrid(doc).dump(color);
  const IrDocument bundled = doc.bundled();
  return render(bundled, layout(bundled, M), color);
}

std::string Text::render(const IrDocument& doc, const LayoutResult& lay, bool color) const {
  if (isSequence(doc)) return sequenceGrid(doc).dump(color);
  return classGrid(doc, lay).dump(color);
}

std::string Text::cells(const IrDocument& doc) const {
  const IrDocument bundled = doc.bundled();
  const Grid g = isSequence(doc) ? sequenceGrid(doc) : classGrid(bundled, layout(bundled, M));
  nlohmann::json out = {{"columns", g.columns()}, {"rows", g.rows()}, {"cells", g.encode()}, {"text", g.dump(false)}};
  return out.dump();
}

}  // namespace render
}  // namespace driftspec
