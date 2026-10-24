#include "driftspec/render/Text.hpp"

#include <algorithm>
#include <map>
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

std::string Text::render(const IrDocument& doc, bool color) const {
  const IrDocument bundled = doc.bundled();
  return render(bundled, layout(bundled, M), color);
}

std::string Text::render(const IrDocument& doc, const LayoutResult& lay, bool color) const {
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
  return g.dump(color);
}

}  // namespace render
}  // namespace driftspec
