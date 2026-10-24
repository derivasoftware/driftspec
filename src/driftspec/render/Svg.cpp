#include "driftspec/render/Svg.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <sstream>

namespace driftspec {
namespace render {

namespace {

using M = Metrics;

// The stylesheet of plantuml-render's engine (shared.ts STYLE), verbatim: one theming contract.
const char* const kStyle = R"CSS(
  .pr-diagram { font-family: var(--pr-font, "JetBrains Mono", "Cascadia Code", "SFMono-Regular", Menlo, Consolas, ui-monospace, monospace); font-size: 12px;
    --pr-text: #1e293b; --pr-muted: #64748b; --pr-stroke: #64748b; --pr-edge: #64748b; --pr-box-fill: #ffffff; --pr-container-fill: #f8fafc; --pr-container-stroke: #94a3b8;
    --pr-note-fill: #fffbeb; --pr-note-stroke: #fcd34d; --pr-head-class: #eef2ff; --pr-head-interface: #ecfdf5; --pr-head-enum: #fff7ed; --pr-head-function: #f1f5f9; --pr-head-abstract: #f5f3ff;
    --pr-badge-class: #4f46e5; --pr-badge-interface: #059669; --pr-badge-enum: #d97706; --pr-badge-function: #475569; --pr-badge-abstract: #7c3aed;
    --pr-name: #0f172a; --pr-type: #2563eb; --pr-param: #475569; --pr-punct: #94a3b8;
    --pr-vis-public: #16a34a; --pr-vis-private: #dc2626; --pr-vis-protected: #d97706; --pr-vis-package: #2563eb;
    --pr-port-in: #2563eb; --pr-port-out: #c2730a; --pr-focus: #c2730a; --pr-dimmed: .25;
    --pr-stroke-width: 1; --pr-container-stroke-width: 1; --pr-edge-width: 1; --pr-edge-hit: 12; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-text: #e2e8f0; --pr-muted: #94a3b8; --pr-stroke: #94a3b8; --pr-edge: #94a3b8; --pr-box-fill: #1e293b; --pr-container-fill: #0f172a; --pr-container-stroke: #64748b;
    --pr-note-fill: #3b3418; --pr-note-stroke: #a16207; --pr-head-class: #312e81; --pr-head-interface: #064e3b; --pr-head-enum: #78350f; --pr-head-function: #334155; --pr-head-abstract: #4c1d95;
    --pr-badge-class: #a5b4fc; --pr-badge-interface: #6ee7b7; --pr-badge-enum: #fcd34d; --pr-badge-function: #cbd5e1; --pr-badge-abstract: #c4b5fd;
    --pr-name: #f8fafc; --pr-type: #7dd3fc; --pr-param: #cbd5e1; --pr-punct: #64748b; --pr-vis-public: #4ade80; --pr-vis-private: #f87171; --pr-vis-protected: #fbbf24; --pr-vis-package: #60a5fa; --pr-port-in: #60a5fa; --pr-port-out: #fbbf24; --pr-focus: #fbbf24; --pr-dimmed: .3; } }
  [data-theme="dark"] .pr-diagram { --pr-text: #e2e8f0; --pr-muted: #94a3b8; --pr-stroke: #94a3b8; --pr-edge: #94a3b8; --pr-box-fill: #1e293b; --pr-container-fill: #0f172a; --pr-container-stroke: #64748b;
    --pr-note-fill: #3b3418; --pr-note-stroke: #a16207; --pr-head-class: #312e81; --pr-head-interface: #064e3b; --pr-head-enum: #78350f; --pr-head-function: #334155; --pr-head-abstract: #4c1d95;
    --pr-badge-class: #a5b4fc; --pr-badge-interface: #6ee7b7; --pr-badge-enum: #fcd34d; --pr-badge-function: #cbd5e1; --pr-badge-abstract: #c4b5fd;
    --pr-name: #f8fafc; --pr-type: #7dd3fc; --pr-param: #cbd5e1; --pr-punct: #64748b; --pr-vis-public: #4ade80; --pr-vis-private: #f87171; --pr-vis-protected: #fbbf24; --pr-vis-package: #60a5fa; --pr-port-in: #60a5fa; --pr-port-out: #fbbf24; --pr-focus: #fbbf24; --pr-dimmed: .3; }
  .pr-diagram text { fill: var(--pr-text); }
  .pr-diagram rect, .pr-diagram line.pr-sep { shape-rendering: crispEdges; }
  .pr-diagram rect, .pr-diagram path, .pr-diagram line { vector-effect: non-scaling-stroke; }
  .pr-diagram .pr-box > rect { fill: var(--pr-box-fill); stroke: var(--pr-stroke); stroke-width: var(--pr-stroke-width); }
  .pr-diagram .pr-head { fill: var(--pr-head-class); stroke: none; }
  .pr-diagram .pr-classifier-interface .pr-head { fill: var(--pr-head-interface); }
  .pr-diagram .pr-classifier-enum .pr-head { fill: var(--pr-head-enum); }
  .pr-diagram .pr-abstract .pr-head { fill: var(--pr-head-abstract); }
  .pr-diagram .pr-badge { fill: var(--pr-badge-class); }
  .pr-diagram .pr-classifier-interface .pr-badge { fill: var(--pr-badge-interface); }
  .pr-diagram .pr-classifier-enum .pr-badge { fill: var(--pr-badge-enum); }
  .pr-diagram .pr-abstract .pr-badge { fill: var(--pr-badge-abstract); }
  .pr-diagram .pr-badge-text { fill: var(--pr-box-fill); font-size: 9.5px; font-weight: 700; }
  .pr-diagram .pr-header { font-weight: 700; fill: var(--pr-name); }
  .pr-diagram .pr-abstract .pr-header { font-style: italic; }
  .pr-diagram .pr-stereo { fill: var(--pr-muted); font-size: 10.5px; }
  .pr-diagram .pr-sep { stroke: var(--pr-stroke); }
  .pr-diagram .pr-name { fill: var(--pr-name); }
  .pr-diagram .pr-type { fill: var(--pr-type); }
  .pr-diagram .pr-param { fill: var(--pr-param); }
  .pr-diagram .pr-punct { fill: var(--pr-punct); }
  .pr-diagram .pr-static { text-decoration: underline; }
  .pr-diagram .pr-abstract-member { font-style: italic; }
  .pr-diagram .pr-vis-public { fill: var(--pr-vis-public); }
  .pr-diagram .pr-vis-private { fill: var(--pr-vis-private); }
  .pr-diagram .pr-vis-protected { fill: var(--pr-vis-protected); }
  .pr-diagram .pr-vis-package { fill: var(--pr-vis-package); }
  .pr-diagram .pr-container > rect { fill: var(--pr-container-fill); stroke: var(--pr-container-stroke); stroke-width: var(--pr-container-stroke-width); }
  .pr-diagram .pr-container > .pr-header { fill: var(--pr-muted); font-weight: 600; font-size: 11px; letter-spacing: .4px; }
  .pr-diagram .pr-note path { fill: var(--pr-note-fill); stroke: var(--pr-note-stroke); }
  .pr-diagram .pr-notice rect { fill: var(--pr-container-fill); stroke: var(--pr-muted); stroke-dasharray: 5 3; }
  .pr-diagram .pr-notice text { fill: var(--pr-muted); font-style: italic; }
  .pr-diagram .pr-edge { stroke: var(--pr-edge); stroke-width: var(--pr-edge-width); fill: none; pointer-events: none; }
  .pr-diagram .pr-edge-hit { stroke: transparent; stroke-width: var(--pr-edge-hit, 12); fill: none; pointer-events: stroke; }
  .pr-diagram .pr-edge-realization, .pr-diagram .pr-edge-dependency, .pr-diagram .pr-edge-attachment { stroke-dasharray: 6 4; }
  .pr-diagram .pr-edge-label { fill: var(--pr-muted); font-size: 11px; paint-order: stroke; stroke: var(--pr-box-fill); stroke-width: 3px; stroke-linejoin: round; }
  .pr-diagram marker path { fill: var(--pr-box-fill); stroke: var(--pr-edge); }
  .pr-diagram marker .pr-filled { fill: var(--pr-edge); }
  .pr-diagram marker .pr-open { fill: none; }
)CSS";

std::string esc(const std::string& text) {
  std::string out;
  for (char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
  return out;
}

/// FNV-1a of the title in base 36, as shared.ts idPrefix: several SVGs share one page.
std::string idPrefix(const std::string& title) {
  uint32_t h = 0x811c9dc5u;
  for (unsigned char c : title) {
    h ^= c;
    h *= 0x01000193u;
  }
  std::string digits;
  if (h == 0) digits = "0";
  while (h > 0) {
    const uint32_t d = h % 36;
    digits.insert(digits.begin(), static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10));
    h /= 36;
  }
  return "pr" + digits + "-";
}

std::string markers(const std::string& px) {
  return "\n  <marker id=\"" + px + "tri\" viewBox=\"0 0 14 12\" refX=\"13\" refY=\"6\" markerWidth=\"14\" markerHeight=\"12\" orient=\"auto\"><path d=\"M1,1 L13,6 L1,11 Z\"/></marker>"
         "\n  <marker id=\"" + px + "diamond-filled\" viewBox=\"0 0 16 10\" refX=\"15\" refY=\"5\" markerWidth=\"16\" markerHeight=\"10\" orient=\"auto\"><path class=\"pr-filled\" d=\"M1,5 L8,1 L15,5 L8,9 Z\"/></marker>"
         "\n  <marker id=\"" + px + "diamond\" viewBox=\"0 0 16 10\" refX=\"15\" refY=\"5\" markerWidth=\"16\" markerHeight=\"10\" orient=\"auto\"><path d=\"M1,5 L8,1 L15,5 L8,9 Z\"/></marker>"
         "\n  <marker id=\"" + px + "arrow\" viewBox=\"0 0 12 12\" refX=\"11\" refY=\"6\" markerWidth=\"12\" markerHeight=\"12\" orient=\"auto\"><path class=\"pr-open\" d=\"M1,1 L11,6 L1,11\"/></marker>\n";
}

const std::map<std::string, std::string> kMarkerByKind = {
    {"inheritance", "tri"}, {"realization", "tri"}, {"composition", "diamond-filled"}, {"aggregation", "diamond"}, {"dependency", "arrow"}, {"association", "arrow"}};

const std::map<std::string, std::string> kBadge = {{"class", "C"}, {"interface", "I"}, {"enum", "E"}, {"dataclass", "D"}, {"function", "f"}, {"struct", "S"}};

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

std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(' ');
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

/// Split on top-level commas: generics and parentheses stay whole.
std::vector<std::string> splitParams(const std::string& text) {
  std::vector<std::string> out;
  int depth = 0;
  std::string current;
  for (char c : text) {
    if (c == '(' || c == '[' || c == '<' || c == '{') ++depth;
    if (c == ')' || c == ']' || c == '>' || c == '}') --depth;
    if (c == ',' && depth == 0) {
      out.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!trim(current).empty()) out.push_back(current);
  return out;
}

std::string typed(const std::string& type) { return "<tspan class=\"pr-type\">" + esc(trim(type)) + "</tspan>"; }

/// One member line of `ds render` (`+ abstract run(model: Model) -> list<Finding>`,
/// `- items: list<T> = 0`) tokenised into the engine's syntax-coloured tspans.
std::pair<std::string, std::string> memberMarkup(const std::string& line) {
  static const std::regex head(R"(^\s*([+\-#~])?\s*((?:(?:abstract|static|const)\s+)*)(.*)$)");
  static const std::regex method(R"(^([^\s(]+)\s*\((.*)\)\s*(?:->\s*(.+))?$)");
  static const std::regex attribute(R"(^([^\s:]+)\s*:\s*([^=]+?)(?:\s*=\s*(.+))?$)");
  static const std::map<char, std::string> visibility = {{'+', "public"}, {'-', "private"}, {'#', "protected"}, {'~', "package"}};
  std::smatch m;
  if (!std::regex_match(line, m, head)) return {"", esc(line)};
  const std::string vis = m[1].matched ? visibility.at(m[1].str()[0]) : "";
  const std::string modifiers = m[2].str();
  const std::string body = trim(m[3].str());
  std::string nameClass = "pr-name";
  if (modifiers.find("static") != std::string::npos) nameClass += " pr-static";
  if (modifiers.find("abstract") != std::string::npos) nameClass += " pr-abstract-member";
  std::smatch mm;
  if (std::regex_match(body, mm, method)) {
    std::string params;
    for (const auto& p : splitParams(mm[2].str())) {
      if (!params.empty()) params += "<tspan class=\"pr-punct\">, </tspan>";
      const auto colon = p.find(':');
      params += colon == std::string::npos ? "<tspan class=\"pr-param\">" + esc(trim(p)) + "</tspan>"
                                           : "<tspan class=\"pr-param\">" + esc(trim(p.substr(0, colon))) + "</tspan><tspan class=\"pr-punct\">: </tspan>" + typed(p.substr(colon + 1));
    }
    const std::string returns = mm[3].matched ? "<tspan class=\"pr-punct\"> -> </tspan>" + typed(mm[3].str()) : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + esc(mm[1].str()) + "</tspan><tspan class=\"pr-punct\">(</tspan>" + params + "<tspan class=\"pr-punct\">)</tspan>" + returns};
  }
  if (std::regex_match(body, mm, attribute)) {
    const std::string def = mm[3].matched ? "<tspan class=\"pr-punct\"> = </tspan><tspan class=\"pr-param\">" + esc(trim(mm[3].str())) + "</tspan>" : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + esc(mm[1].str()) + "</tspan><tspan class=\"pr-punct\"> : </tspan>" + typed(mm[2].str()) + def};
  }
  return {vis, "<tspan class=\"" + nameClass + "\">" + esc(body) + "</tspan>"};
}

std::string num(int v) { return std::to_string(v); }

std::string emitNode(const Placed& p, const std::string& px) {
  const IrNode& n = *p.node;
  const Rect& r = p.rect;
  std::string classes = "pr-" + n.kind;
  if (!n.classifier.empty()) classes += " pr-classifier-" + n.classifier;
  if (!n.stereotype.empty()) {
    std::string st;
    for (char c : n.stereotype) st += std::isalnum(static_cast<unsigned char>(c)) ? c : '-';
    classes += " pr-stereotype-" + st;
  }
  if (n.abstract) classes += " pr-abstract";
  std::ostringstream out;
  out << "<g id=\"" << px << esc(n.id) << "\" data-id=\"" << esc(n.id) << "\" class=\"" << classes << "\">";
  if (!n.title.empty()) out << "<title>" << esc(n.title) << "</title>";
  if (n.kind == "container") {
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"6\"/>";
    out << "<text class=\"pr-header\" x=\"" << r.x + 10 << "\" y=\"" << r.y + 15 << "\">" << esc(n.label) << "</text>";
  } else if (n.kind == "note") {
    const int f = M::NOTE_FOLD;
    out << "<path d=\"M" << r.x << "," << r.y << " H" << r.x + r.w - f << " L" << r.x + r.w << "," << r.y + f << " V" << r.y + r.h << " H" << r.x << " Z\"/>";
    out << "<path d=\"M" << r.x + r.w - f << "," << r.y << " V" << r.y + f << " H" << r.x + r.w << "\"/>";
    const auto lines = linesOf(n.label);
    for (size_t i = 0; i < lines.size(); ++i) {
      out << "<text x=\"" << r.x + M::PAD << "\" y=\"" << r.y + M::PAD + 12 + static_cast<int>(i) * M::LINE_H << "\">" << esc(lines[i]) << "</text>";
    }
  } else {
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"4\"/>";
    int ty = r.y + M::PAD + 12;
    const int rad = 4;
    const int bandH = n.sections.empty() ? r.h : M::PAD + 12 + (n.stereotype.empty() ? 0 : M::LINE_H) + M::SECTION_GAP + 6;
    out << "<path class=\"pr-head\" d=\"M" << r.x + rad << "," << r.y << " H" << r.x + r.w - rad << " A" << rad << "," << rad << " 0 0 1 " << r.x + r.w << "," << r.y + rad
        << " V" << r.y + bandH << " H" << r.x << " V" << r.y + rad << " A" << rad << "," << rad << " 0 0 1 " << r.x + rad << "," << r.y << " Z\"/>";
    if (!n.stereotype.empty()) {
      out << "<text class=\"pr-stereo\" x=\"" << r.x + M::PAD << "\" y=\"" << ty - 2 << "\">" << esc("\xC2\xAB" + n.stereotype + "\xC2\xBB") << "</text>";
      ty += M::LINE_H;
    }
    std::string letter = "C";
    if (n.abstract && n.classifier.empty()) {
      letter = "A";
    } else if (!n.classifier.empty()) {
      const auto b = kBadge.find(n.classifier);
      letter = b != kBadge.end() ? b->second : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(n.classifier[0]))));
    }
    out << "<circle class=\"pr-badge\" cx=\"" << r.x + M::PAD + 7 << "\" cy=\"" << ty - 4 << "\" r=\"7\"/>";
    out << "<text class=\"pr-badge-text\" x=\"" << r.x + M::PAD + 7 << "\" y=\"" << ty - 0.5 << "\" text-anchor=\"middle\">" << esc(letter) << "</text>";
    out << "<text class=\"pr-header\" x=\"" << r.x + M::PAD + M::BADGE_W << "\" y=\"" << ty << "\">" << esc(n.label) << "</text>";
    ty += M::LINE_H;
    for (const auto& section : n.sections) {
      const int sepY = ty - M::LINE_H + M::SECTION_GAP + 6;
      out << "<line class=\"pr-sep\" x1=\"" << r.x << "\" y1=\"" << sepY << "\" x2=\"" << r.x + r.w << "\" y2=\"" << sepY << "\"/>";
      ty += M::SECTION_GAP;
      for (const auto& line : section) {
        const auto marked = memberMarkup(line);
        if (!marked.first.empty()) {
          out << "<circle class=\"pr-vis pr-vis-" << marked.first << "\" cx=\"" << r.x + M::PAD + 4 << "\" cy=\"" << ty - 4 << "\" r=\"3.5\"/>";
        }
        out << "<text x=\"" << r.x + M::PAD + (marked.first.empty() ? 0 : M::DOT_W) << "\" y=\"" << ty << "\">" << marked.second << "</text>";
        ty += M::LINE_H;
      }
    }
  }
  out << "</g>";
  const std::string g = out.str();
  return n.href.empty() ? g : "<a href=\"" + esc(n.href) + "\" class=\"pr-link\">" + g + "</a>";
}

