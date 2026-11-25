#include "ds/render/Svg.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include "ds/text/Text.hpp"
#include "ds/theme/Writers.hpp"

namespace ds {
namespace render {

namespace {

std::string dataAttrs(const std::map<std::string, std::string>& refs);

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
  if (!std::string(text::trimSpaces(current)).empty()) out.push_back(current);
  return out;
}

std::string typed(const std::string& type) {
  return "<tspan class=\"pr-type\">" + text::escapeXml(std::string(text::trimSpaces(type))) + "</tspan>";
}

/// One member line of `ds render` (`+ abstract run(model: Model) -> list<Finding>`,
/// `- items: list<T> = 0`) tokenised into the engine's syntax-coloured tspans.
std::pair<std::string, std::string> memberMarkup(const std::string& line) {
  static const std::regex head(R"(^\s*([+\-#~])?\s*((?:(?:abstract|static|const)\s+)*)(.*)$)");
  static const std::regex method(R"(^([^\s(]+)\s*\((.*)\)\s*(?:->\s*(.+))?$)");
  static const std::regex attribute(R"(^([^\s:]+)\s*:\s*([^=]+?)(?:\s*=\s*(.+))?$)");
  static const std::map<char, std::string> visibility = {{'+', "public"}, {'-', "private"}, {'#', "protected"}, {'~', "package"}};
  std::smatch m;
  if (!std::regex_match(line, m, head)) return {"", text::escapeXml(line)};
  const std::string vis = m[1].matched ? visibility.at(m[1].str()[0]) : "";
  const std::string modifiers = m[2].str();
  const std::string body = std::string(text::trimSpaces(m[3].str()));
  std::string nameClass = "pr-name";
  if (modifiers.find("static") != std::string::npos) nameClass += " pr-static";
  if (modifiers.find("abstract") != std::string::npos) nameClass += " pr-abstract-member";
  std::smatch mm;
  if (std::regex_match(body, mm, method)) {
    std::string params;
    for (const auto& p : splitParams(mm[2].str())) {
      if (!params.empty()) params += "<tspan class=\"pr-punct\">, </tspan>";
      const auto colon = p.find(':');
      params += colon == std::string::npos
                    ? "<tspan class=\"pr-param\">" + text::escapeXml(std::string(text::trimSpaces(p))) + "</tspan>"
                    : "<tspan class=\"pr-param\">" + text::escapeXml(std::string(text::trimSpaces(p.substr(0, colon)))) +
                          "</tspan><tspan class=\"pr-punct\">: </tspan>" + typed(p.substr(colon + 1));
    }
    const std::string returns = mm[3].matched ? "<tspan class=\"pr-punct\"> -> </tspan>" + typed(mm[3].str()) : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + text::escapeXml(mm[1].str()) + "</tspan><tspan class=\"pr-punct\">(</tspan>" +
                     params + "<tspan class=\"pr-punct\">)</tspan>" + returns};
  }
  if (std::regex_match(body, mm, attribute)) {
    const std::string def = mm[3].matched ? "<tspan class=\"pr-punct\"> = </tspan><tspan class=\"pr-param\">" +
                                                text::escapeXml(std::string(text::trimSpaces(mm[3].str()))) + "</tspan>"
                                          : "";
    return {vis, "<tspan class=\"" + nameClass + "\">" + text::escapeXml(mm[1].str()) + "</tspan><tspan class=\"pr-punct\"> : </tspan>" +
                     typed(mm[2].str()) + def};
  }
  return {vis, "<tspan class=\"" + nameClass + "\">" + text::escapeXml(body) + "</tspan>"};
}

std::string num(int v) { return std::to_string(v); }
std::string num(double v) { return num(static_cast<int>(v)); }  // a position computed in doubles, drawn in whole pixels

std::string emitNode(const Placed& p, const std::string& px, const Metrics& M) {
  const IrNode& n = *p.node;
  const Rect& r = p.rect;
  std::string classes = "pr-" + n.kind;
  if (n.kind == "state") classes += " pr-box";  // a state is a box with round ends: it takes a box's frame (DS-442)
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
  out << "<g id=\"" << px << text::escapeXml(n.id) << "\" data-id=\"" << text::escapeXml(n.id) << "\" class=\"" << classes << "\""
      << dataAttrs(n.refs) << ">";
  if (!n.title.empty()) out << "<title>" << text::escapeXml(n.title) << "</title>";
  if (n.kind == "container") {
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"6\"/>";
    out << "<text class=\"pr-header\" x=\"" << r.x + 10 << "\" y=\"" << r.y + 15 << "\">" << text::escapeXml(n.label) << "</text>";
  } else if (n.kind == "note") {
    const int f = M.NOTE_FOLD;
    out << "<path d=\"M" << r.x << "," << r.y << " H" << r.x + r.w - f << " L" << r.x + r.w << "," << r.y + f << " V" << r.y + r.h << " H"
        << r.x << " Z\"/>";
    out << "<path d=\"M" << r.x + r.w - f << "," << r.y << " V" << r.y + f << " H" << r.x + r.w << "\"/>";
    const auto lines = text::linesOf(n.label);
    for (size_t i = 0; i < lines.size(); ++i) {
      out << "<text x=\"" << r.x + M.PAD_X << "\" y=\"" << r.y + M.PAD_X + 12 + static_cast<int>(i) * M.LINE_H << "\">"
          << text::escapeXml(lines[i]) << "</text>";
    }
  } else if (n.kind == "initial" || n.kind == "final") {
    // where a machine starts, UML's filled dot; where it ends, a bullseye (DS-442). Their colour is the text's token
    const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (n.kind == "initial") {
      out << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"7\" style=\"fill: var(--pr-text)\"/>";
    } else {
      out << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"9\" style=\"fill: none; stroke: var(--pr-text); stroke-width: 1.5\"/>"
          << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"5\" style=\"fill: var(--pr-text)\"/>";
    }
  } else {
    // a state is a capsule: round ends, no head band and no badge (DS-442); a box keeps its corners and its band
    const bool state = n.kind == "state";
    const int rad = state ? (n.sections.empty() ? r.h / 2 : 10) : 4;
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"" << rad << "\"/>";
    int ty = r.y + M.PAD_X + 12;
    const int bandH = n.sections.empty() ? r.h : M.PAD_X + 12 + (n.stereotype.empty() ? 0 : M.LINE_H) + M.SECTION_GAP + 6;
    if (!state)
      out << "<path class=\"pr-head\" d=\"M" << r.x + rad << "," << r.y << " H" << r.x + r.w - rad << " A" << rad << "," << rad << " 0 0 1 "
          << r.x + r.w << "," << r.y + rad << " V" << r.y + bandH << " H" << r.x << " V" << r.y + rad << " A" << rad << "," << rad
          << " 0 0 1 " << r.x + rad << "," << r.y << " Z\"/>";
    if (!n.stereotype.empty()) {
      out << "<text class=\"pr-stereo\" x=\"" << r.x + M.PAD_X << "\" y=\"" << ty - 2 << "\">"
          << text::escapeXml("\xC2\xAB" + n.stereotype + "\xC2\xBB") << "</text>";
      ty += M.LINE_H;
    }
    if (state) {  // its name in the middle, as the capsule is drawn round it
      out << "<text class=\"pr-header\" x=\"" << r.x + r.w / 2 << "\" y=\"" << ty << "\" text-anchor=\"middle\">"
          << text::escapeXml(n.label) << "</text>";
    } else {
      const std::string letter = IrNode::badgeOf(n.classifier, n.abstract);
      out << "<circle class=\"pr-badge\" cx=\"" << r.x + M.PAD_X + 7 << "\" cy=\"" << ty - 4 << "\" r=\"7\"/>";
      out << "<text class=\"pr-badge-text\" x=\"" << r.x + M.PAD_X + 7 << "\" y=\"" << ty - 0.5 << "\" text-anchor=\"middle\">"
          << text::escapeXml(letter) << "</text>";
      out << "<text class=\"pr-header\" x=\"" << r.x + M.PAD_X + M.BADGE_W << "\" y=\"" << ty << "\">" << text::escapeXml(n.label)
          << "</text>";
    }
    ty += M.LINE_H;
    for (size_t s = 0; s < n.sections.size(); ++s) {
      const auto& section = n.sections[s];
      const int sepY = ty - M.LINE_H + M.SECTION_GAP + 6;
      out << "<line class=\"pr-sep\" x1=\"" << r.x << "\" y1=\"" << sepY << "\" x2=\"" << r.x + r.w << "\" y2=\"" << sepY << "\"/>";
      ty += M.SECTION_GAP;
      // what a closed box offers, all of one kind: each name with the badge a box of its own would carry (DS-244)
      const std::string offered = s < n.sectionKinds.size() && !IrNode::holdsMembers(n.sectionKinds[s]) ? n.sectionKinds[s] : "";
      for (const auto& line : section) {
        if (!offered.empty()) {
          out << "<g class=\"pr-offered pr-classifier-" << offered << "\"><circle class=\"pr-badge\" cx=\"" << r.x + M.PAD_X + 6
              << "\" cy=\"" << ty - 4 << "\" r=\"6\"/><text class=\"pr-badge-text\" x=\"" << r.x + M.PAD_X + 6 << "\" y=\"" << ty - 1
              << "\" text-anchor=\"middle\">" << text::escapeXml(IrNode::badgeOf(offered, false)) << "</text></g>";
          out << "<text x=\"" << r.x + M.PAD_X + M.BADGE_W << "\" y=\"" << ty << "\">" << text::escapeXml(line) << "</text>";
          ty += M.LINE_H;
          continue;
        }
        // only class members are tokenised; an artefact's title is prose and stays plain
        const auto marked = n.sectionKinds.empty() ? std::make_pair(std::string(), text::escapeXml(line)) : memberMarkup(line);
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
  return n.href.empty() ? g : "<a href=\"" + text::escapeXml(n.href) + "\" class=\"pr-link\">" + g + "</a>";
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
  if (route.curve && points.size() > 2) {
    // a smooth curve through every point (DS-443): Catmull-Rom, each stretch a cubic Bezier, the ends held
    d = "M" + num(points[0].x) + "," + num(points[0].y);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
      const Point& p0 = points[i == 0 ? 0 : i - 1];
      const Point& p1 = points[i];
      const Point& p2 = points[i + 1];
      const Point& p3 = points[i + 2 < points.size() ? i + 2 : i + 1];
      d += " C" + num(p1.x + (p2.x - p0.x) / 6) + "," + num(p1.y + (p2.y - p0.y) / 6) + " " + num(p2.x - (p3.x - p1.x) / 6) + "," +
           num(p2.y - (p3.y - p1.y) / 6) + " " + num(p2.x) + "," + num(p2.y);
    }
  } else {
    for (size_t i = 0; i < points.size(); ++i) d += (i == 0 ? "M" : " L") + num(points[i].x) + "," + num(points[i].y);
  }
  const auto marker = kMarkerByKind.find(e.kind);
  const std::string markerAttr = marker == kMarkerByKind.end() ? "" : " marker-end=\"url(#" + px + marker->second + ")\"";
  EdgeMarkup out;
  if (!e.label.empty() && route.label) {  // where the router put it: on the curve, clear of the boxes (DS-443)
    // on the outer side of its curve: a label left of the line between the ends grows to the left, away from it
    const bool left = !points.empty() && route.label->x * 2 < points.front().x + points.back().x;
    const int x = route.label->x + (left ? -6 : 6);
    const int y = route.label->y - 4;
    const auto lines = text::linesOf(e.label);
    std::string rows;
    for (size_t i = 0; i < lines.size(); ++i)
      rows += "<tspan x=\"" + num(x) + "\" dy=\"" + num(i == 0 ? 0 : 14) + "\">" + text::escapeXml(lines[i]) + "</tspan>";
    out.label = "<text class=\"pr-edge-label\"" + std::string(left ? " text-anchor=\"end\"" : "") + " x=\"" + num(x) + "\" y=\"" + num(y) +
                "\">" + (lines.size() == 1 ? text::escapeXml(e.label) : rows) + "</text>";
  } else if (!e.label.empty() && points.size() >= 2) {
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
    const auto lines = text::linesOf(e.label);
    std::string rows;
    for (size_t i = 0; i < lines.size(); ++i)
      rows += "<tspan x=\"" + num(x) + "\" dy=\"" + num(i == 0 ? 0 : 14) + "\">" + text::escapeXml(lines[i]) + "</tspan>";
    out.label = "<text class=\"pr-edge-label\" x=\"" + num(x) + "\" y=\"" + num(y) + "\">" +
                (lines.size() == 1 ? text::escapeXml(e.label) : rows) + "</text>";
  }
  const auto change = e.refs.find("change");
  const std::string changeClass = change == e.refs.end() ? "" : " pr-edge-change-" + change->second;
  out.path = "<g class=\"pr-edge-group\" data-from=\"" + text::escapeXml(e.from) + "\" data-to=\"" + text::escapeXml(e.to) + "\"" +
             dataAttrs(e.refs) + ">" + (e.title.empty() ? "" : "<title>" + text::escapeXml(e.title) + "</title>") +
             "<path class=\"pr-edge-hit\" d=\"" + d + "\"/><path class=\"pr-edge pr-edge-" + e.kind + changeClass + "\" d=\"" + d + "\"" +
             markerAttr + "/></g>";
  if (!e.href.empty()) out.path = "<a href=\"" + text::escapeXml(e.href) + "\" class=\"pr-link\">" + out.path + "</a>";
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
  for (const auto& r : refs) out += " data-ref-" + r.first + "=\"" + text::escapeXml(r.second) + "\"";
  return out;
}

std::string sequenceSvg(const IrDocument& doc, const Metrics& M, const std::string& style, const std::string& sequenceStyle) {
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
           style + "</style><text x=\"10\" y=\"25\">" + text::escapeXml(doc.notice.empty() ? "Nothing to draw." : doc.notice) +
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
  if (!doc.title.empty()) out << "<title>" << text::escapeXml(doc.title) << "</title>\n";
  out << "<style>" << style << sequenceStyle << "</style>\n<defs>" << markers(px) << "  <marker id=\"" << px
      << "msg\" viewBox=\"0 0 12 12\" refX=\"11\" refY=\"6\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\"><path "
         "class=\"pr-filled\" d=\"M1,1 L11,6 L1,11 Z\"/></marker>\n</defs>\n";
  // frames under everything
  for (const Frame& f : frames) {
    const std::string kind = f.node->classifier;
    std::string condition = f.node->label;
    if (condition.rfind(kind + " ", 0) == 0) condition = condition.substr(kind.size() + 1);
    out << "<g class=\"pr-frame pr-frame-" << text::escapeXml(kind) << "\" data-id=\"" << text::escapeXml(f.node->id) << "\" data-first=\""
        << f.first << "\" data-last=\"" << f.last << "\" data-top=\"" << f.top << "\" data-bottom=\"" << f.bottom
        << "\" transform=\"translate(0," << f.top << ")\">";
    out << "<title>" << text::escapeXml(kind + ": " + condition) << "</title>";
    out << "<rect x=\"" << f.left << "\" y=\"0\" width=\"" << f.right - f.left << "\" height=\"" << f.bottom - f.top << "\" rx=\"3\"/>";
    const int tabW = static_cast<int>(kind.size()) * 7 + 16;
    out << "<path class=\"pr-frame-tab\" d=\"M" << f.left << ",0 H" << f.left + tabW << " V" << FRAME_HEAD - 6 << " L" << f.left + tabW - 6
        << "," << FRAME_HEAD << " H" << f.left << " Z\"/>";
    out << "<text class=\"pr-frame-label\" x=\"" << f.left + 8 << "\" y=\"" << FRAME_HEAD - 7 << "\">" << text::escapeXml(kind)
        << "</text>";
    out << "<text class=\"pr-frame-cond\" x=\"" << f.left + tabW + 10 << "\" y=\"" << FRAME_HEAD - 7 << "\">" << text::escapeXml(condition)
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
          << dy - f.top + 4 << "\">[" << text::escapeXml(d.second) << "]</text>";
    }
    out << "</g>\n";
  }
  // lifelines
  for (const Lane& l : lanes) {
    const IrNode& n = *l.node;
    const int hx = l.cx - l.headW / 2, hy = MARGIN;
    out << "<g class=\"pr-lifeline" << (n.classifier == "actor" ? " pr-actor" : "") << "\" data-id=\"" << text::escapeXml(n.id) << "\""
        << dataAttrs(n.refs) << ">";
    if (!n.title.empty()) out << "<title>" << text::escapeXml(n.title) << "</title>";
    out << "<line class=\"pr-lifeline-line\" x1=\"" << l.cx << "\" y1=\"" << hy + HEAD_H << "\" x2=\"" << l.cx << "\" y2=\"" << bottomY - 10
        << "\" data-y2=\"" << bottomY - 10 << "\"/>";
    std::string head;
    if (n.classifier == "actor") {
      const int cx = l.cx, top = hy;
      head = "<g class=\"pr-lifeline-head\"><circle cx=\"" + num(cx) + "\" cy=\"" + num(top + 6) + "\" r=\"5\"/><path d=\"M" + num(cx) +
             "," + num(top + 11) + " V" + num(top + 22) + " M" + num(cx - 8) + "," + num(top + 15) + " H" + num(cx + 8) + " M" + num(cx) +
             "," + num(top + 22) + " L" + num(cx - 7) + "," + num(top + 32) + " M" + num(cx) + "," + num(top + 22) + " L" + num(cx + 7) +
             "," + num(top + 32) + "\"/>" + "<text class=\"pr-header\" x=\"" + num(cx) + "\" y=\"" + num(top + HEAD_H + 10) +
             "\" text-anchor=\"middle\" font-size=\"11\">" + text::escapeXml(n.label) + "</text></g>";
    } else {
      head = "<g class=\"pr-lifeline-head\"><rect x=\"" + num(hx) + "\" y=\"" + num(hy) + "\" width=\"" + num(l.headW) + "\" height=\"" +
             num(HEAD_H) + "\" rx=\"4\"/><text class=\"pr-header\" x=\"" + num(l.cx) + "\" y=\"" + num(hy + 24) +
             "\" text-anchor=\"middle\">" + text::escapeXml(n.label) + "</text></g>";
    }
    out << (n.href.empty() ? head : "<a href=\"" + text::escapeXml(n.href) + "\" class=\"pr-link\">" + head + "</a>") << "</g>\n";
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
        << "\" data-from=\"" << text::escapeXml(m->from) << "\" data-to=\"" << text::escapeXml(m->to) << "\"" << dataAttrs(m->refs) << ">";
    std::string body;
    if (!m->title.empty()) body += "<title>" + text::escapeXml(m->title) + "</title>";
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
      body += "<text class=\"pr-msg-label\" x=\"" + num(a.cx + SELF_W + 6) + "\" y=\"" + num(ly + 12) + "\">" + text::escapeXml(m->label) +
              "</text>";
    } else {
      body += "<text class=\"pr-msg-label\" x=\"" + num((a.cx + b.cx) / 2) + "\" y=\"" + num(ly - 5) + "\">" + text::escapeXml(m->label) +
              "</text>";
    }
    if (m->refs.count("span")) {
      const int tx = self ? a.cx + SELF_W + 6 + static_cast<int>(m->label.size()) * M.CHAR_W + 6
                          : (a.cx + b.cx) / 2 + static_cast<int>(m->label.size()) * 4 + 8;
      const int ty = self ? ly + 12 : ly - 5;
      body += "<text class=\"pr-toggle pr-toggle-open\" x=\"" + num(tx) + "\" y=\"" + num(ty) + "\">\xE2\x96\xBE fold</text>";
      body += "<text class=\"pr-toggle pr-toggle-closed\" x=\"" + num(tx) + "\" y=\"" + num(ty) + "\">\xE2\x96\xB8 unfold</text>";
    }
    out << (m->href.empty() ? body : "<a href=\"" + text::escapeXml(m->href) + "\" class=\"pr-link\">" + body + "</a>") << "</g>\n";
  }
  if (!doc.notice.empty())
    out << "<text class=\"pr-notice\" x=\"" << MARGIN << "\" y=\"" << bottomY - 4 << "\">" << text::escapeXml(doc.notice) << "</text>\n";
  std::string script = kSeqScript;
  const std::string marker = "%ROOT%";
  script.replace(script.find(marker), marker.size(), rootId);
  out << "<script><![CDATA[" << script << "]]></script>\n</svg>\n";
  return out.str();
}

}  // namespace

