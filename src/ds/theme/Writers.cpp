#include "ds/theme/Writers.hpp"

#include <stdexcept>

namespace ds {
namespace theme {

// The stylesheet of plantuml-render's engine (shared.ts STYLE): one theming contract. Where each value goes; the values
// are the theme's.
const char* const SvgStyleSheet::kStylesheet = R"CSS(
  .pr-diagram { font-family: var(--pr-font, {{font-family}}); font-size: {{font-size}}px;
    --pr-text: {{text}}; --pr-muted: {{muted}}; --pr-stroke: {{stroke}}; --pr-edge: {{edge}}; --pr-box-fill: {{box-fill}}; --pr-container-fill: {{container-fill}}; --pr-container-stroke: {{container-stroke}};
    --pr-note-fill: {{note-fill}}; --pr-note-stroke: {{note-stroke}}; --pr-head-class: {{head-class}}; --pr-head-interface: {{head-interface}}; --pr-head-enum: {{head-enum}}; --pr-head-function: {{head-function}}; --pr-head-abstract: {{head-abstract}};
    --pr-badge-class: {{badge-class}}; --pr-badge-interface: {{badge-interface}}; --pr-badge-enum: {{badge-enum}}; --pr-badge-function: {{badge-function}}; --pr-badge-abstract: {{badge-abstract}}; --pr-badge-dataclass: {{badge-dataclass}}; --pr-badge-requirement: {{badge-requirement}}; --pr-badge-verification: {{badge-verification}}; --pr-badge-constraint: {{badge-constraint}};
    --pr-name: {{name}}; --pr-type: {{type}}; --pr-param: {{param}}; --pr-punct: {{punct}};
    --pr-vis-public: {{vis-public}}; --pr-vis-private: {{vis-private}}; --pr-vis-protected: {{vis-protected}}; --pr-vis-package: {{vis-package}};
    --pr-port-in: {{port-in}}; --pr-port-out: {{port-out}}; --pr-focus: {{focus}}; --pr-dimmed: {{dimmed}};
    --pr-stroke-width: {{stroke-width}}; --pr-container-stroke-width: {{container-stroke-width}}; --pr-edge-width: {{edge-width}}; --pr-edge-hit: {{edge-hit}};
    --pr-line-requirement: {{line-requirement}}; --pr-line-constraint: {{line-constraint}}; --pr-line-usecase: {{line-usecase}}; --pr-line-verification: {{line-verification}}; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .pr-diagram { --pr-text: {{text:dark}}; --pr-muted: {{muted:dark}}; --pr-stroke: {{stroke:dark}}; --pr-edge: {{edge:dark}}; --pr-box-fill: {{box-fill:dark}}; --pr-container-fill: {{container-fill:dark}}; --pr-container-stroke: {{container-stroke:dark}};
    --pr-note-fill: {{note-fill:dark}}; --pr-note-stroke: {{note-stroke:dark}}; --pr-head-class: {{head-class:dark}}; --pr-head-interface: {{head-interface:dark}}; --pr-head-enum: {{head-enum:dark}}; --pr-head-function: {{head-function:dark}}; --pr-head-abstract: {{head-abstract:dark}};
    --pr-badge-class: {{badge-class:dark}}; --pr-badge-interface: {{badge-interface:dark}}; --pr-badge-enum: {{badge-enum:dark}}; --pr-badge-function: {{badge-function:dark}}; --pr-badge-abstract: {{badge-abstract:dark}}; --pr-badge-dataclass: {{badge-dataclass:dark}}; --pr-badge-requirement: {{badge-requirement:dark}}; --pr-badge-verification: {{badge-verification:dark}}; --pr-badge-constraint: {{badge-constraint:dark}};
    --pr-name: {{name:dark}}; --pr-type: {{type:dark}}; --pr-param: {{param:dark}}; --pr-punct: {{punct:dark}}; --pr-vis-public: {{vis-public:dark}}; --pr-vis-private: {{vis-private:dark}}; --pr-vis-protected: {{vis-protected:dark}}; --pr-vis-package: {{vis-package:dark}}; --pr-port-in: {{port-in:dark}}; --pr-port-out: {{port-out:dark}}; --pr-focus: {{focus:dark}}; --pr-dimmed: {{dimmed:dark}}; --pr-line-requirement: {{line-requirement:dark}}; --pr-line-constraint: {{line-constraint:dark}}; --pr-line-usecase: {{line-usecase:dark}}; --pr-line-verification: {{line-verification:dark}}; } }
  [data-theme="dark"] .pr-diagram { --pr-text: {{text:dark}}; --pr-muted: {{muted:dark}}; --pr-stroke: {{stroke:dark}}; --pr-edge: {{edge:dark}}; --pr-box-fill: {{box-fill:dark}}; --pr-container-fill: {{container-fill:dark}}; --pr-container-stroke: {{container-stroke:dark}};
    --pr-note-fill: {{note-fill:dark}}; --pr-note-stroke: {{note-stroke:dark}}; --pr-head-class: {{head-class:dark}}; --pr-head-interface: {{head-interface:dark}}; --pr-head-enum: {{head-enum:dark}}; --pr-head-function: {{head-function:dark}}; --pr-head-abstract: {{head-abstract:dark}};
    --pr-badge-class: {{badge-class:dark}}; --pr-badge-interface: {{badge-interface:dark}}; --pr-badge-enum: {{badge-enum:dark}}; --pr-badge-function: {{badge-function:dark}}; --pr-badge-abstract: {{badge-abstract:dark}}; --pr-badge-dataclass: {{badge-dataclass:dark}}; --pr-badge-requirement: {{badge-requirement:dark}}; --pr-badge-verification: {{badge-verification:dark}}; --pr-badge-constraint: {{badge-constraint:dark}};
    --pr-name: {{name:dark}}; --pr-type: {{type:dark}}; --pr-param: {{param:dark}}; --pr-punct: {{punct:dark}}; --pr-vis-public: {{vis-public:dark}}; --pr-vis-private: {{vis-private:dark}}; --pr-vis-protected: {{vis-protected:dark}}; --pr-vis-package: {{vis-package:dark}}; --pr-port-in: {{port-in:dark}}; --pr-port-out: {{port-out:dark}}; --pr-focus: {{focus:dark}}; --pr-dimmed: {{dimmed:dark}}; --pr-line-requirement: {{line-requirement:dark}}; --pr-line-constraint: {{line-constraint:dark}}; --pr-line-usecase: {{line-usecase:dark}}; --pr-line-verification: {{line-verification:dark}}; }
  .pr-diagram text { fill: var(--pr-text); }
  .pr-diagram rect, .pr-diagram line.pr-sep { shape-rendering: crispEdges; }
  .pr-diagram rect, .pr-diagram path, .pr-diagram line { vector-effect: non-scaling-stroke; }
  .pr-diagram .pr-box > rect { fill: var(--pr-box-fill); stroke: var(--pr-stroke); stroke-width: var(--pr-stroke-width); }
  .pr-diagram .pr-head { fill: var(--pr-head-class); stroke: none; }
  .pr-diagram .pr-classifier-interface .pr-head { fill: var(--pr-head-interface); }
  .pr-diagram .pr-classifier-enum .pr-head { fill: var(--pr-head-enum); }
  .pr-diagram .pr-abstract .pr-head { fill: var(--pr-head-abstract); }
  .pr-diagram .pr-classifier-requirement .pr-head { fill: var(--pr-head-function); }
  .pr-diagram .pr-classifier-requirement .pr-badge { fill: var(--pr-badge-requirement); }
  .pr-diagram .pr-classifier-verification .pr-head { fill: var(--pr-head-interface); }
  .pr-diagram .pr-classifier-verification .pr-badge { fill: var(--pr-badge-verification); }
  .pr-diagram .pr-classifier-constraint .pr-head { fill: var(--pr-head-enum); }
  .pr-diagram .pr-classifier-constraint .pr-badge { fill: var(--pr-badge-constraint); }
  .pr-diagram .pr-badge { fill: var(--pr-badge-class); }
  .pr-diagram .pr-classifier-interface .pr-badge { fill: var(--pr-badge-interface); }
  .pr-diagram .pr-classifier-enum .pr-badge { fill: var(--pr-badge-enum); }
  .pr-diagram .pr-classifier-dataclass .pr-badge { fill: var(--pr-badge-dataclass); }
  .pr-diagram .pr-abstract .pr-badge { fill: var(--pr-badge-abstract); }
  .pr-diagram .pr-badge-text { fill: var(--pr-box-fill); font-size: 9.5px; font-weight: 700; }
  .pr-diagram .pr-offered .pr-badge-text { font-size: 8.5px; }
  .pr-diagram .pr-classifier-function .pr-badge { fill: var(--pr-badge-function); }
  .pr-diagram .pr-header { font-weight: 700; fill: var(--pr-name); }
  .pr-diagram .pr-abstract .pr-header { font-style: italic; }
  .pr-diagram .pr-stereo { fill: var(--pr-muted); font-size: 10.5px; }
  .pr-diagram .pr-sep { stroke: var(--pr-stroke); }
  .pr-diagram .pr-box .pr-header + .pr-sep { stroke-width: 2; stroke: var(--pr-badge-class); }
  .pr-diagram .pr-classifier-interface .pr-header + .pr-sep { stroke: var(--pr-badge-interface); }
  .pr-diagram .pr-classifier-dataclass .pr-header + .pr-sep { stroke: var(--pr-badge-dataclass); }
  .pr-diagram .pr-classifier-enum .pr-header + .pr-sep { stroke: var(--pr-badge-enum); }
  .pr-diagram .pr-classifier-namespace .pr-header + .pr-sep { stroke: var(--pr-stroke); stroke-width: 1; }
  .pr-diagram .pr-classifier-requirement .pr-header + .pr-sep { stroke: var(--pr-line-requirement); }
  .pr-diagram .pr-classifier-constraint .pr-header + .pr-sep { stroke: var(--pr-line-constraint); }
  .pr-diagram .pr-classifier-usecase .pr-header + .pr-sep { stroke: var(--pr-line-usecase); }
  .pr-diagram .pr-classifier-verification .pr-header + .pr-sep { stroke: var(--pr-line-verification); }
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

const char* const SequenceStyleSheet::kStylesheet = R"CSS(
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

// viewer/scene.html: where each value of the page goes; the values are the theme's.
const char* const ViewerVariables::kStylesheet = R"CSS(:root{
  --bg:{{bg}};--panel:{{panel}};--fg:{{fg}};--muted:{{muted}};--line:{{line}};--soft:{{soft}};
  --accent:{{accent}};--accent-soft:{{accent-soft}};--req:{{req}};--req-soft:{{req-soft}};
  --priv:{{priv}};--pkg:{{pkg}};--mod:{{mod}};--ifc:{{ifc}};
  --k-class:{{k-class}};--k-function:{{k-function}};--k-dataclass:{{k-dataclass}};--k-enum:{{k-enum}};--k-interface:{{k-interface}};--k-abstract:{{k-abstract}};--ok:{{ok}};--warn:{{warn}};--bad:{{bad}};
  --r-owns:{{r-owns}};--r-agg:{{r-agg}};--r-uses:{{r-uses}};--r-calls:{{r-calls}};
  --you:{{you}};--shadow:{{shadow}};--shadow-pop:{{shadow-pop}};--shadow-soft:{{shadow-soft}};
  --mono:ui-monospace,"JetBrains Mono","SFMono-Regular",Menlo,Consolas,monospace;
  --sans:"Inter",system-ui,-apple-system,"Segoe UI",sans-serif;
}
@media (prefers-color-scheme:dark){:root{
  --bg:{{bg:dark}};--panel:{{panel:dark}};--fg:{{fg:dark}};--muted:{{muted:dark}};--line:{{line:dark}};--soft:{{soft:dark}};
  --accent:{{accent:dark}};--accent-soft:{{accent-soft:dark}};--req:{{req:dark}};--req-soft:{{req-soft:dark}};
  --priv:{{priv:dark}};--pkg:{{pkg:dark}};--mod:{{mod:dark}};--ifc:{{ifc:dark}};
  --k-class:{{k-class:dark}};--k-function:{{k-function:dark}};--k-dataclass:{{k-dataclass:dark}};--k-enum:{{k-enum:dark}};--k-interface:{{k-interface:dark}};--k-abstract:{{k-abstract:dark}};--ok:{{ok:dark}};--warn:{{warn:dark}};--bad:{{bad:dark}};
  --r-owns:{{r-owns:dark}};--r-agg:{{r-agg:dark}};--r-uses:{{r-uses:dark}};--r-calls:{{r-calls:dark}};}}
)CSS";

