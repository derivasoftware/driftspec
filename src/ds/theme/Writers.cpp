#include "ds/theme/Writers.hpp"

#include <stdexcept>

namespace ds {
namespace theme {

namespace {

/// A stylesheet with its values put in: {{name}} is a token's light value, {{name:dark}} its dark one (the light one
/// when it has none), {{font-family}} and {{font-size}} the font's.
std::string filled(const std::string& stylesheet, const Theme& theme) {
  std::string out;
  out.reserve(stylesheet.size() + stylesheet.size() / 4);
  std::size_t at = 0;
  while (true) {
    const std::size_t open = stylesheet.find("{{", at);
    if (open == std::string::npos) break;
    const std::size_t close = stylesheet.find("}}", open);
    if (close == std::string::npos) throw std::logic_error("a placeholder of the stylesheet is not closed");
    out.append(stylesheet, at, open - at);
    std::string name = stylesheet.substr(open + 2, close - open - 2);
    if (name == "font-family") {
      out += theme.font().family;
    } else if (name == "font-size") {
      out += std::to_string(theme.font().sizePx);
    } else {
      const bool dark = name.size() > 5 && name.compare(name.size() - 5, 5, ":dark") == 0;
      if (dark) name.resize(name.size() - 5);
      const Token& token = theme.token(name);
      out += dark && !token.dark.empty() ? token.dark : token.light;
    }
    at = close + 2;
  }
  out.append(stylesheet, at, std::string::npos);
  return out;
}

// The stylesheet of plantuml-render's engine (shared.ts STYLE): one theming contract. Where each value goes; the values
// are the theme's.
const char* const kStyle = R"CSS(
  .pr-diagram { font-family: var(--pr-font, {{font-family}}); font-size: {{font-size}}px;
    --pr-text: {{text}}; --pr-muted: {{muted}}; --pr-stroke: {{stroke}}; --pr-edge: {{edge}}; --pr-box-fill: {{box-fill}}; --pr-container-fill: {{container-fill}}; --pr-container-stroke: {{container-stroke}};
    --pr-note-fill: {{note-fill}}; --pr-note-stroke: {{note-stroke}}; --pr-head-class: {{head-class}}; --pr-head-interface: {{head-interface}}; --pr-head-enum: {{head-enum}}; --pr-head-function: {{head-function}}; --pr-head-abstract: {{head-abstract}};
    --pr-badge-class: {{badge-class}}; --pr-badge-interface: {{badge-interface}}; --pr-badge-enum: {{badge-enum}}; --pr-badge-function: {{badge-function}}; --pr-badge-abstract: {{badge-abstract}};
    --pr-name: {{name}}; --pr-type: {{type}}; --pr-param: {{param}}; --pr-punct: {{punct}};
    --pr-vis-public: {{vis-public}}; --pr-vis-private: {{vis-private}}; --pr-vis-protected: {{vis-protected}}; --pr-vis-package: {{vis-package}};
    --pr-port-in: {{port-in}}; --pr-port-out: {{port-out}}; --pr-focus: {{focus}}; --pr-dimmed: {{dimmed}};
    --pr-stroke-width: {{stroke-width}}; --pr-container-stroke-width: {{container-stroke-width}}; --pr-edge-width: {{edge-width}}; --pr-edge-hit: {{edge-hit}}; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-text: {{text:dark}}; --pr-muted: {{muted:dark}}; --pr-stroke: {{stroke:dark}}; --pr-edge: {{edge:dark}}; --pr-box-fill: {{box-fill:dark}}; --pr-container-fill: {{container-fill:dark}}; --pr-container-stroke: {{container-stroke:dark}};
    --pr-note-fill: {{note-fill:dark}}; --pr-note-stroke: {{note-stroke:dark}}; --pr-head-class: {{head-class:dark}}; --pr-head-interface: {{head-interface:dark}}; --pr-head-enum: {{head-enum:dark}}; --pr-head-function: {{head-function:dark}}; --pr-head-abstract: {{head-abstract:dark}};
    --pr-badge-class: {{badge-class:dark}}; --pr-badge-interface: {{badge-interface:dark}}; --pr-badge-enum: {{badge-enum:dark}}; --pr-badge-function: {{badge-function:dark}}; --pr-badge-abstract: {{badge-abstract:dark}};
    --pr-name: {{name:dark}}; --pr-type: {{type:dark}}; --pr-param: {{param:dark}}; --pr-punct: {{punct:dark}}; --pr-vis-public: {{vis-public:dark}}; --pr-vis-private: {{vis-private:dark}}; --pr-vis-protected: {{vis-protected:dark}}; --pr-vis-package: {{vis-package:dark}}; --pr-port-in: {{port-in:dark}}; --pr-port-out: {{port-out:dark}}; --pr-focus: {{focus:dark}}; --pr-dimmed: {{dimmed:dark}}; } }
  [data-theme="dark"] .pr-diagram { --pr-text: {{text:dark}}; --pr-muted: {{muted:dark}}; --pr-stroke: {{stroke:dark}}; --pr-edge: {{edge:dark}}; --pr-box-fill: {{box-fill:dark}}; --pr-container-fill: {{container-fill:dark}}; --pr-container-stroke: {{container-stroke:dark}};
    --pr-note-fill: {{note-fill:dark}}; --pr-note-stroke: {{note-stroke:dark}}; --pr-head-class: {{head-class:dark}}; --pr-head-interface: {{head-interface:dark}}; --pr-head-enum: {{head-enum:dark}}; --pr-head-function: {{head-function:dark}}; --pr-head-abstract: {{head-abstract:dark}};
    --pr-badge-class: {{badge-class:dark}}; --pr-badge-interface: {{badge-interface:dark}}; --pr-badge-enum: {{badge-enum:dark}}; --pr-badge-function: {{badge-function:dark}}; --pr-badge-abstract: {{badge-abstract:dark}};
    --pr-name: {{name:dark}}; --pr-type: {{type:dark}}; --pr-param: {{param:dark}}; --pr-punct: {{punct:dark}}; --pr-vis-public: {{vis-public:dark}}; --pr-vis-private: {{vis-private:dark}}; --pr-vis-protected: {{vis-protected:dark}}; --pr-vis-package: {{vis-package:dark}}; --pr-port-in: {{port-in:dark}}; --pr-port-out: {{port-out:dark}}; --pr-focus: {{focus:dark}}; --pr-dimmed: {{dimmed:dark}}; }
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
  .pr-diagram .pr-offered .pr-badge-text { font-size: 8.5px; }
  .pr-diagram .pr-classifier-function .pr-badge { fill: var(--pr-badge-function); }
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
  .pr-diagram { --pr-added: {{added}}; --pr-removed: {{removed}}; --pr-changed: {{changed}}; --pr-renamed: {{renamed}}; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-added: {{added:dark}}; --pr-removed: {{removed:dark}}; --pr-changed: {{changed:dark}}; --pr-renamed: {{renamed:dark}}; } }
  [data-theme="dark"] .pr-diagram { --pr-added: {{added:dark}}; --pr-removed: {{removed:dark}}; --pr-changed: {{changed:dark}}; --pr-renamed: {{renamed:dark}}; }
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
  .pr-diagram .pr-edge-change-changed { stroke: var(--pr-changed); stroke-width: 2; } .pr-diagram .pr-edge-change-renamed { stroke: var(--pr-renamed); stroke-width: 2; }
)CSS";

