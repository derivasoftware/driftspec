#include "ds/render/Text.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "ds/text/Text.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace render {

namespace {

/// One character of the drawing, in 7 bytes (DS-350): its UTF-8 bytes inline, the index of its colour in the
/// grid's palette and what an edge drew there. A glyph longer than one code point (never drawn today) is kept
/// in the grid's list of long glyphs and found by its index.
struct Cell {
  std::array<char, 4> glyph = {' ', 0, 0, 0};
  std::uint8_t size = 1;    ///< bytes of glyph in use; kLong: glyph holds the index of a long glyph
  std::uint8_t colour = 0;  ///< the grid's palette; 0 is the default colour
  std::uint8_t line = 0;    ///< 1 horizontal, 2 vertical, 3 both: what an edge already drew here
};

constexpr std::uint8_t kLong = 0xFF;

class Grid {
 public:
  Grid(int minX, int minY, int w, int h) : minX_(minX), minY_(minY), w_(w), h_(h), cells_(sized(w, h)) {}

  Cell* at(int x, int y) {
    x -= minX_;
    y -= minY_;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return nullptr;
    return &cells_[static_cast<size_t>(y) * w_ + x];
  }

  void put(int x, int y, const std::string& glyph, const std::string& color = "") {
    if (Cell* c = at(x, y)) {
      setGlyph(*c, glyph);
      c->colour = paint(color);
      c->line = 0;
    }
  }

  void text(int x, int y, const std::string& s, const std::string& color = "") {
    int col = x;
    for (size_t i = 0; i < s.size();) {
      // one UTF-8 code point per cell
      size_t len = 1;
      const unsigned char c = static_cast<unsigned char>(s[i]);
      if (c >= 0xF0)
        len = 4;
      else if (c >= 0xE0)
        len = 3;
      else if (c >= 0xC0)
        len = 2;
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
      setGlyph(*c, "\xE2\x94\xBC");  // ┼
    } else if (c->line == bit) {
      return;
    } else {
      setGlyph(*c, horizontal ? (dashed ? "\xE2\x94\x84" : "\xE2\x94\x80") : (dashed ? "\xE2\x94\x86" : "\xE2\x94\x82"));  // ┄ ─ ┆ │
    }
    c->line = static_cast<std::uint8_t>(c->line | bit);
    c->colour = paint(color);
  }