// viewer/themes/night.css: where each value of the page goes; the values are the theme's.
const char* const ViewerNightVariables::kStylesheet = R"CSS(:root {
  --bg: {{bg}};          /* the canvas */
  --panel: {{panel}};       /* boxes, cards, buttons */
  --fg: {{fg}};          /* text and outlines */
  --muted: {{muted}};       /* secondary text */
  --line: {{line}};        /* thin rules */
  --soft: {{soft}};        /* hover */
  --accent: {{accent}};      /* selection, interfaces, the viewport in the minimap */
  --accent-soft: {{accent-soft}};
  --req: {{req}};         /* requirement chips */
  --req-soft: {{req-soft}};
  --priv: {{priv}};        /* private members */
  --pkg: {{pkg}};         /* a package */
  --mod: {{mod}};         /* a module */
  --ifc: {{ifc}};         /* an interface */
  --k-class: {{k-class}};     /* the stripe of each kind */
  --k-function: {{k-function}};
  --k-dataclass: {{k-dataclass}};
  --k-enum: {{k-enum}};
  --k-interface: {{k-interface}};
  --k-abstract: {{k-abstract}};
  --r-owns: {{r-owns}};      /* what a touched box composes, aggregates, calls and uses */
  --r-agg: {{r-agg}};
  --r-uses: {{r-uses}};
  --r-calls: {{r-calls}};
  --ok: {{ok}};          /* linked to a requirement */
  --warn: {{warn}};        /* linked to none */
  --bad: {{bad}};         /* what breaks a rule, in the logic perspective */
  --you: {{you}};         /* what the reader wrote, in a note */
  --shadow: {{shadow}};       /* under a card */
  --shadow-pop: {{shadow-pop}};    /* under a popup */
  --shadow-soft: {{shadow-soft}};  /* under a caption */
  --mono: ui-monospace, "JetBrains Mono", Menlo, Consolas, monospace;
  --sans: "Inter", system-ui, -apple-system, "Segoe UI", sans-serif;
}
)CSS";