struct EdgeMarkup {
  std::string path;
  std::string label;
};

EdgeMarkup emitEdge(const IrEdge& e, const Route& route, const std::string& px) {
  std::vector<Point> points = route.points;
  // the diamond sits on the whole, the edge's source: draw from the part so the end marker lands there
  if (e.kind == "composition" || e.kind == "aggregation") std::reverse(points.begin(), points.end());
  std::string d;
  for (size_t i = 0; i < points.size(); ++i) d += (i == 0 ? "M" : " L") + num(points[i].x) + "," + num(points[i].y);
  const auto marker = kMarkerByKind.find(e.kind);
  const std::string markerAttr = marker == kMarkerByKind.end() ? "" : " marker-end=\"url(#" + px + marker->second + ")\"";
  EdgeMarkup out;
  if (!e.label.empty() && points.size() >= 2) {
    // beside the midpoint of the longest segment
    size_t best = 0;
    int bestLen = -1;
    for (size_t i = 1; i < points.size(); ++i) {
      const int len = std::abs(points[i].x - points[i - 1].x) + std::abs(points[i].y - points[i - 1].y);
      if (len > bestLen) {
        bestLen = len;
        best = i - 1;
      }
    }
    const Point a = points[best];
    const Point b = points[best + 1];
    const int x = (a.x + b.x) / 2 + 6;
    const int y = (a.y + b.y) / 2 - 4;
    const auto lines = linesOf(e.label);
    std::string rows;
    for (size_t i = 0; i < lines.size(); ++i) rows += "<tspan x=\"" + num(x) + "\" dy=\"" + num(i == 0 ? 0 : M::LABEL_H) + "\">" + esc(lines[i]) + "</tspan>";
    out.label = "<text class=\"pr-edge-label\" x=\"" + num(x) + "\" y=\"" + num(y) + "\">" + (lines.size() == 1 ? esc(e.label) : rows) + "</text>";
  }
  out.path = "<g class=\"pr-edge-group\" data-from=\"" + esc(e.from) + "\" data-to=\"" + esc(e.to) + "\">" + (e.title.empty() ? "" : "<title>" + esc(e.title) + "</title>") +
             "<path class=\"pr-edge-hit\" d=\"" + d + "\"/><path class=\"pr-edge pr-edge-" + e.kind + "\" d=\"" + d + "\"" + markerAttr + "/></g>";
  if (!e.href.empty()) out.path = "<a href=\"" + esc(e.href) + "\" class=\"pr-link\">" + out.path + "</a>";
  return out;
}

}  // namespace

