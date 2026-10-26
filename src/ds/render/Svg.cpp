#include "ds/render/Svg.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

namespace ds {
namespace render {

namespace {

const Metrics M = Metrics::svg();

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
  .pr-diagram .pr-classifier-requirement .pr-head { fill: var(--pr-head-function); }
  .pr-diagram .pr-classifier-requirement .pr-badge { fill: var(--pr-badge-function); }
  .pr-diagram .pr-classifier-verification .pr-head { fill: var(--pr-head-interface); }
  .pr-diagram .pr-classifier-verification .pr-badge { fill: var(--pr-badge-interface); }
  .pr-diagram .pr-classifier-constraint .pr-head { fill: var(--pr-head-enum); }
  .pr-diagram .pr-classifier-constraint .pr-badge { fill: var(--pr-badge-enum); }
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
  .pr-diagram { --pr-added: #15803d; --pr-removed: #b91c1c; --pr-changed: #b45309; --pr-renamed: #1d4ed8; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-added: #4ade80; --pr-removed: #f87171; --pr-changed: #fbbf24; --pr-renamed: #93c5fd; } }
  [data-theme="dark"] .pr-diagram { --pr-added: #4ade80; --pr-removed: #f87171; --pr-changed: #fbbf24; --pr-renamed: #93c5fd; }
  .pr-diagram .pr-change-added > rect { stroke: var(--pr-added); stroke-width: 2; }
  .pr-diagram .pr-change-added .pr-header { fill: var(--pr-added); }
  .pr-diagram .pr-change-removed > rect { stroke: var(--pr-removed); stroke-dasharray: 5 3; }
  .pr-diagram .pr-change-removed text { fill: var(--pr-removed); opacity: .8; }
  .pr-diagram .pr-change-removed .pr-head { fill: none; }
  .pr-diagram .pr-change-changed > rect { stroke: var(--pr-changed); stroke-width: 2; }
  .pr-diagram .pr-change-changed .pr-header { fill: var(--pr-changed); }
  .pr-diagram .pr-change-renamed > rect { stroke: var(--pr-renamed); stroke-width: 2; }
  .pr-diagram .pr-change-renamed .pr-header { fill: var(--pr-renamed); }
  .pr-diagram .pr-edge-change-added { stroke: var(--pr-added); stroke-width: 2; }
  .pr-diagram .pr-edge-change-removed { stroke: var(--pr-removed); stroke-dasharray: 3 3; }
  .pr-diagram .pr-edge-change-added + text, .pr-diagram .pr-edge-change-removed + text { font-weight: 700; }
)CSS";

std::string esc(const std::string& text);
std::string dataAttrs(const std::map<std::string, std::string>& refs);

std::string esc(const std::string& text) {
  std::string out;
  for (char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
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
  return "\n  <marker id=\"" + px +
         "tri\" viewBox=\"0 0 14 12\" refX=\"13\" refY=\"6\" markerWidth=\"14\" markerHeight=\"12\" orient=\"auto\"><path d=\"M1,1 L13,6 "
         "L1,11 Z\"/></marker>"
         "\n  <marker id=\"" +
         px +
         "diamond-filled\" viewBox=\"0 0 16 10\" refX=\"15\" refY=\"5\" markerWidth=\"16\" markerHeight=\"10\" orient=\"auto\"><path "
         "class=\"pr-filled\" d=\"M1,5 L8,1 L15,5 L8,9 Z\"/></marker>"
         "\n  <marker id=\"" +
         px +
         "diamond\" viewBox=\"0 0 16 10\" refX=\"15\" refY=\"5\" markerWidth=\"16\" markerHeight=\"10\" orient=\"auto\"><path d=\"M1,5 "
         "L8,1 L15,5 L8,9 Z\"/></marker>"
         "\n  <marker id=\"" +
         px +
         "arrow\" viewBox=\"0 0 12 12\" refX=\"11\" refY=\"6\" markerWidth=\"12\" markerHeight=\"12\" orient=\"auto\"><path "
         "class=\"pr-open\" d=\"M1,1 L11,6 L1,11\"/></marker>\n";
}

const std::map<std::string, std::string> kMarkerByKind = {
    {"inheritance", "tri"},     {"realization", "tri"},  {"composition", "diamond-filled"},
    {"aggregation", "diamond"}, {"dependency", "arrow"}, {"association", "arrow"}};

const std::map<std::string, std::string> kBadge = {{"class", "C"},       {"interface", "I"},  {"enum", "E"},
                                                   {"dataclass", "D"},   {"function", "f"},   {"struct", "S"},
                                                   {"requirement", "R"}, {"constraint", "K"}, {"verification", "V"}};

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
                                           : "<tspan class=\"pr-param\">" + esc(trim(p.substr(0, colon))) +
                                                 "</tspan><tspan class=\"pr-punct\">: </tspan>" + typed(p.substr(colon + 1));
    }
    const std::string returns = mm[3].matched ? "<tspan class=\"pr-punct\"> -> </tspan>" + typed(mm[3].str()) : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + esc(mm[1].str()) + "</tspan><tspan class=\"pr-punct\">(</tspan>" + params +
                     "<tspan class=\"pr-punct\">)</tspan>" + returns};
  }
  if (std::regex_match(body, mm, attribute)) {
    const std::string def =
        mm[3].matched ? "<tspan class=\"pr-punct\"> = </tspan><tspan class=\"pr-param\">" + esc(trim(mm[3].str())) + "</tspan>" : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + esc(mm[1].str()) + "</tspan><tspan class=\"pr-punct\"> : </tspan>" +
                     typed(mm[2].str()) + def};
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
  const auto change = n.refs.find("change");
  if (change != n.refs.end()) classes += " pr-change-" + change->second;
  std::ostringstream out;
  out << "<g id=\"" << px << esc(n.id) << "\" data-id=\"" << esc(n.id) << "\" class=\"" << classes << "\"" << dataAttrs(n.refs) << ">";
  if (!n.title.empty()) out << "<title>" << esc(n.title) << "</title>";
  if (n.kind == "container") {
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"6\"/>";
    out << "<text class=\"pr-header\" x=\"" << r.x + 10 << "\" y=\"" << r.y + 15 << "\">" << esc(n.label) << "</text>";
  } else if (n.kind == "note") {
    const int f = M.NOTE_FOLD;
    out << "<path d=\"M" << r.x << "," << r.y << " H" << r.x + r.w - f << " L" << r.x + r.w << "," << r.y + f << " V" << r.y + r.h << " H"
        << r.x << " Z\"/>";
    out << "<path d=\"M" << r.x + r.w - f << "," << r.y << " V" << r.y + f << " H" << r.x + r.w << "\"/>";
    const auto lines = linesOf(n.label);
    for (size_t i = 0; i < lines.size(); ++i) {
      out << "<text x=\"" << r.x + M.PAD_X << "\" y=\"" << r.y + M.PAD_X + 12 + static_cast<int>(i) * M.LINE_H << "\">" << esc(lines[i])
          << "</text>";
    }
  } else {
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"4\"/>";
    int ty = r.y + M.PAD_X + 12;
    const int rad = 4;
    const int bandH = n.sections.empty() ? r.h : M.PAD_X + 12 + (n.stereotype.empty() ? 0 : M.LINE_H) + M.SECTION_GAP + 6;
    out << "<path class=\"pr-head\" d=\"M" << r.x + rad << "," << r.y << " H" << r.x + r.w - rad << " A" << rad << "," << rad << " 0 0 1 "
        << r.x + r.w << "," << r.y + rad << " V" << r.y + bandH << " H" << r.x << " V" << r.y + rad << " A" << rad << "," << rad
        << " 0 0 1 " << r.x + rad << "," << r.y << " Z\"/>";
    if (!n.stereotype.empty()) {
      out << "<text class=\"pr-stereo\" x=\"" << r.x + M.PAD_X << "\" y=\"" << ty - 2 << "\">"
          << esc("\xC2\xAB" + n.stereotype + "\xC2\xBB") << "</text>";
      ty += M.LINE_H;
    }
    std::string letter = "C";
    if (n.abstract && n.classifier.empty()) {
      letter = "A";
    } else if (!n.classifier.empty()) {
      const auto b = kBadge.find(n.classifier);
      letter = b != kBadge.end() ? b->second : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(n.classifier[0]))));
    }
    out << "<circle class=\"pr-badge\" cx=\"" << r.x + M.PAD_X + 7 << "\" cy=\"" << ty - 4 << "\" r=\"7\"/>";
    out << "<text class=\"pr-badge-text\" x=\"" << r.x + M.PAD_X + 7 << "\" y=\"" << ty - 0.5 << "\" text-anchor=\"middle\">" << esc(letter)
        << "</text>";
    out << "<text class=\"pr-header\" x=\"" << r.x + M.PAD_X + M.BADGE_W << "\" y=\"" << ty << "\">" << esc(n.label) << "</text>";
    ty += M.LINE_H;
    for (const auto& section : n.sections) {
      const int sepY = ty - M.LINE_H + M.SECTION_GAP + 6;
      out << "<line class=\"pr-sep\" x1=\"" << r.x << "\" y1=\"" << sepY << "\" x2=\"" << r.x + r.w << "\" y2=\"" << sepY << "\"/>";
      ty += M.SECTION_GAP;
      for (const auto& line : section) {
        // only class members are tokenised; an artefact's title is prose and stays plain
        const auto marked = n.sectionKinds.empty() ? std::make_pair(std::string(), esc(line)) : memberMarkup(line);
        if (!marked.first.empty()) {
          out << "<circle class=\"pr-vis pr-vis-" << marked.first << "\" cx=\"" << r.x + M.PAD_X + 4 << "\" cy=\"" << ty - 4
              << "\" r=\"3.5\"/>";
        }
        out << "<text x=\"" << r.x + M.PAD_X + (marked.first.empty() ? 0 : M.DOT_W) << "\" y=\"" << ty << "\">" << marked.second
            << "</text>";
        ty += M.LINE_H;
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
    for (size_t i = 0; i < lines.size(); ++i)
      rows += "<tspan x=\"" + num(x) + "\" dy=\"" + num(i == 0 ? 0 : 14) + "\">" + esc(lines[i]) + "</tspan>";
    out.label =
        "<text class=\"pr-edge-label\" x=\"" + num(x) + "\" y=\"" + num(y) + "\">" + (lines.size() == 1 ? esc(e.label) : rows) + "</text>";
  }
  const auto change = e.refs.find("change");
  const std::string changeClass = change == e.refs.end() ? "" : " pr-edge-change-" + change->second;
  out.path = "<g class=\"pr-edge-group\" data-from=\"" + esc(e.from) + "\" data-to=\"" + esc(e.to) + "\"" + dataAttrs(e.refs) + ">" +
             (e.title.empty() ? "" : "<title>" + esc(e.title) + "</title>") + "<path class=\"pr-edge-hit\" d=\"" + d +
             "\"/><path class=\"pr-edge pr-edge-" + e.kind + changeClass + "\" d=\"" + d + "\"" + markerAttr + "/></g>";
  if (!e.href.empty()) out.path = "<a href=\"" + esc(e.href) + "\" class=\"pr-link\">" + out.path + "</a>";
  return out;
}

}  // namespace