// python/driftspec/live_page.py: where each value of the page goes; the values are the theme's.
const char* const LivePageVariables::kStylesheet =
    R"CSS(:root{--bg:{{bg}};--panel:{{panel}};--fg:{{fg}};--muted:{{muted}};--line:{{line}};--accent:{{accent}};--ok:{{ok}};--err:{{err}};--warn:{{warn}};--info:{{info}};--sel:{{sel}};--on-sel:{{on-sel}};
 font-family:system-ui,"Segoe UI",sans-serif;font-size:13px;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:{{bg:dark}};--panel:{{panel:dark}};--fg:{{fg:dark}};--muted:{{muted:dark}};--line:{{line:dark}};--accent:{{accent:dark}};--ok:{{ok:dark}};--err:{{err:dark}};--warn:{{warn:dark}};--info:{{info:dark}};--sel:{{sel:dark}};color-scheme:dark}}
)CSS";

// playground/web/index.html: where each value of the page goes; the values are the theme's.
const char* const PlaygroundVariables::kStylesheet = R"CSS(:root {
  --page: {{page:dark}}; --bg: {{bg:dark}}; --panel: {{panel:dark}}; --raised: {{raised:dark}}; --line: {{line:dark}};
  --rule: {{rule:dark}}; --fg: {{fg:dark}}; --muted: {{muted:dark}}; --faint: {{faint:dark}}; --soft: {{soft:dark}};
  --accent: {{accent:dark}};
  --c-req: {{c-req:dark}}; --c-cst: {{c-cst:dark}}; --c-rule: {{c-rule:dark}}; --c-seq: {{c-seq:dark}}; --c-state: {{c-state:dark}}; --c-fail: {{c-fail:dark}};
  --k-class: {{k-class:dark}}; --k-interface: {{k-interface:dark}}; --k-dataclass: {{k-dataclass:dark}}; --k-enum: {{k-enum:dark}}; --k-function: {{k-function:dark}}; --k-abstract: {{k-abstract:dark}};
  --s-fn: {{s-fn:dark}}; --s-pn: {{s-pn:dark}}; --s-ty: {{s-ty:dark}}; --s-kw: {{s-kw:dark}}; --s-str: {{s-str:dark}};
  --v-pub: {{v-pub:dark}}; --v-prot: {{v-prot:dark}}; --v-priv: {{v-priv:dark}};
  --r-inherits: {{r-inherits:dark}}; --r-has: {{r-has:dark}}; --r-refers: {{r-refers:dark}}; --r-uses: {{r-uses:dark}};
  --err: var(--c-fail); --warn: var(--c-rule); --info: var(--c-seq);
  --kw: var(--s-kw); --ty: var(--s-ty); --id: var(--c-req); --com: var(--faint); --str: var(--s-str);
  --mono: ui-monospace, "JetBrains Mono", "Cascadia Code", Menlo, Consolas, monospace;
  --sans: system-ui, -apple-system, "Segoe UI", sans-serif;
  --brand: {{brand:dark}}; --on-brand: {{on-brand:dark}}; --figtree: "Figtree", system-ui, -apple-system, "Segoe UI", sans-serif;
  --coach: {{coach:dark}}; --on-coach: {{on-coach:dark}}; --shadow-card: {{shadow-card:dark}}; --shadow-float: {{shadow-float:dark}}; --eye: {{eye:dark}}; --pupil: {{pupil:dark}};
  color-scheme: dark;
}
:root[data-theme="light"] {
  --page: {{page}}; --bg: {{bg}}; --panel: {{panel}}; --raised: {{raised}}; --line: {{line}};
  --rule: {{rule}}; --fg: {{fg}}; --muted: {{muted}}; --faint: {{faint}}; --soft: {{soft}};
  --accent: {{accent}};
  --c-req: {{c-req}}; --c-cst: {{c-cst}}; --c-rule: {{c-rule}}; --c-seq: {{c-seq}}; --c-state: {{c-state}}; --c-fail: {{c-fail}};
  --k-class: {{k-class}}; --k-interface: {{k-interface}}; --k-dataclass: {{k-dataclass}}; --k-enum: {{k-enum}}; --k-function: {{k-function}}; --k-abstract: {{k-abstract}};
  --s-fn: {{s-fn}}; --s-pn: {{s-pn}}; --s-ty: {{s-ty}}; --s-kw: {{s-kw}}; --s-str: {{s-str}};
  --v-pub: {{v-pub}}; --v-prot: {{v-prot}}; --v-priv: {{v-priv}};
  --r-inherits: {{r-inherits}}; --r-has: {{r-has}}; --r-refers: {{r-refers}}; --r-uses: {{r-uses}};
  --brand: {{brand}}; --on-brand: {{on-brand}};
  --coach: {{coach}}; --on-coach: {{on-coach}}; --shadow-card: {{shadow-card}};
  color-scheme: light;
}
)CSS";