std::string Svg::render(const IrDocument& doc) const {
  const IrDocument bundled = doc.bundled();
  return render(bundled, layout(bundled));
}

std::string Svg::render(const IrDocument& doc, const LayoutResult& lay) const {
  int minX = std::numeric_limits<int>::max(), minY = minX, maxX = std::numeric_limits<int>::min(), maxY = maxX;
  auto extend = [&](int x, int y) {
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
  };
  for (const auto& p : lay.placed) {
    extend(p.rect.x, p.rect.y);
    extend(p.rect.x + p.rect.w, p.rect.y + p.rect.h);
  }
  for (const auto& r : lay.routes) {
    for (const auto& pt : r.second.points) extend(pt.x, pt.y);
  }
  if (lay.placed.empty()) extend(0, 0), extend(10, 10);

  const std::string px = idPrefix(doc.title);
  std::vector<std::string> containers, leaves;
  std::map<std::string, const Placed*> byId;
  for (const auto& p : lay.placed) byId[p.node->id] = &p;
  // paint order: outer containers first, then edges, then leaves, then labels
  std::vector<const Placed*> frames;
  for (const auto& p : lay.placed) {
    if (p.node->kind == "container") frames.push_back(&p);
  }
  std::stable_sort(frames.begin(), frames.end(), [&](const Placed* a, const Placed* b) {
    int da = 0, db = 0;
    for (std::string s = a->node->parent; !s.empty() && byId.count(s); s = byId[s]->node->parent) ++da;
    for (std::string s = b->node->parent; !s.empty() && byId.count(s); s = byId[s]->node->parent) ++db;
    return da < db;
  });
  for (const Placed* f : frames) containers.push_back(emitNode(*f, px));
  for (const auto& p : lay.placed) {
    if (p.node->kind != "container") leaves.push_back(emitNode(p, px));
  }
  std::vector<std::string> paths, labels, missing;
  for (size_t i = 0; i < doc.edges.size(); ++i) {
    const auto r = lay.routes.find(i);
    if (r == lay.routes.end()) {
      for (const std::string& end : {doc.edges[i].from, doc.edges[i].to}) {
        if (!byId.count(end) && std::find(missing.begin(), missing.end(), end) == missing.end()) missing.push_back(end);
      }
      continue;
    }
    EdgeMarkup m = emitEdge(doc.edges[i], r->second, px);
    paths.push_back(m.path);
    if (!m.label.empty()) labels.push_back(m.label);
  }
  std::string notice = doc.notice;
  if (!missing.empty()) {
    std::string names;
    for (size_t i = 0; i < missing.size() && i < 4; ++i) names += (i ? ", " : "") + missing[i];
    if (missing.size() > 4) names += " and " + num(static_cast<int>(missing.size()) - 4) + " more";
    notice += (notice.empty() ? "" : "\n") + std::string("Relations to nodes not in this document: ") + names;
  }
  std::string noticeBlock;
  if (!notice.empty()) {
    const auto lines = linesOf(notice);
    size_t width = 0;
    for (const auto& l : lines) width = std::max(width, l.size());
    const int w = static_cast<int>(width) * M::CHAR_W + 2 * M::PAD;
    const int h = static_cast<int>(lines.size()) * M::LINE_H + 2 * M::PAD - 4;
    const int x = minX;
    const int y = maxY + 2 * M::PAD;
    noticeBlock = "<g class=\"pr-notice\"><rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" rx=\"3\"/>";
    for (size_t i = 0; i < lines.size(); ++i) noticeBlock += "<text x=\"" + num(x + M::PAD) + "\" y=\"" + num(y + M::PAD + 4 + static_cast<int>(i) * M::LINE_H) + "\">" + esc(lines[i]) + "</text>";
    noticeBlock += "</g>";
    extend(x + w, y + h);
  }
  const int vx = minX - M::PAD, vy = minY - M::PAD, vw = maxX + M::PAD - vx, vh = maxY + M::PAD - vy;
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << vx << " " << vy << " " << vw << " " << vh << "\" width=\"" << vw << "\" height=\"" << vh
      << "\" style=\"max-width:100%;height:auto\" class=\"pr-diagram\" role=\"img\">\n";
  if (!doc.title.empty()) out << "<title>" << esc(doc.title) << "</title>\n";
  out << "<style>" << kStyle << "</style>\n<defs>" << markers(px) << "</defs>\n";
  for (const auto& s : containers) out << s << "\n";
  for (const auto& s : paths) out << s << "\n";
  for (const auto& s : leaves) out << s << "\n";
  for (const auto& s : labels) out << s << "\n";
  if (!noticeBlock.empty()) out << noticeBlock << "\n";
  out << "</svg>\n";
  return out.str();
}

}  // namespace render
}  // namespace driftspec
