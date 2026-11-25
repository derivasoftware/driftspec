// A ds drawing at rest and lit on pointing (DS-491): one script for every page that shows `ds render -f svg` drawings.
// The playground loads it (playground/build.py copies it into the site); ds puts it into a viewer page beside the
// walkthroughs' mode (format::ViewerPage). At rest, each relation says its kind, the ones that touch a class or a closed
// box wait to be asked, and the tips are smaller and wear the colour of the card they touch; pointing at a box lights
// its relations and the boxes at their other end. What a page does around that (a selection it keeps, a card saying
// which interfaces a use goes through) it hands in: wireDrawing(svg, page) knows nothing else of the page.

// ---- the look of a drawing at rest and lit: interaction, written once (the static look is the drawing's own) ----
// :root svg weighs as the playground's .pane svg did, so a page's own rules meet it as they met those
const DRAWING_STYLE = `
:root svg .pr-edge-group.quiet { display: none; }
:root svg .pr-edge-group[data-rk="inherits"] { --rc: var(--r-inherits); }
:root svg .pr-edge-group[data-rk="has"] { --rc: var(--r-has); }
:root svg .pr-edge-group[data-rk="refers"] { --rc: var(--r-refers); }
:root svg .pr-edge-group[data-rk="uses"] { --rc: var(--r-uses); }
:root svg .pr-edge-group.lit .pr-edge { stroke: var(--rc); stroke-width: 1.6; }
:root svg .pr-edge-group.lit .pr-edge-label { fill: var(--rc); }
:root svg.quietable .pr-box { cursor: pointer; }
:root svg .pr-box.sel > rect { stroke: var(--fg); stroke-width: 1.6; }
:root svg .pr-box.rel > rect { stroke: var(--rc); stroke-width: 1.6; }
`;
if (typeof document !== "undefined" && !document.getElementById("ds-drawings")) {
  const style = document.createElement("style");
  style.id = "ds-drawings";
  style.textContent = DRAWING_STYLE;
  document.head.append(style);
}

// ---- a drawing at rest, decided (tests/integration/viewer runs this part as it is: keep it free of the page) ----
const EDGE_KIND = { inheritance: "inherits", realization: "inherits", composition: "has", aggregation: "refers", dependency: "uses", association: "has" };  // association: a structural bond between closed boxes (DS-230)
const UNIT = /pr-classifier-(class|dataclass|enum|package|module)\b/;  // what lights its relations when pointed at: a class, or a closed box
// a relation's kind from its edge's classes, and whether it waits to be asked: one that touches a class or a closed box
// does, unless it inherits; one between packages or interfaces stays. fromClass and toClass are the class attributes
// of the boxes at its ends, null for an end the drawing does not hold.
function relationAtRest(edgeClasses, fromClass, toClass) {
  const k = edgeClasses.map((c) => c.replace("pr-edge-", "")).find((c) => EDGE_KIND[c]);
  const rk = EDGE_KIND[k] || "uses";
  const betweenUnits = (fromClass !== null && UNIT.test(fromClass)) || (toClass !== null && UNIT.test(toClass));
  return { rk, quiet: !!betweenUnits && rk !== "inherits" };
}
// the tip of a relation wears the colour of the kind of the card it touches (DS-193)
const CARD_COLOUR = { class: "--k-class", dataclass: "--k-dataclass", enum: "--k-enum", interface: "--k-interface" };
function colourOfCard(cardClass) {
  const m = /pr-classifier-(\w+)/.exec(cardClass);
  return m && CARD_COLOUR[m[1]];
}
// the engine's tips are sized for print and grow with the line: here they are smaller, and fixed
const TIP_SCALE = 0.75;
const tipSize = (written) => (parseFloat(written) || 12) * TIP_SCALE;
// ---- a drawing at rest, decided: end ----

// ---- relations at rest and lit -----------------------------------------------------------