// tools/design_page.py: where each value of the page goes; the values are the theme's.
const char* const DesignPageVariables::kStylesheet = R"CSS(:root {
  --bg: {{bg}}; --panel: {{panel}}; --ink: {{ink}}; --muted: {{muted}}; --rule: {{rule}};
  --accent: {{accent}}; --accent-soft: {{accent-soft}}; --mark: {{mark}}; --warn: {{warn}};
  --kw: {{kw}}; --ty: {{ty}}; --id: {{id}}; --cm: {{cm}}; --st: {{st}}; --ar: {{ar}};
  --sans: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "JetBrains Mono", "Cascadia Code", ui-monospace, Menlo, Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: {{bg:dark}}; --panel: {{panel:dark}}; --ink: {{ink:dark}}; --muted: {{muted:dark}}; --rule: {{rule:dark}};
  --accent: {{accent:dark}}; --accent-soft: {{accent-soft:dark}}; --mark: {{mark:dark}}; --warn: {{warn:dark}};
  --kw: {{kw:dark}}; --ty: {{ty:dark}}; --id: {{id:dark}}; --cm: {{cm:dark}}; --st: {{st:dark}}; --ar: {{ar:dark}}; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: {{bg:dark}}; --panel: {{panel:dark}}; --ink: {{ink:dark}}; --muted: {{muted:dark}}; --rule: {{rule:dark}};
  --accent: {{accent:dark}}; --accent-soft: {{accent-soft:dark}}; --mark: {{mark:dark}}; --warn: {{warn:dark}};
  --kw: {{kw:dark}}; --ty: {{ty:dark}}; --id: {{id:dark}}; --cm: {{cm:dark}}; --st: {{st:dark}}; --ar: {{ar:dark}}; color-scheme: dark }
)CSS";

std::string SvgStyleSheet::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.tokens()); }

std::string SequenceStyleSheet::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.tokens()); }

std::string ViewerVariables::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.page("viewer")); }

std::string ViewerNightVariables::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.page("viewer-night")); }

std::string LivePageVariables::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.page("live")); }

std::string PlaygroundVariables::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.page("playground")); }

std::string DesignPageVariables::write(const Theme& theme) const { return theme.filled(kStylesheet, theme.page("design-page")); }

std::map<std::string, std::unique_ptr<IThemeWriter>> ThemeWriterFactory::of() {
  std::map<std::string, std::unique_ptr<IThemeWriter>> out;
  out["svg"] = std::make_unique<SvgStyleSheet>();
  out["sequence"] = std::make_unique<SequenceStyleSheet>();
  out["viewer"] = std::make_unique<ViewerVariables>();
  out["viewer-night"] = std::make_unique<ViewerNightVariables>();
  out["live"] = std::make_unique<LivePageVariables>();
  out["playground"] = std::make_unique<PlaygroundVariables>();
  out["design-page"] = std::make_unique<DesignPageVariables>();
  return out;
}

}  // namespace theme
}  // namespace ds