namespace {
/// A drawing that is a table (DS-251): the headings across and down, and each cell drawn by its kind.
std::string tableSvg(const IrDocument& doc, const std::string& style) {
  const IrTable& t = doc.table;
  const int charW = 8, padX = 14, rowH = 34, headH = 38, lineH = 17;
  const std::size_t kWrap = 40;  // a cell is as wide as this many characters; what is longer goes on more lines (DS-268)
  const auto lines = [&](const std::string& text) {
    std::vector<std::string> out{""};
    std::istringstream in(text);
    std::string word;
    while (in >> word) {
      if (!out.back().empty() && out.back().size() + 1 + word.size() > kWrap) out.push_back("");
      out.back() += (out.back().empty() ? "" : " ") + word;
    }
    return out;
  };
  const auto widest = [&](const std::string& text) {
    std::size_t w = 0;
    for (const auto& l : lines(text)) w = std::max(w, l.size());
    return static_cast<int>(w);
  };
  std::vector<int> widths{widest(t.corner)};
  for (const IrRow& row : t.rows) widths[0] = std::max(widths[0], widest(row.label));
  for (std::size_t c = 0; c < t.columns.size(); ++c) {
    int longest = widest(t.columns[c]);
    for (const IrRow& row : t.rows)
      if (c < row.cells.size()) longest = std::max(longest, widest(row.cells[c].text));
    widths.push_back(longest);
  }
  int width = 0;
  for (int& w : widths) {
    w = w * charW + 2 * padX;
    width += w;
  }
  std::vector<int> heights;
  int height = headH;
  for (const IrRow& row : t.rows) {
    std::size_t most = lines(row.label).size();
    for (const IrCell& it : row.cells) most = std::max(most, lines(it.text).size());
    heights.push_back(std::max(rowH, static_cast<int>(most) * lineH + 17));
    height += heights.back();
  }
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-10 -10 " << width + 20 << " " << height + 20 << "\" width=\"" << width + 20
      << "\" height=\"" << height + 20 << "\" style=\"max-width:100%;height:auto\" class=\"pr-diagram\" role=\"img\">\n";
  if (!doc.title.empty()) out << "<title>" << text::escapeXml(doc.title) << "</title>\n";
  out << "<style>" << style
      << "\n  .pr-diagram .pr-cell rect { fill: var(--pr-box-fill); stroke: var(--pr-stroke); stroke-width: 1; }"
         "\n  .pr-diagram .pr-cell-head rect { fill: var(--pr-head-class); }"
         "\n  .pr-diagram .pr-cell-head text, .pr-diagram .pr-cell-jump text { font-weight: 600; }"
         "\n  .pr-diagram .pr-cell-against rect { stroke: var(--pr-removed); stroke-width: 2; }"
         "\n  .pr-diagram .pr-cell-against text { fill: var(--pr-removed); font-weight: 600; }"
      << "</style>\n";
  const auto cell = [&](int x, int y, int w, int h, const std::string& text, const std::string& kind, const std::string& title,
                        bool centred) {
    const auto written = lines(text);
    out << "<g class=\"pr-cell pr-cell-" << kind << "\">";
    if (!title.empty()) out << "<title>" << text::escapeXml(title) << "</title>";
    out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\"/>";
    const int first = y + h / 2 + 4 - (static_cast<int>(written.size()) - 1) * lineH / 2;
    for (std::size_t l = 0; l < written.size(); ++l)
      out << "<text x=\"" << (centred ? x + w / 2 : x + padX) << "\" y=\"" << first + static_cast<int>(l) * lineH << "\""
          << (centred ? " text-anchor=\"middle\"" : "") << ">" << text::escapeXml(written[l]) << "</text>";
    out << "</g>\n";
  };
  int x = 0;
  cell(x, 0, widths[0], headH, t.corner, "head", "", false);
  x += widths[0];
  for (std::size_t c = 0; c < t.columns.size(); ++c) {
    cell(x, 0, widths[c + 1], headH, t.columns[c], "head", "", true);
    x += widths[c + 1];
  }
  int y = headH;
  for (std::size_t r = 0; r < t.rows.size(); ++r) {
    cell(0, y, widths[0], heights[r], t.rows[r].label, "head", t.rows[r].title, false);
    x = widths[0];
    for (std::size_t c = 0; c < t.columns.size() && c < t.rows[r].cells.size(); ++c) {
      const IrCell& it = t.rows[r].cells[c];
      cell(x, y, widths[c + 1], heights[r], it.text, it.kind, it.title, lines(it.text).size() == 1);
      x += widths[c + 1];
    }
    y += heights[r];
  }
  out << "</svg>\n";
  return out.str();
}

/// Several drawings in one (DS-245): each keeps its own drawing and is placed beside the last, or under it when
/// the width asked for does not hold them side by side. Each is a nested svg that says which part it is.
std::string besideEachOther(const std::vector<std::pair<std::string, std::string>>& drawings, int maxWidth) {
  static const std::regex size(R"re(width="([0-9.]+)" height="([0-9.]+)")re");
  const int gap = 32;
  std::vector<std::pair<double, double>> sizes;
  double all = 0;
  for (const auto& drawing : drawings) {
    std::smatch m;
    const std::string head = drawing.second.substr(0, drawing.second.find('>'));
    sizes.push_back(std::regex_search(head, m, size) ? std::make_pair(std::stod(m[1].str()), std::stod(m[2].str()))
                                                     : std::make_pair(0.0, 0.0));
    all += sizes.back().first + (all > 0 ? gap : 0);
  }
  const bool under = maxWidth > 0 && all > maxWidth;
  double width = 0, height = 0;
  std::string inside;
  for (size_t i = 0; i < drawings.size(); ++i) {
    const double x = under || width == 0 ? 0 : width + gap, y = under && height > 0 ? height + gap : 0;
    std::string placed = drawings[i].second;
    placed.insert(placed.find("<svg ") + 5, "x=\"" + num(x) + "\" y=\"" + num(y) + "\" data-part=\"" + drawings[i].first + "\" ");
    inside += placed;
    width = under ? std::max(width, sizes[i].first) : x + sizes[i].first;
    height = under ? y + sizes[i].second : std::max(height, sizes[i].second);
  }
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << num(width) << " " << num(height) << "\" width=\"" << num(width)
      << "\" height=\"" << num(height) << "\" class=\"pr-parts\">" << inside << "</svg>";
  return out.str();
}
}  // namespace