// page holds what is selected (page.selected, read and written as the drawing is pointed at and clicked), and may say
// chose() (a box was clicked), point(edges, at) and leave() (show and hide what it says of a use's interfaces). Returns
// light(id), which selects id and lights it, or null when there is no drawing or nothing in it waits to be asked.
function wireDrawing(svg, page) {
  if (!svg) return null;
  const box = (id) => svg.querySelector(`.pr-box[data-id="${CSS.escape(id)}"]`);
  let quietable = false;
  for (const g of svg.querySelectorAll(".pr-edge-group")) {
    const edge = g.querySelector(".pr-edge");
    const a = box(g.dataset.from), b = box(g.dataset.to);
    const rest = relationAtRest(edge ? [...edge.classList] : [], a ? a.getAttribute("class") : null, b ? b.getAttribute("class") : null);
    g.dataset.rk = rest.rk;
    g.dataset.quiet = rest.quiet ? "1" : "";
    if (g.dataset.quiet) quietable = true;
  }
  tintTips(svg, box);
  svg.classList.toggle("quietable", quietable);
  if (!quietable) return null;
  const light = (id) => {
    for (const el of svg.querySelectorAll(".pr-box.sel, .pr-box.rel")) { el.classList.remove("sel", "rel"); el.style.removeProperty("--rc"); }
    for (const g of svg.querySelectorAll(".pr-edge-group")) {
      const touches = id && (g.dataset.from === id || g.dataset.to === id);
      g.classList.toggle("lit", !!touches);
      g.classList.toggle("quiet", !!g.dataset.quiet && !touches);
      if (touches) {
        const other = box(g.dataset.from === id ? g.dataset.to : g.dataset.from);
        if (other && other.dataset.id !== id) {
          other.classList.add("rel");
          other.style.setProperty("--rc", getComputedStyle(g).getPropertyValue("--rc"));
        }
      }
    }
    if (id) box(id)?.classList.add("sel");
  };
  const point = (edges, at) => page.point?.(edges, at), leave = () => page.leave?.();
  // a use between two boxes says which interfaces it goes through (DS-266): the page shows it beside what is pointed at
  for (const g of svg.querySelectorAll('.pr-edge-group[data-ref-bond="uses"]')) {
    const said = g.querySelector(":scope > title");
    if (!said) continue;
    g.dataset.via = said.textContent; said.remove();  // the page's own card, not the browser's tooltip over it
    g.addEventListener("mouseenter", () => point([g], g));
    g.addEventListener("mouseleave", leave);
  }
  for (const b of svg.querySelectorAll(".pr-box")) {
    if (!UNIT.test(b.getAttribute("class"))) continue;
    b.addEventListener("mouseenter", () => { light(b.dataset.id); point([...svg.querySelectorAll(".pr-edge-group[data-via]")].filter((g) => g.dataset.from === b.dataset.id || g.dataset.to === b.dataset.id), b); });
    b.addEventListener("mouseleave", () => { light(page.selected); leave(); });
    b.addEventListener("click", (e) => { e.stopPropagation(); page.selected = page.selected === b.dataset.id ? null : b.dataset.id; light(page.selected); page.chose?.(); });
  }
  svg.addEventListener("click", () => { page.selected = null; light(null); });
  if (page.selected && !box(page.selected)) page.selected = null;
  light(page.selected);
  return (id) => { page.selected = id; light(id); };
}

// each marker is copied once per kind of card a tip touches, and the end points at its copy
function cardColour(el) {
  return el && colourOfCard(el.getAttribute("class"));
}
function shrinkTips(svg) {
  for (const m of svg.querySelectorAll("marker")) {
    if (m.dataset.shrunk) continue;
    m.setAttribute("markerUnits", "userSpaceOnUse");
    m.setAttribute("markerWidth", tipSize(m.getAttribute("markerWidth")));
    m.setAttribute("markerHeight", tipSize(m.getAttribute("markerHeight")));
    m.dataset.shrunk = "1";
  }
}

function tintTips(svg, box) {
  shrinkTips(svg);
  const defs = svg.querySelector("defs") || svg.insertBefore(document.createElementNS("http://www.w3.org/2000/svg", "defs"), svg.firstChild);
  for (const g of svg.querySelectorAll(".pr-edge-group")) {
    const ends = [box(g.dataset.from), box(g.dataset.to)].filter(Boolean);
    for (const path of g.querySelectorAll(".pr-edge")) {
      for (const [attr, atEnd] of [["marker-start", false], ["marker-end", true]]) {
        const ref = path.getAttribute(attr);
        const id = ref && /url\(#([^)]+)\)/.exec(ref)?.[1];
        if (!id || !ends.length) continue;
        let p;
        try { p = path.getPointAtLength(atEnd ? path.getTotalLength() : 0); } catch { continue; }
        // the card the tip touches is the nearer end
        const dist = (el) => { const b = el.getBBox(); const dx = Math.max(b.x - p.x, 0, p.x - b.x - b.width); const dy = Math.max(b.y - p.y, 0, p.y - b.y - b.height); return dx * dx + dy * dy; };
        const touched = ends.reduce((a, b) => (dist(b) < dist(a) ? b : a));
        const colour = cardColour(touched);
        if (!colour) continue;
        const copy = `${id}--${colour.slice(4)}`;
        if (!svg.getElementById(copy)) {
          const src = svg.getElementById(id);
          if (!src) continue;
          const m = src.cloneNode(true);
          m.id = copy;
          for (const shape of m.querySelectorAll("path, polygon, polyline")) {
            shape.style.stroke = `var(${colour})`;
            if (shape.classList.contains("pr-filled")) shape.style.fill = `var(${colour})`;
          }
          defs.append(m);
        }
        path.setAttribute(attr, `url(#${copy})`);
      }
    }
  }
}