  /// base64 of u32 little-endian triplets per cell: codepoint, foreground, background
  std::string encode(const std::map<std::string, std::uint32_t>& rgb) const {
    std::string bytes;
    bytes.reserve(cells_.size() * 12);
    auto put32 = [&](uint32_t v) {
      for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    };
    for (const Cell& c : cells_) {
      uint32_t cp = 0x20;
      const std::string_view glyph = glyphOf(c);
      const unsigned char b0 = glyph.empty() ? 0 : static_cast<unsigned char>(glyph[0]);
      if (b0 < 0x80)
        cp = b0;
      else if (b0 >= 0xF0 && glyph.size() >= 4)
        cp = ((b0 & 0x07u) << 18) | ((glyph[1] & 0x3fu) << 12) | ((glyph[2] & 0x3fu) << 6) | (glyph[3] & 0x3fu);
      else if (b0 >= 0xE0 && glyph.size() >= 3)
        cp = ((b0 & 0x0fu) << 12) | ((glyph[1] & 0x3fu) << 6) | (glyph[2] & 0x3fu);
      else if (b0 >= 0xC0 && glyph.size() >= 2)
        cp = ((b0 & 0x1fu) << 6) | (glyph[1] & 0x3fu);
      const auto colour = rgb.find(palette_[c.colour]);
      put32(cp);
      put32(colour == rgb.end() ? 0x01000000u : colour->second);
      put32(0x01000000u);
    }
    static const char* const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
      const uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) | (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                         static_cast<unsigned char>(bytes[i + 2]);
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

  /// Another grid copied onto this one, its first row at `row`: the parts of a drawing, one under the other.
  void paste(const Grid& other, int row) {
    // the other grid's palette and long glyphs are its own: each cell is told again in this grid's
    std::vector<std::uint8_t> colours;
    for (const auto& c : other.palette_) colours.push_back(paint(c));
    for (int y = 0; y < other.h_ && row + y < h_; ++y)
      for (int x = 0; x < other.w_ && x < w_; ++x) {
        const Cell& from = other.cells_[static_cast<size_t>(y) * other.w_ + x];
        Cell& to = cells_[static_cast<size_t>(row + y) * w_ + x];
        to = from;
        to.colour = colours[from.colour];
        if (from.size == kLong) setGlyph(to, std::string(other.glyphOf(from)));
      }
  }

  std::string dump(bool color) const {
    std::string out;
    for (int y = 0; y < h_; ++y) {
      std::string row;
      std::string current;
      for (int x = 0; x < w_; ++x) {
        const Cell& c = cells_[static_cast<size_t>(y) * w_ + x];
        const std::string& colour = palette_[c.colour];
        if (color && colour != current) {
          row += colour.empty() ? "\x1b[0m" : "\x1b[" + colour + "m";
          current = colour;
        }
        row += glyphOf(c);
      }
      if (color && !current.empty()) row += "\x1b[0m";
      // trailing blanks are noise in a buffer
      const auto end = row.find_last_not_of(' ');
      out += (end == std::string::npos ? "" : row.substr(0, end + 1)) + "\n";
    }
    return out;
  }

 private:
  /// A drawing too large to hold as text is refused with what it would take, instead of exhausting the machine
  /// (DS-350): some 128 million characters, about 0.9 GB of cells, is the most it holds.
  static std::size_t sized(int w, int h) {
    const std::size_t cells = static_cast<std::size_t>(std::max(w, 0)) * static_cast<std::size_t>(std::max(h, 0));
    if (cells > kMaxCells)
      throw std::length_error("the drawing would take " + std::to_string(w) + " x " + std::to_string(h) +
                              " characters, too large for text: draw it as svg, or select less");
    return cells;
  }
  static constexpr std::size_t kMaxCells = std::size_t{128} * 1024 * 1024;
  /// The index of `color` in the palette, added the first time it is asked.
  std::uint8_t paint(const std::string& color) {
    for (std::size_t i = 0; i < palette_.size(); ++i)
      if (palette_[i] == color) return static_cast<std::uint8_t>(i);
    if (palette_.size() >= kLong) throw std::length_error("a text drawing of more than 254 colours");
    palette_.push_back(color);
    return static_cast<std::uint8_t>(palette_.size() - 1);
  }
  void setGlyph(Cell& c, const std::string& glyph) {
    if (glyph.size() <= c.glyph.size()) {
      std::copy(glyph.begin(), glyph.end(), c.glyph.begin());
      c.size = static_cast<std::uint8_t>(glyph.size());
      return;
    }
    const auto index = static_cast<std::uint32_t>(long_.size());
    long_.push_back(glyph);
    std::memcpy(c.glyph.data(), &index, sizeof index);
    c.size = kLong;
  }
  std::string_view glyphOf(const Cell& c) const {
    if (c.size != kLong) return std::string_view(c.glyph.data(), c.size);
    std::uint32_t index = 0;
    std::memcpy(&index, c.glyph.data(), sizeof index);
    return long_[index];
  }

  int minX_, minY_, w_, h_;
  std::vector<Cell> cells_;
  std::vector<std::string> palette_{""};  ///< the colours the cells name, "" first: the default
  std::vector<std::string> long_;         ///< the glyphs longer than one code point
};

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
void member(Grid& g, int x, int y, const std::string& line, const TextPen& pen) {
  const Metrics& M = pen.M;
  const std::string& kType = pen.type;
  const std::string& kPublic = pen.publicMember;
  const std::string& kPrivate = pen.privateMember;
  const std::string& kProtected = pen.protectedMember;
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

void box(Grid& g, const Placed& p, const TextPen& pen) {
  const Metrics& M = pen.M;
  const std::string& kMuted = pen.muted;
  const std::string& kName = pen.name;
  const std::string& kNote = pen.note;
  const IrNode& n = *p.node;
  const Rect& r = p.rect;
  clear(g, r);
  if (n.kind == "initial" || n.kind == "final") {  // a dot where the machine starts, a bullseye where it ends (DS-442)
    g.text(r.x + r.w / 2, r.y, n.kind == "initial" ? "\xE2\x97\x8F" : "\xE2\x97\x89", kName);  // ● ◉
    return;
  }
  if (n.kind == "note") {
    frame(g, r, false, kNote);
    const auto lines = text::linesOf(n.label);
    for (size_t i = 0; i < lines.size(); ++i) g.text(r.x + M.PAD_X, r.y + M.PAD_Y + static_cast<int>(i), lines[i], kNote);
    return;
  }
  const auto change = n.refs.find("change");
  const std::string changeColor = change == n.refs.end() ? "" : pen.changeMarkOf(change->second).colour;
  frame(g, r, true, changeColor);
  int y = r.y + M.PAD_Y;
  if (!n.stereotype.empty()) g.text(r.x + M.PAD_X, y++, "\xC2\xAB" + n.stereotype + "\xC2\xBB", kMuted);
  const std::string badge = badgeOf(n.classifier, n.abstract);  // the same letter as the SVG
  if (n.kind == "state") {                                      // a state carries no badge (DS-442)
  } else if (change != n.refs.end()) {
    const std::string mark = pen.changeMarkOf(change->second).mark;
    g.text(r.x + M.PAD_X, y, mark, changeColor);
  } else {
    g.text(r.x + M.PAD_X, y, badge, pen.badgeColour(n.classifier, n.abstract));
  }
  g.text(r.x + M.PAD_X + (n.kind == "state" ? 0 : M.BADGE_W), y, n.label, change == n.refs.end() ? kName : changeColor);
  ++y;
  // only class members are tokenised; an artefact's title is prose and stays plain
  const bool members = !n.sectionKinds.empty();
  for (size_t s = 0; s < n.sections.size(); ++s) {
    const auto& section = n.sections[s];
    g.put(r.x, y, "\xE2\x94\x9C");  // ├
    for (int x = r.x + 1; x < r.x + r.w - 1; ++x) g.put(x, y, "\xE2\x94\x80");
    g.put(r.x + r.w - 1, y, "\xE2\x94\xA4");  // ┤
    ++y;
    // what a closed box offers, all of one kind: each name with the letter a box of its own would carry (DS-244)
    const std::string offered = s < n.sectionKinds.size() && !holdsMembers(n.sectionKinds[s]) ? n.sectionKinds[s] : "";
    for (const auto& line : section) {
      if (!offered.empty()) {
        g.text(r.x + M.PAD_X, y, badgeOf(offered, false), pen.badgeColour(offered, false));
        g.text(r.x + M.PAD_X + M.BADGE_W, y++, line);
      } else if (members)
        member(g, r.x + M.PAD_X, y++, line, pen);
      else
        g.text(r.x + M.PAD_X, y++, line);
    }
  }
}

void container(Grid& g, const Placed& p, const TextPen& pen) {
  const std::string& kContainer = pen.container;
  const Rect& r = p.rect;
  frame(g, r, false, kContainer);
  g.text(r.x + 2, r.y, " " + p.node->label + " ", kContainer);
}

/// The marker glyph for an edge that enters its target moving in (dx, dy).
std::string marker(const std::string& kind, int dx, int dy) {
  if (kind == "inheritance" || kind == "realization")
    return dy < 0 ? "\xE2\x96\xB3" : dy > 0 ? "\xE2\x96\xBD" : dx < 0 ? "\xE2\x97\x81" : "\xE2\x96\xB7";  // △ ▽ ◁ ▷
  if (kind == "composition") return "\xE2\x97\x86";                                                       // ◆
  if (kind == "aggregation") return "\xE2\x97\x87";                                                       // ◇
  if (kind == "attachment") return "";
  return dy < 0 ? "\xE2\x96\xB2" : dy > 0 ? "\xE2\x96\xBC" : dx < 0 ? "\xE2\x97\x80" : "\xE2\x96\xB6";  // ▲ ▼ ◀ ▶
}

void edge(Grid& g, const IrEdge& e, Route route, const std::map<std::string, const Placed*>& byId, const TextPen& pen) {
  const std::string& kMuted = pen.muted;
  const std::string& kEdge = pen.edge;
  if (route.points.size() < 2) return;
  const bool diamond = e.kind == "composition" || e.kind == "aggregation";
  const auto change = e.refs.find("change");
  const bool removed = change != e.refs.end() && change->second == "removed";
  const bool dashed = removed || e.kind == "dependency" || e.kind == "realization" || e.kind == "attachment";
  const std::string edgeColor = change == e.refs.end() ? std::string(kEdge) : pen.changeMarkOf(change->second).colour;
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
        if (up && right)
          glyph = "\xE2\x94\x94";  // └
        else if (up && left)
          glyph = "\xE2\x94\x98";  // ┘
        else if (down && right)
          glyph = "\xE2\x94\x8C";  // ┌
        else if (down && left)
          glyph = "\xE2\x94\x90";  // ┐
        g.put(x, y, glyph, edgeColor);
      } else {
        g.line(x, y, horizontal, dashed, edgeColor);
      }
    }
  }
  // the head: on the target end, or on the source end for a diamond (the whole holds it)
  const Point tail = pts.front(), head = pts.back();
  const Point before = pts[pts.size() - 2];
  if (diamond) {
    g.put(tail.x, tail.y, marker(e.kind, 0, 0), edgeColor);
    const std::string m = marker("association", head.x - before.x, head.y - before.y);
    if (e.label.empty()) (void)m;
  } else {
    const std::string m = marker(e.kind, head.x - before.x, head.y - before.y);
    if (!m.empty()) g.put(head.x, head.y, m, edgeColor);
  }
  if (!e.label.empty() || change != e.refs.end()) {
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
    const auto lines = text::linesOf(change == e.refs.end() ? e.label : pen.changeMarkOf(change->second).mark + std::string(" ") + e.label);
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

Grid sequenceGrid(const IrDocument& doc, const TextPen& pen) {
  const std::string& kMuted = pen.muted;
  const std::string& kName = pen.name;
  const std::string& kEdge = pen.edge;
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
  // messages; a diff marks them: + added, - removed, ~ changed
  for (const IrEdge* m : messages) {
    const int y = rowOf[m->order] + 1;
    const int a = lines[index[m->from]].centre, b = lines[index[m->to]].centre;
    const auto change = m->refs.find("change");
    const std::string colour = change == m->refs.end() ? std::string(kEdge) : pen.changeMarkOf(change->second).colour;
    const std::string labelColour = change == m->refs.end() ? std::string(kMuted) : colour;
    const std::string label = change == m->refs.end() ? m->label : pen.changeMarkOf(change->second).mark + std::string(" ") + m->label;
    const std::string dash =
        m->dashed || (change != m->refs.end() && change->second == "removed") ? "\xE2\x94\x84" : "\xE2\x94\x80";  // ┄ ─
    if (a == b) {
      g.text(a + 1, y, dash + dash + "\xE2\x94\x90", colour);                // ──┐
      g.text(a + 1, y + 1, "\xE2\x97\x80" + dash + "\xE2\x94\x98", colour);  // ◀─┘
      g.text(a + 5, y, label, labelColour);
      continue;
    }
    const int lo = std::min(a, b), hi = std::max(a, b);
    for (int xx = lo + 1; xx < hi; ++xx) g.put(xx, y, dash, colour);
    if (b > a)
      g.put(hi - 1, y, "\xE2\x96\xB6", colour);  // ▶
    else
      g.put(lo + 1, y, "\xE2\x97\x80", colour);  // ◀
    g.text((lo + hi) / 2 - static_cast<int>(label.size()) / 2, y - 1, label, labelColour);
  }
  return g;
}

}  // namespace

namespace {

Grid classGrid(const IrDocument& doc, const LayoutResult& lay, const TextPen& pen) {
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
  std::stable_sort(frames.begin(), frames.end(),
                   [](const Placed* a, const Placed* b) { return a->rect.w * a->rect.h > b->rect.w * b->rect.h; });
  for (const Placed* f : frames) container(g, *f, pen);
  for (size_t i = 0; i < doc.edges.size(); ++i) {
    const auto r = lay.routes.find(i);
    if (r != lay.routes.end()) edge(g, doc.edges[i], r->second, byId, pen);
  }
  for (const auto& p : lay.placed) {
    if (p.node->kind != "container") box(g, p, pen);
  }
  return g;
}

bool isSequence(const IrDocument& doc) {
  for (const auto& n : doc.nodes) {
    if (n.kind == "lifeline") return true;
  }
  return false;
}

/// The grid keeps a one-cell margin on each side: the layout gets the rest.
int contentWidth(int maxWidth) { return maxWidth > 0 ? std::max(10, maxWidth - 2) : 0; }

}  // namespace

namespace {
/// The parts of a drawing (DS-245), one under the other with a line between: a terminal reads down.
// A drawing that is a table (DS-251): a row of headings, a rule under it, and a row for each of its rows; a
// cell against the machine is marked with a bang.
Grid tableGrid(const IrDocument& doc, const TextPen& pen) {
  const std::string& kMuted = pen.muted;
  const std::string& kAgainst = pen.against;
  const IrTable& t = doc.table;
  const auto shown = [](const IrCell& cell) {
    return cell.kind == "against" ? "! " + cell.text : cell.kind == "none" ? "\xC2\xB7" : cell.text;
  };
  const auto wide = [](const std::string& s) {  // in cells: one for each UTF-8 code point
    return static_cast<int>(text::characters(s));
  };
  std::vector<int> widths{wide(t.corner)};
  for (const IrRow& row : t.rows) widths[0] = std::max(widths[0], wide(row.label));
  for (std::size_t c = 0; c < t.columns.size(); ++c) {
    int longest = wide(t.columns[c]);
    for (const IrRow& row : t.rows)
      if (c < row.cells.size()) longest = std::max(longest, wide(shown(row.cells[c])));
    widths.push_back(longest);
  }
  int width = 1;
  for (const int w : widths) width += w + 3;
  Grid g(0, 0, width, static_cast<int>(t.rows.size()) + 2);
  int x = 1;
  g.text(x, 0, t.corner);
  for (std::size_t c = 0; c < t.columns.size(); ++c) {
    x += widths[c] + 3;
    g.put(x - 2, 0, "\xE2\x94\x82", kMuted);  // │
    g.text(x, 0, t.columns[c]);
  }
  for (int i = 0; i < width; ++i) g.put(i, 1, "\xE2\x94\x80", kMuted);  // ─
  x = 1;
  for (std::size_t c = 0; c < t.columns.size(); ++c) {
    x += widths[c] + 3;
    g.put(x - 2, 1, "\xE2\x94\xBC", kMuted);  // ┼
  }
  for (std::size_t r = 0; r < t.rows.size(); ++r) {
    const int y = static_cast<int>(r) + 2;
    x = 1;
    g.text(x, y, t.rows[r].label);
    for (std::size_t c = 0; c < t.columns.size() && c < t.rows[r].cells.size(); ++c) {
      x += widths[c] + 3;
      g.put(x - 2, y, "\xE2\x94\x82", kMuted);  // │
      const IrCell& cell = t.rows[r].cells[c];
      g.text(x, y, shown(cell), cell.kind == "against" ? kAgainst : cell.kind == "none" ? kMuted : "");
    }
  }
  return g;
}

Grid partsGrid(const IrDocument& doc, int maxWidth, const TextPen& pen) {
  const Metrics& M = pen.M;
  std::vector<Grid> grids;
  int w = 1, h = 0;
  for (const auto& name : doc.parts) {
    const IrDocument part = doc.part(name);
    const IrDocument bundled = part.bundled();
    grids.push_back(isSequence(part) ? sequenceGrid(part, pen) : classGrid(bundled, Layout(M).place(bundled, contentWidth(maxWidth)), pen));
    w = std::max(w, grids.back().columns());
    h += grids.back().rows() + (h ? 1 : 0);
  }
  Grid all(0, 0, w, std::max(1, h));
  int row = 0;
  for (const auto& g : grids) {
    all.paste(g, row);
    row += g.rows() + 1;
  }
  return all;
}
}  // namespace

Text::Text(const theme::Theme& theme) : pen_(theme) {}

std::string Text::render(const IrDocument& doc, bool color, int maxWidth) const {
  if (!doc.table.columns.empty()) return tableGrid(doc, pen_).dump(color);
  if (!doc.parts.empty()) return partsGrid(doc, maxWidth, pen_).dump(color);
  if (isSequence(doc)) return sequenceGrid(doc, pen_).dump(color);
  const IrDocument bundled = doc.bundled();
  return render(bundled, Layout(pen_.M).place(bundled, contentWidth(maxWidth)), color);
}

std::string Text::render(const IrDocument& doc, const LayoutResult& lay, bool color) const {
  if (isSequence(doc)) return sequenceGrid(doc, pen_).dump(color);
  return classGrid(doc, lay, pen_).dump(color);
}

std::string Text::cells(const IrDocument& doc, int maxWidth) const {
  const IrDocument bundled = doc.bundled();
  const Grid g = !doc.table.columns.empty() ? tableGrid(doc, pen_)
                 : !doc.parts.empty()       ? partsGrid(doc, maxWidth, pen_)
                 : isSequence(doc)          ? sequenceGrid(doc, pen_)
                                            : classGrid(bundled, Layout(pen_.M).place(bundled, contentWidth(maxWidth)), pen_);
  nlohmann::json out = {{"columns", g.columns()}, {"rows", g.rows()}, {"cells", g.encode(pen_.cellColours)}, {"text", g.dump(false)}};
  return out.dump();
}

}  // namespace render
}  // namespace ds