Svg::Svg(const theme::Theme& theme)
    : metrics_(Metrics::svg(theme.measures(theme::Sink::svg))),
      style_(theme::ThemeWriterFactory::of().at("svg")->write(theme)),
      sequenceStyle_(theme::ThemeWriterFactory::of().at("sequence")->write(theme)) {}

std::string Svg::render(const IrDocument& doc, int maxWidth) const {
  if (!doc.table.columns.empty()) return tableSvg(doc, style_);
  if (!doc.parts.empty()) {
    std::vector<std::pair<std::string, std::string>> drawings;
    for (const auto& name : doc.parts) drawings.push_back({name, render(doc.part(name), maxWidth)});
    return besideEachOther(drawings, maxWidth);
  }
  for (const auto& n : doc.nodes) {
    if (n.kind == "lifeline") return sequenceSvg(doc, metrics_, style_, sequenceStyle_);
  }
  const IrDocument bundled = doc.bundled();
  return render(bundled, Layout(metrics_).place(bundled, maxWidth, true));  // routed as its kind is drawn (DS-443)
}

std::string Svg::render(const IrDocument& doc, const LayoutResult& lay) const {
  const Metrics& M = metrics_;
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
    if (r.second.label && r.first < doc.edges.size()) {  // a label the router placed, a loop's beside its box (DS-443)
      int widest = 0;
      const auto lines = text::linesOf(doc.edges[r.first].label);
      for (const auto& l : lines) widest = std::max(widest, static_cast<int>(l.size()) * M.CHAR_W);
      const auto& pts = r.second.points;
      const bool left = !pts.empty() && r.second.label->x * 2 < pts.front().x + pts.back().x;  // as emitEdge sets it
      extend(r.second.label->x + (left ? -6 - widest : 6 + widest), r.second.label->y - 16);
      extend(r.second.label->x, r.second.label->y + 14 * static_cast<int>(lines.size()));
    }
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
  for (const Placed* f : frames) containers.push_back(emitNode(*f, px, metrics_));
  for (const auto& p : lay.placed) {
    if (p.node->kind != "container") leaves.push_back(emitNode(p, px, metrics_));
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
    const auto lines = text::linesOf(notice);
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
                     text::escapeXml(lines[i]) + "</text>";
    noticeBlock += "</g>";
    extend(x + w, y + h);
  }
  const int vx = minX - M.PAD_X, vy = minY - M.PAD_X, vw = maxX + M.PAD_X - vx, vh = maxY + M.PAD_X - vy;
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << vx << " " << vy << " " << vw << " " << vh << "\" width=\"" << vw
      << "\" height=\"" << vh << "\" style=\"max-width:100%;height:auto\" class=\"pr-diagram\" role=\"img\">\n";
  if (!doc.title.empty()) out << "<title>" << text::escapeXml(doc.title) << "</title>\n";
  out << "<style>" << style_ << "</style>\n<defs>" << markers(px) << "</defs>\n";
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