const char* const kSequenceStyle = R"CSS(
  .pr-diagram .pr-lifeline-line { stroke: var(--pr-stroke); stroke-dasharray: 4 4; }
  .pr-diagram .pr-lifeline-head > rect { fill: var(--pr-head-class); stroke: var(--pr-stroke); }
  .pr-diagram .pr-actor circle, .pr-diagram .pr-actor path { fill: none; stroke: var(--pr-text); stroke-width: 1.4; stroke-linecap: round; }
  .pr-diagram .pr-msg { stroke: var(--pr-edge); fill: none; pointer-events: none; }
  .pr-diagram .pr-msg-dashed { stroke-dasharray: 6 4; }
  .pr-diagram .pr-msg-label { fill: var(--pr-text); font-size: 11.5px; text-anchor: middle; paint-order: stroke; stroke: var(--pr-box-fill); stroke-width: 3px; stroke-linejoin: round; }
  .pr-diagram .pr-msg-self .pr-msg-label { text-anchor: start; }
  .pr-diagram { --pr-frame-alt: {{frame-alt}}; --pr-frame-loop: {{frame-loop}}; --pr-frame-opt: {{frame-opt}}; --pr-frame-other: {{frame-other}}; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-frame-alt: {{frame-alt:dark}}; --pr-frame-loop: {{frame-loop:dark}}; --pr-frame-opt: {{frame-opt:dark}}; --pr-frame-other: {{frame-other:dark}}; } }
  [data-theme="dark"] .pr-diagram { --pr-frame-alt: {{frame-alt:dark}}; --pr-frame-loop: {{frame-loop:dark}}; --pr-frame-opt: {{frame-opt:dark}}; --pr-frame-other: {{frame-other:dark}}; }
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

}  // namespace

std::string SvgStyleSheet::write(const Theme& theme) const { return filled(kStyle, theme); }

std::string SequenceStyleSheet::write(const Theme& theme) const { return filled(kSequenceStyle, theme); }

std::map<std::string, std::unique_ptr<IThemeWriter>> ThemeWriterFactory::of() {
  std::map<std::string, std::unique_ptr<IThemeWriter>> out;
  out["svg"] = std::make_unique<SvgStyleSheet>();
  out["sequence"] = std::make_unique<SequenceStyleSheet>();
  return out;
}

}  // namespace theme
}  // namespace ds