// ---- sequences: lifelines as lanes, messages as rows; interaction lives in the SVG -----------
//
// The drawing carries its own usability: a tooltip on everything, links by identity
// (a lifeline to its class, a message to the behaviour it expands to), focus on hover,
// and expansions that fold on click with every row below moving up. A small script is
// embedded so this works wherever the SVG is inline or opened on its own; as an <img>
// it is simply static.

namespace {

const char* const kSeqStyle = R"CSS(
  .pr-diagram .pr-lifeline-line { stroke: var(--pr-stroke); stroke-dasharray: 4 4; }
  .pr-diagram .pr-lifeline-head > rect { fill: var(--pr-head-class); stroke: var(--pr-stroke); }
  .pr-diagram .pr-actor circle, .pr-diagram .pr-actor path { fill: none; stroke: var(--pr-text); stroke-width: 1.4; stroke-linecap: round; }
  .pr-diagram .pr-msg { stroke: var(--pr-edge); fill: none; pointer-events: none; }
  .pr-diagram .pr-msg-dashed { stroke-dasharray: 6 4; }
  .pr-diagram .pr-msg-label { fill: var(--pr-text); font-size: 11.5px; text-anchor: middle; paint-order: stroke; stroke: var(--pr-box-fill); stroke-width: 3px; stroke-linejoin: round; }
  .pr-diagram .pr-msg-self .pr-msg-label { text-anchor: start; }
  .pr-diagram { --pr-frame-alt: #4338ca; --pr-frame-loop: #047857; --pr-frame-opt: #b45309; --pr-frame-other: #334155; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-frame-alt: #a5b4fc; --pr-frame-loop: #6ee7b7; --pr-frame-opt: #fcd34d; --pr-frame-other: #cbd5e1; } }
  [data-theme="dark"] .pr-diagram { --pr-frame-alt: #a5b4fc; --pr-frame-loop: #6ee7b7; --pr-frame-opt: #fcd34d; --pr-frame-other: #cbd5e1; }
  .pr-diagram .pr-frame { --pr-frame: var(--pr-frame-other); }
  .pr-diagram .pr-frame-alt { --pr-frame: var(--pr-frame-alt); }
  .pr-diagram .pr-frame-loop { --pr-frame: var(--pr-frame-loop); }
  .pr-diagram .pr-frame-opt { --pr-frame: var(--pr-frame-opt); }
  .pr-diagram .pr-frame > rect { fill: none; stroke: var(--pr-frame); stroke-width: 1.6px; }
  .pr-diagram .pr-frame-tab { fill: var(--pr-frame); stroke: none; }
  .pr-diagram .pr-frame-label { font-weight: 700; fill: var(--pr-box-fill); font-size: 11px; letter-spacing: .3px; }
  .pr-diagram .pr-frame-cond { fill: var(--pr-text); font-size: 11px; }
  .pr-diagram .pr-frame-divider { stroke: var(--pr-frame); stroke-dasharray: 5 4; stroke-width: 1.2px; }
  .pr-diagram .pr-expandable { cursor: pointer; }
  .pr-diagram .pr-toggle { fill: var(--pr-muted); font-size: 10px; }
  .pr-diagram .pr-toggle-closed { display: none; }
  .pr-diagram .pr-collapsed .pr-toggle-open { display: none; }
  .pr-diagram .pr-collapsed .pr-toggle-closed { display: inline; }
  .pr-diagram .pr-collapsed .pr-msg-label { font-weight: 700; }
  .pr-diagram.pr-focusing .pr-row, .pr-diagram.pr-focusing .pr-lifeline, .pr-diagram.pr-focusing .pr-frame { opacity: var(--pr-dimmed, .25); transition: opacity .12s; }
  .pr-diagram.pr-focusing .pr-focus { opacity: 1; }
  .pr-diagram .pr-link:hover .pr-msg-label, .pr-diagram .pr-link:hover .pr-header { text-decoration: underline; }
  .pr-diagram .pr-row.pr-change-added .pr-msg { stroke: var(--pr-added); stroke-width: 2; }
  .pr-diagram .pr-row.pr-change-added .pr-msg-label { fill: var(--pr-added); font-weight: 700; }
  .pr-diagram .pr-row.pr-change-removed .pr-msg { stroke: var(--pr-removed); stroke-dasharray: 3 3; }
  .pr-diagram .pr-row.pr-change-removed .pr-msg-label { fill: var(--pr-removed); text-decoration: line-through; }
  .pr-diagram .pr-row.pr-change-changed .pr-msg { stroke: var(--pr-changed); stroke-width: 2; }
  .pr-diagram .pr-row.pr-change-changed .pr-msg-label { fill: var(--pr-changed); font-weight: 700; }
  .pr-diagram .pr-lifeline.pr-change-renamed .pr-header { fill: var(--pr-renamed); }
  .pr-diagram .pr-lifeline.pr-change-added .pr-header { fill: var(--pr-added); }
)CSS";

const char* const kSeqScript = R"JS(
(function () {
  var root = document.getElementById('%ROOT%');
  if (!root || root.dataset.wired) return;
  root.dataset.wired = '1';
  var rows = Array.prototype.slice.call(root.querySelectorAll('.pr-row')).map(function (el) {
    return { el: el, row: +el.dataset.row, y: +el.dataset.y, h: +el.dataset.h, span: el.dataset.refSpan };
  }).sort(function (a, b) { return a.row - b.row; });
  var frames = Array.prototype.slice.call(root.querySelectorAll('.pr-frame'));
  var lines = Array.prototype.slice.call(root.querySelectorAll('.pr-lifeline-line'));
  var collapsed = {};
  function relayout() {
    var hidden = {};
    rows.forEach(function (r) {
      if (!collapsed[r.row] || !r.span) return;
      var ab = r.span.split('-').map(Number);
      rows.forEach(function (q) { if (q.row >= ab[0] && q.row <= ab[1]) hidden[q.row] = true; });
    });
    var shift = 0, shiftOf = {}, endOf = {};
    rows.forEach(function (r) {
      shiftOf[r.row] = shift;
      if (hidden[r.row]) { r.el.style.display = 'none'; shift += r.h; return; }
      r.el.style.display = '';
      r.el.setAttribute('transform', 'translate(0,' + (-shift) + ')');
      r.el.classList.toggle('pr-collapsed', !!collapsed[r.row]);
      endOf[r.row] = r.y + r.h - shift;
    });
    frames.forEach(function (f) {
      var first = +f.dataset.first, last = +f.dataset.last, top = +f.dataset.top, bottom = +f.dataset.bottom;
      var lastVisible = -1;
      for (var q = last; q >= first; q--) { if (!hidden[q] && endOf[q] !== undefined) { lastVisible = q; break; } }
      if (hidden[first] || lastVisible < 0) { f.style.display = 'none'; return; }
      f.style.display = '';
      var newTop = top - shiftOf[first];
      var newBottom = lastVisible === last ? bottom - shiftOf[last] : endOf[lastVisible] - 2;
      f.setAttribute('transform', 'translate(0,' + newTop + ')');
      f.querySelector('rect').setAttribute('height', Math.max(24, newBottom - newTop));
      Array.prototype.forEach.call(f.querySelectorAll('[data-divider-row]'), function (d) {
        var row = +d.dataset.dividerRow, dy = +d.dataset.y - shiftOf[row] - newTop;
        if (d.tagName === 'line') { d.setAttribute('y1', dy); d.setAttribute('y2', dy); } else { d.setAttribute('y', dy + 4); }
        d.style.display = hidden[row] ? 'none' : '';
      });
    });
    lines.forEach(function (l) { l.setAttribute('y2', +l.dataset.y2 - shift); });
    var vb = root.dataset.viewbox.split(' ').map(Number);
    vb[3] -= shift;
    root.setAttribute('viewBox', vb.join(' '));
    root.setAttribute('height', vb[3]);
  }
  rows.forEach(function (r) {
    if (!r.span) return;
    r.el.classList.add('pr-expandable');
    r.el.addEventListener('click', function (e) { e.preventDefault(); collapsed[r.row] = !collapsed[r.row]; relayout(); });
  });
  function focus(ids, rowEl) {
    root.classList.add('pr-focusing');
    Array.prototype.forEach.call(root.querySelectorAll('.pr-lifeline'), function (el) { el.classList.toggle('pr-focus', !!ids[el.dataset.id]); });
    rows.forEach(function (r) { r.el.classList.toggle('pr-focus', rowEl ? r.el === rowEl : !!(ids[r.el.dataset.from] || ids[r.el.dataset.to])); });
  }
  function unfocus() {
    root.classList.remove('pr-focusing');
    Array.prototype.forEach.call(root.querySelectorAll('.pr-focus'), function (el) { el.classList.remove('pr-focus'); });
  }
  Array.prototype.forEach.call(root.querySelectorAll('.pr-lifeline'), function (l) {
    l.addEventListener('mouseenter', function () { var ids = {}; ids[l.dataset.id] = true; focus(ids); });
    l.addEventListener('mouseleave', unfocus);
  });
  rows.forEach(function (r) {
    r.el.addEventListener('mouseenter', function () { var ids = {}; ids[r.el.dataset.from] = true; ids[r.el.dataset.to] = true; focus(ids, r.el); });
    r.el.addEventListener('mouseleave', unfocus);
  });
})();
)JS";

struct Lane {
  const IrNode* node;
  int cx = 0;
  int headW = 0;
};

std::string dataAttrs(const std::map<std::string, std::string>& refs) {
  std::string out;
  for (const auto& r : refs) out += " data-ref-" + r.first + "=\"" + esc(r.second) + "\"";
  return out;
}

std::string sequenceSvg(const IrDocument& doc) {
  const int HEAD_H = 38, ROW_H = 30, SELF_H = 42, MARGIN = 20, SELF_W = 32;
  const int FRAME_LEAD = 30, FRAME_GAP = 8, FRAME_HEAD = 22, DIVIDER_LEAD = 26, FRAME_INSET = 16;
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
  const std::string px = idPrefix(doc.title);
  const std::string rootId = px + "root";
  if (lanes.empty()) {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 40\" width=\"320\" height=\"40\" class=\"pr-diagram\" "
           "role=\"img\"><style>" +
           std::string(kStyle) + "</style><text x=\"10\" y=\"25\">" + esc(doc.notice.empty() ? "Nothing to draw." : doc.notice) +
           "</text></svg>\n";
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

  // ---- emission
  std::ostringstream out;
  const int vx = 0, vy = 0, vw = width, vh = bottomY;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" id=\"" << rootId << "\" viewBox=\"" << vx << " " << vy << " " << vw << " " << vh
      << "\" data-viewbox=\"" << vx << " " << vy << " " << vw << " " << vh << "\" width=\"" << vw << "\" height=\"" << vh
      << "\" style=\"max-width:100%;height:auto\" class=\"pr-diagram pr-sequence\" role=\"img\">\n";
  if (!doc.title.empty()) out << "<title>" << esc(doc.title) << "</title>\n";
  out << "<style>" << kStyle << kSeqStyle << "</style>\n<defs>" << markers(px) << "  <marker id=\"" << px
      << "msg\" viewBox=\"0 0 12 12\" refX=\"11\" refY=\"6\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\"><path "
         "class=\"pr-filled\" d=\"M1,1 L11,6 L1,11 Z\"/></marker>\n</defs>\n";
  // frames under everything
  for (const Frame& f : frames) {
    const std::string kind = f.node->classifier;
    std::string condition = f.node->label;
    if (condition.rfind(kind + " ", 0) == 0) condition = condition.substr(kind.size() + 1);
    out << "<g class=\"pr-frame pr-frame-" << esc(kind) << "\" data-id=\"" << esc(f.node->id) << "\" data-first=\"" << f.first
        << "\" data-last=\"" << f.last << "\" data-top=\"" << f.top << "\" data-bottom=\"" << f.bottom << "\" transform=\"translate(0,"
        << f.top << ")\">";
    out << "<title>" << esc(kind + ": " + condition) << "</title>";
    out << "<rect x=\"" << f.left << "\" y=\"0\" width=\"" << f.right - f.left << "\" height=\"" << f.bottom - f.top << "\" rx=\"3\"/>";
    const int tabW = static_cast<int>(kind.size()) * 7 + 16;
    out << "<path class=\"pr-frame-tab\" d=\"M" << f.left << ",0 H" << f.left + tabW << " V" << FRAME_HEAD - 6 << " L" << f.left + tabW - 6
        << "," << FRAME_HEAD << " H" << f.left << " Z\"/>";
    out << "<text class=\"pr-frame-label\" x=\"" << f.left + 8 << "\" y=\"" << FRAME_HEAD - 7 << "\">" << esc(kind) << "</text>";
    out << "<text class=\"pr-frame-cond\" x=\"" << f.left + tabW + 10 << "\" y=\"" << FRAME_HEAD - 7 << "\">" << esc(condition)
        << "</text>";
    for (const auto& d : f.node->dividers) {
      if (d.first >= rowsCount) continue;
      // the divider sits in the lead reserved above its row
      int dy = rowStart[d.first];
      for (size_t k = 0; k < opensAt[d.first].size(); ++k) dy += FRAME_LEAD;
      dy += 6;
      out << "<line class=\"pr-frame-divider\" data-divider-row=\"" << d.first << "\" data-y=\"" << dy << "\" x1=\"" << f.left << "\" y1=\""
          << dy - f.top << "\" x2=\"" << f.right << "\" y2=\"" << dy - f.top << "\"/>";
      out << "<text class=\"pr-frame-cond\" data-divider-row=\"" << d.first << "\" data-y=\"" << dy << "\" x=\"" << f.left + 8 << "\" y=\""
          << dy - f.top + 4 << "\">[" << esc(d.second) << "]</text>";
    }
    out << "</g>\n";
  }
  // lifelines
  for (const Lane& l : lanes) {
    const IrNode& n = *l.node;
    const int hx = l.cx - l.headW / 2, hy = MARGIN;
    out << "<g class=\"pr-lifeline" << (n.classifier == "actor" ? " pr-actor" : "") << "\" data-id=\"" << esc(n.id) << "\""
        << dataAttrs(n.refs) << ">";
    if (!n.title.empty()) out << "<title>" << esc(n.title) << "</title>";
    out << "<line class=\"pr-lifeline-line\" x1=\"" << l.cx << "\" y1=\"" << hy + HEAD_H << "\" x2=\"" << l.cx << "\" y2=\"" << bottomY - 10
        << "\" data-y2=\"" << bottomY - 10 << "\"/>";
    std::string head;
    if (n.classifier == "actor") {
      const int cx = l.cx, top = hy;
      head = "<g class=\"pr-lifeline-head\"><circle cx=\"" + num(cx) + "\" cy=\"" + num(top + 6) + "\" r=\"5\"/><path d=\"M" + num(cx) +
             "," + num(top + 11) + " V" + num(top + 22) + " M" + num(cx - 8) + "," + num(top + 15) + " H" + num(cx + 8) + " M" + num(cx) +
             "," + num(top + 22) + " L" + num(cx - 7) + "," + num(top + 32) + " M" + num(cx) + "," + num(top + 22) + " L" + num(cx + 7) +
             "," + num(top + 32) + "\"/>" + "<text class=\"pr-header\" x=\"" + num(cx) + "\" y=\"" + num(top + HEAD_H + 10) +
             "\" text-anchor=\"middle\" font-size=\"11\">" + esc(n.label) + "</text></g>";
    } else {
      head = "<g class=\"pr-lifeline-head\"><rect x=\"" + num(hx) + "\" y=\"" + num(hy) + "\" width=\"" + num(l.headW) + "\" height=\"" +
             num(HEAD_H) + "\" rx=\"4\"/><text class=\"pr-header\" x=\"" + num(l.cx) + "\" y=\"" + num(hy + 24) +
             "\" text-anchor=\"middle\">" + esc(n.label) + "</text></g>";
    }
    out << (n.href.empty() ? head : "<a href=\"" + esc(n.href) + "\" class=\"pr-link\">" + head + "</a>") << "</g>\n";
  }
  // rows: one group per message, positioned at its absolute y, movable by the script
  for (const IrEdge* m : messages) {
    const int r = m->order;
    const Lane& a = lanes[laneOf[m->from]];
    const Lane& b = lanes[laneOf[m->to]];
    const int ly = rowLine[r];
    const bool self = m->from == m->to;
    std::string classes = std::string("pr-row pr-msg-group") + (self ? " pr-msg-self" : "") + (m->dashed ? " pr-msg-return" : "");
    const auto change = m->refs.find("change");
    if (change != m->refs.end()) classes += " pr-change-" + change->second;
    out << "<g class=\"" << classes << "\" data-row=\"" << r << "\" data-y=\"" << rowStart[r] << "\" data-h=\"" << rowEnd[r] - rowStart[r]
        << "\" data-from=\"" << esc(m->from) << "\" data-to=\"" << esc(m->to) << "\"" << dataAttrs(m->refs) << ">";
    std::string body;
    if (!m->title.empty()) body += "<title>" + esc(m->title) + "</title>";
    std::string d;
    if (self) {
      d = "M" + num(a.cx) + "," + num(ly) + " H" + num(a.cx + SELF_W) + " V" + num(ly + 16) + " H" + num(a.cx + 3);
    } else {
      const int dir = b.cx > a.cx ? -1 : 1;
      d = "M" + num(a.cx) + "," + num(ly) + " L" + num(b.cx + dir * 2) + "," + num(ly);
    }
    body += "<path class=\"pr-edge-hit\" d=\"" + d + "\"/>";
    body += "<path class=\"pr-msg" + std::string(m->dashed ? " pr-msg-dashed" : "") + "\" d=\"" + d + "\" marker-end=\"url(#" + px +
            (m->dashed ? "arrow" : "msg") + ")\"/>";
    if (self) {
      body += "<text class=\"pr-msg-label\" x=\"" + num(a.cx + SELF_W + 6) + "\" y=\"" + num(ly + 12) + "\">" + esc(m->label) + "</text>";
    } else {
      body += "<text class=\"pr-msg-label\" x=\"" + num((a.cx + b.cx) / 2) + "\" y=\"" + num(ly - 5) + "\">" + esc(m->label) + "</text>";
    }
    if (m->refs.count("span")) {
      const int tx = self ? a.cx + SELF_W + 6 + static_cast<int>(m->label.size()) * M.CHAR_W + 6
                          : (a.cx + b.cx) / 2 + static_cast<int>(m->label.size()) * 4 + 8;
      const int ty = self ? ly + 12 : ly - 5;
      body += "<text class=\"pr-toggle pr-toggle-open\" x=\"" + num(tx) + "\" y=\"" + num(ty) + "\">\xE2\x96\xBE fold</text>";
      body += "<text class=\"pr-toggle pr-toggle-closed\" x=\"" + num(tx) + "\" y=\"" + num(ty) + "\">\xE2\x96\xB8 unfold</text>";
    }
    out << (m->href.empty() ? body : "<a href=\"" + esc(m->href) + "\" class=\"pr-link\">" + body + "</a>") << "</g>\n";
  }
  if (!doc.notice.empty())
    out << "<text class=\"pr-notice\" x=\"" << MARGIN << "\" y=\"" << bottomY - 4 << "\">" << esc(doc.notice) << "</text>\n";
  std::string script = kSeqScript;
  const std::string marker = "%ROOT%";
  script.replace(script.find(marker), marker.size(), rootId);
  out << "<script><![CDATA[" << script << "]]></script>\n</svg>\n";
  return out.str();
}

}  // namespace

std::string Svg::render(const IrDocument& doc, int maxWidth) const {
  for (const auto& n : doc.nodes) {
    if (n.kind == "lifeline") return sequenceSvg(doc);
  }
  const IrDocument bundled = doc.bundled();
  return render(bundled, layout(bundled, Metrics::svg(), maxWidth));
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
    const int w = static_cast<int>(width) * M.CHAR_W + 2 * M.PAD_X;
    const int h = static_cast<int>(lines.size()) * M.LINE_H + 2 * M.PAD_X - 4;
    const int x = minX;
    const int y = maxY + 2 * M.PAD_X;
    noticeBlock = "<g class=\"pr-notice\"><rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
                  "\" rx=\"3\"/>";
    for (size_t i = 0; i < lines.size(); ++i)
      noticeBlock += "<text x=\"" + num(x + M.PAD_X) + "\" y=\"" + num(y + M.PAD_X + 4 + static_cast<int>(i) * M.LINE_H) + "\">" +
                     esc(lines[i]) + "</text>";
    noticeBlock += "</g>";
    extend(x + w, y + h);
  }
  const int vx = minX - M.PAD_X, vy = minY - M.PAD_X, vw = maxX + M.PAD_X - vx, vh = maxY + M.PAD_X - vy;
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << vx << " " << vy << " " << vw << " " << vh << "\" width=\"" << vw
      << "\" height=\"" << vh << "\" style=\"max-width:100%;height:auto\" class=\"pr-diagram\" role=\"img\">\n";
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
}  // namespace ds
