#!/usr/bin/env python3
"""A browsable page of a DriftSpec design: every diagram beside the text it comes from.

    tools/design_page.py --workspace ds=design --workspace hola=../hola --out atlas.html

Everything is computed by `ds` (query, render, export); this only puts it on one page,
self-contained, with no server. --fragment leaves out the document skeleton, for hosts
that wrap the page themselves.
"""

import argparse
import html
import json
import re
import subprocess
import sys
from pathlib import Path

KIND_ORDER = ["hld", "interface", "state", "sequence", "class", "requirement"]


def ds(binary, *args, check=False):
    done = subprocess.run([binary, *args], capture_output=True, text=True)
    if check and done.returncode != 0:
        sys.exit(f"ds {' '.join(args)}: {done.stderr or done.stdout}")
    return done.stdout


def group_of(diagram):
    if diagram["id"].startswith("HLD_"):
        return "hld"
    return diagram["detail"] if diagram["detail"] in KIND_ORDER else "class"


def text_of(diagram_fields, where):
    """Where what a diagram asks for is written: the behaviour it includes, as it is written there."""
    first = diagram_fields.get("include", "").split(",")[0].strip()
    subject, _, scenario = first.partition(" / ")
    candidates = [first, f"{subject}::sequence/{scenario}", f"{subject}::sequence", f"{subject}::state", subject]
    if diagram_fields.get("kind") == "sequence" and not scenario:
        candidates = [f"{subject}::sequence", first]
    if diagram_fields.get("kind") == "state":
        candidates = [f"{subject}::state", first]
    for candidate in candidates:
        if candidate in where:
            return where[candidate]
    return None


def workspace_data(binary, name, root):
    root = Path(root).resolve()
    model = json.loads(ds(binary, "export", str(root), check=True))
    fields = {entity["id"]: entity.get("fields", {}) for entity in model["entities"]}
    where = {}
    for entity in model["entities"]:
        location = entity.get("location") or {}
        if entity.get("expected") or not location.get("file"):
            continue
        where[entity["id"]] = [str(Path(location["file"]).resolve().relative_to(root)), location.get("line", 1)]

    files = {}
    for path in sorted(root.rglob("*.ds")):
        files[str(path.relative_to(root))] = path.read_text(encoding="utf-8")

    diagrams = []
    for diagram in json.loads(ds(binary, "query", str(root), "diagrams", "--json")):
        svg = ds(binary, "render", str(root), diagram["id"], "--svg")
        shown = json.loads(ds(binary, "query", str(root), "shows", diagram["id"], "--json") or "[]")
        ids = [item["id"] for item in shown if item["id"] in where]
        own = where.get("diagram:" + diagram["id"])
        main = text_of(fields.get("diagram:" + diagram["id"], {}), where) or (where.get(ids[-1]) if ids else own)
        related = [main[0]] if main else []
        for entity in ids:
            file = where[entity][0]
            if file not in related:
                related.append(file)
        diagrams.append(
            {
                "name": diagram["id"],
                "group": group_of(diagram),
                "svg": svg,
                "main": main,
                "declared": own,
                "related": related,
            }
        )
    diagrams.sort(key=lambda d: (KIND_ORDER.index(d["group"]), d["name"]))

    findings = [
        {
            "code": f["code"],
            "severity": f["severity"],
            "message": f["message"],
            "file": str(Path(f["location"]["file"]).resolve().relative_to(root)) if f["location"].get("file") else "",
            "line": f["location"].get("line", 1),
        }
        for f in model.get("findings", [])
    ]
    counts = {}
    for entity in model["entities"]:
        if not entity.get("expected"):
            counts[entity["kind"]] = counts.get(entity["kind"], 0) + 1
    return {
        "name": name,
        "diagrams": diagrams,
        "files": files,
        "where": where,
        "findings": findings,
        "counts": counts,
        "relations": len(model["relations"]),
    }


STYLE = """
/* Layout: a drafting table. The index on the left, the drawing and its text side by side. */
:root {
  --bg: #eef1ee; --panel: #fbfcfa; --ink: #18211e; --muted: #5d6b65; --rule: #cfd7d1;
  --accent: #0b6e69; --accent-soft: #d5ebe7; --mark: #fdf0c4; --warn: #a8530a;
  --kw: #7a3e9d; --ty: #0b6e69; --id: #a8530a; --cm: #7a8880; --st: #2f6f3e; --ar: #5d6b65;
  --sans: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "JetBrains Mono", "Cascadia Code", ui-monospace, Menlo, Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #0f1614; --panel: #161f1c; --ink: #e4ebe7; --muted: #93a39c; --rule: #2b3833;
  --accent: #58c4b8; --accent-soft: #1b3a36; --mark: #4a3f14; --warn: #e5a15e;
  --kw: #c79bf0; --ty: #58c4b8; --id: #e5a15e; --cm: #7d8d86; --st: #8fce9d; --ar: #93a39c; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #0f1614; --panel: #161f1c; --ink: #e4ebe7; --muted: #93a39c; --rule: #2b3833;
  --accent: #58c4b8; --accent-soft: #1b3a36; --mark: #4a3f14; --warn: #e5a15e;
  --kw: #c79bf0; --ty: #58c4b8; --id: #e5a15e; --cm: #7d8d86; --st: #8fce9d; --ar: #93a39c; color-scheme: dark }
html, body { height: 100% }
body { background: var(--bg); color: var(--ink); font: 14px/1.45 var(--sans); }
.app { display: grid; grid-template-columns: 250px minmax(0, 1fr); grid-template-rows: auto minmax(0, 1fr); height: 100% }
header { grid-column: 1 / -1; display: flex; flex-wrap: wrap; gap: 12px 20px; align-items: center;
  padding-block: 10px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
  background: var(--panel); border-bottom: 1px solid var(--rule) }
h1 { font: 600 15px/1.2 var(--mono); margin: 0; text-wrap: balance }
.stats { color: var(--muted); font: 12px var(--mono); font-variant-numeric: tabular-nums; min-width: 0 }
.stats b { color: var(--ink); font-weight: 600 }
.stats .bad { color: var(--warn) }
.seg { display: inline-flex; border: 1px solid var(--rule); border-radius: 5px; overflow: hidden }
.seg button { font: 12px var(--sans); border: 0; background: transparent; color: var(--muted); padding: 5px 11px; cursor: pointer }
.seg button + button { border-left: 1px solid var(--rule) }
.seg button[aria-pressed="true"] { background: var(--accent-soft); color: var(--ink); font-weight: 600 }
button:focus-visible, a:focus-visible, .nav-item:focus-visible { outline: 2px solid var(--accent); outline-offset: -2px }
.push { margin-left: auto }
nav { overflow: auto; background: var(--panel); border-right: 1px solid var(--rule); padding-block: 8px;
  padding-left: max(0px, env(safe-area-inset-left, 0px)) }
nav h2 { font: 600 11px/1 var(--sans); letter-spacing: .07em; text-transform: uppercase; color: var(--muted); margin: 14px 14px 6px }
.nav-item { display: block; width: 100%; text-align: left; border: 0; background: transparent; color: var(--ink);
  font: 12px/1.3 var(--mono); padding: 4px 14px; cursor: pointer; overflow-wrap: anywhere }
.nav-item:hover { background: var(--accent-soft) }
.nav-item[aria-current="true"] { background: var(--accent-soft); box-shadow: inset 2px 0 0 var(--accent); font-weight: 600 }
.nav-item small { color: var(--muted); font-weight: 400 }
main { display: grid; min-height: 0; min-width: 0; grid-template-columns: minmax(0, 1.25fr) minmax(0, 1fr) }
main.only-drawing { grid-template-columns: minmax(0, 1fr) } main.only-drawing .text { display: none }
main.only-text { grid-template-columns: minmax(0, 1fr) } main.only-text .drawing { display: none }
.pane { display: grid; grid-template-rows: auto minmax(0, 1fr); min-height: 0; min-width: 0 }
.bar { display: flex; flex-wrap: wrap; gap: 6px 10px; align-items: center; padding: 7px 12px; background: var(--panel);
  border-bottom: 1px solid var(--rule); font: 12px var(--mono); min-width: 0 }
.bar strong { font-weight: 600; overflow-wrap: anywhere }
.bar .tool { border: 1px solid var(--rule); background: transparent; color: var(--ink); border-radius: 4px; padding: 1px 8px; cursor: pointer; font: 12px var(--mono) }
.chips { display: flex; flex-wrap: wrap; gap: 4px; min-width: 0 }
.chip { border: 1px solid var(--rule); background: transparent; color: var(--muted); border-radius: 3px; padding: 0 6px; cursor: pointer; font: 11px/1.6 var(--mono) }
.chip[aria-current="true"] { color: var(--ink); border-color: var(--accent); background: var(--accent-soft) }
.canvas { overflow: auto; padding: 14px; min-width: 0 }
.canvas svg { display: block; margin: 0 auto }
.canvas [data-id] { cursor: pointer }
.text { border-left: 1px solid var(--rule) }
.code { overflow: auto; background: var(--panel); font: 12px/1.55 var(--mono); padding-block: 8px; min-width: 0 }
.line { display: grid; grid-template-columns: 46px auto; width: max-content; min-width: 100% }
.line .n { color: var(--cm); text-align: right; padding-right: 12px; user-select: none; font-variant-numeric: tabular-nums }
.line .c { white-space: pre; padding-right: 16px }
.line.hit { background: var(--mark) }
.kw { color: var(--kw); font-weight: 600 } .ty { color: var(--ty) } .id { color: var(--id) }
.cm { color: var(--cm); font-style: italic } .st { color: var(--st) } .ar { color: var(--ar) }
a.link { color: var(--id); text-decoration: underline; text-underline-offset: 2px; cursor: pointer }
.findings { padding: 14px; overflow: auto } .finding { padding: 6px 0; border-bottom: 1px solid var(--rule); font: 12px var(--mono); overflow-wrap: anywhere }
.finding b { color: var(--warn) }
.empty { color: var(--muted); padding: 24px; max-width: 60ch }
@media (max-width: 860px) {
  html, body { height: auto }
  .app { grid-template-columns: minmax(0, 1fr); height: auto; padding-left: 0 }
  nav { border-right: 0; border-bottom: 1px solid var(--rule); max-height: 240px; padding-left: max(0px, env(safe-area-inset-left, 0px)); padding-right: max(0px, env(safe-area-inset-right, 0px)) }
  main, main.only-drawing, main.only-text { grid-template-columns: minmax(0, 1fr) }
  .text { border-left: 0; border-top: 1px solid var(--rule) }
  .canvas, .code { max-height: 70vh }
}
"""

SCRIPT = r"""
const DATA = JSON.parse(document.getElementById('atlas-data').textContent);
const GROUPS = {hld: 'Alto nivel', interface: 'Interfaces', state: 'Navegación y estados', sequence: 'Secuencias', class: 'Clases', requirement: 'Requisitos'};
const KEYWORDS = /^(package|module|extern|interface|operation|class|dataclass|enum|function|sequence|state|view|initial|local|participant|return|loop|alt|else|opt|requirement|verification|constraint|diagram|on|operator|static|abstract|const|owns|refs|shares)$/;
const state = {ws: 0, kind: 'd', item: 0, zoom: 0, layout: 'both', file: null, hit: null};
const $ = (id) => document.getElementById(id);
const esc = (s) => s.replace(/[&<>]/g, (c) => ({'&': '&amp;', '<': '&lt;', '>': '&gt;'}[c]));
const ws = () => DATA[state.ws];

function highlight(line) {
  const comment = line.indexOf('//');
  const code = comment >= 0 ? line.slice(0, comment) : line;
  const rest = comment >= 0 ? line.slice(comment) : '';
  const out = code.replace(/"[^"]*"|@[A-Za-z_]+|->|\b(?:S?REQ|S?VER|CST)-\d+-\d+\b|[A-Za-z_][A-Za-z0-9_-]*(?:@[A-Za-z_][\w-]*)*(?:::[A-Za-z_][\w<>-]*)*|[^"@\-A-Za-z_]+|./g, (tok) => {
    if (tok[0] === '"') return `<span class="st">${esc(tok)}</span>`;
    if (tok[0] === '@') return `<span class="kw">${esc(tok)}</span>`;
    if (tok === '->') return `<span class="ar">${esc(tok)}</span>`;
    if (/^(S?REQ|S?VER|CST)-/.test(tok)) return ws().where[tok] ? `<a class="link id" data-go="${esc(tok)}">${esc(tok)}</a>` : `<span class="id">${esc(tok)}</span>`;
    if (KEYWORDS.test(tok)) return `<span class="kw">${esc(tok)}</span>`;
    if (/^[A-Z]/.test(tok) || tok.includes('::') || tok.includes('@')) return `<span class="ty">${esc(tok)}</span>`;
    return esc(tok);
  });
  return out + (rest ? `<span class="cm">${esc(rest)}</span>` : '');
}

function showFile(file, line) {
  state.file = file; state.hit = line || null;
  const text = ws().files[file];
  if (text === undefined) { $('code').innerHTML = '<p class="empty">Este dibujo no tiene un fichero asociado.</p>'; $('file').textContent = ''; return; }
  $('file').textContent = file;
  $('code').innerHTML = text.split('\n').map((l, i) => `<div class="line${i + 1 === line ? ' hit' : ''}" data-line="${i + 1}"><span class="n">${i + 1}</span><span class="c">${highlight(l) || ' '}</span></div>`).join('');
  document.querySelectorAll('#chips .chip').forEach((c) => c.setAttribute('aria-current', c.dataset.file === file));
  const target = line ? $('code').querySelector(`[data-line="${line}"]`) : null;
  $('code').scrollTop = target ? Math.max(0, target.offsetTop - $('code').offsetTop - 80) : 0;
}

function jump(id) {
  const place = ws().where[id];
  if (!place) return;
  if (state.layout === 'drawing') setLayout('both');
  showFile(place[0], place[1]);
}

function applyZoom() {
  const svg = $('canvas').querySelector('svg');
  if (!svg) return;
  const natural = parseFloat(svg.getAttribute('width')) || 800;
  if (state.zoom === 0) { svg.style.maxWidth = '100%'; svg.style.width = natural + 'px'; svg.style.height = 'auto'; $('zoomv').textContent = 'ajustar'; return; }
  svg.style.maxWidth = 'none'; svg.style.width = natural * state.zoom + 'px'; svg.style.height = 'auto';
  $('zoomv').textContent = Math.round(state.zoom * 100) + ' %';
}

function hashOf() {
  const clean = (s) => s.replace(/[^A-Za-z0-9]+/g, '-');
  if (state.kind === 'd') return `d-${clean(ws().name)}-${clean(ws().diagrams[state.item].name)}`;
  return state.kind === 'f' ? `f-${clean(ws().name)}-${state.item}` : `r-${clean(ws().name)}`;
}

function render() {
  const w = ws();
  document.querySelectorAll('.nav-item').forEach((n) => n.setAttribute('aria-current', n.dataset.kind === state.kind && +n.dataset.item === state.item));
  $('main').className = state.layout === 'drawing' ? 'only-drawing' : state.layout === 'text' ? 'only-text' : '';
  const canvas = $('canvas');
  if (state.kind === 'd') {
    const d = w.diagrams[state.item];
    $('title').textContent = d.name;
    $('tools').hidden = false;
    canvas.className = 'canvas';
    canvas.innerHTML = d.svg;
    applyZoom();
    $('chips').innerHTML = d.related.map((f) => `<button class="chip" data-file="${esc(f)}">${esc(f)}</button>`).join('');
    const place = d.main || d.declared;
    place ? showFile(place[0], place[1]) : showFile(null);
  } else if (state.kind === 'f') {
    const f = Object.keys(w.files)[state.item];
    $('title').textContent = f;
    $('tools').hidden = true;
    const here = w.diagrams.filter((d) => d.related.includes(f));
    canvas.className = 'findings';
    canvas.innerHTML = here.length ? `<p>Este fichero aparece en:</p>` + here.map((d) => `<button class="nav-item" data-open="${esc(d.name)}">${esc(d.name)}</button>`).join('') : '<p class="empty">Ningún diagrama dibuja lo que declara este fichero.</p>';
    $('chips').innerHTML = '';
    showFile(f);
  } else {
    $('title').textContent = 'Hallazgos del modelo';
    $('tools').hidden = true;
    canvas.className = 'findings';
    canvas.innerHTML = w.findings.length ? w.findings.map((f) => `<div class="finding"><b>${esc(f.code)}</b> <a class="link" data-file="${esc(f.file)}" data-line="${f.line}">${esc(f.file)}:${f.line}</a><br>${esc(f.message)}</div>`).join('') : '<p class="empty">El modelo es coherente: <code>ds check</code> no da ningún hallazgo.</p>';
    $('chips').innerHTML = '';
    showFile(null);
  }
  const hash = hashOf();
  if (location.hash.slice(1) !== hash) location.hash = hash;
}

function setLayout(layout) {
  state.layout = layout;
  document.querySelectorAll('#layout button').forEach((b) => b.setAttribute('aria-pressed', b.dataset.layout === layout));
  $('main').className = layout === 'drawing' ? 'only-drawing' : layout === 'text' ? 'only-text' : '';
}

function buildNav() {
  const w = ws();
  const c = w.counts;
  $('stats').innerHTML = `<b>${c.class || 0}</b> clases · <b>${c.method || 0}</b> métodos · <b>${c.function || 0}</b> funciones · <b>${c.interface || 0}</b> interfaces · <b>${c.sequence || 0}</b> secuencias · <b>${c.requirement || 0}</b> requisitos · <span class="${w.findings.length ? 'bad' : ''}"><b>${w.findings.length}</b> hallazgos</span>`;
  let html = '';
  for (const key of Object.keys(GROUPS)) {
    const items = w.diagrams.map((d, i) => [d, i]).filter(([d]) => d.group === key);
    if (!items.length) continue;
    html += `<h2>${GROUPS[key]}</h2>` + items.map(([d, i]) => `<button class="nav-item" data-kind="d" data-item="${i}">${esc(d.name)}</button>`).join('');
  }
  html += '<h2>El DriftSpec</h2>' + Object.keys(w.files).map((f, i) => `<button class="nav-item" data-kind="f" data-item="${i}">${esc(f)}</button>`).join('');
  html += `<h2>Comprobación</h2><button class="nav-item" data-kind="r" data-item="0">Hallazgos <small>(${w.findings.length})</small></button>`;
  $('nav').innerHTML = html;
}

function openHash() {
  const hash = location.hash.slice(1);
  if (!hash) return false;
  const clean = (s) => s.replace(/[^A-Za-z0-9]+/g, '-');
  for (let wi = 0; wi < DATA.length; wi++) {
    const pre = clean(DATA[wi].name);
    const di = DATA[wi].diagrams.findIndex((d) => hash === `d-${pre}-${clean(d.name)}`);
    if (di >= 0) { state.ws = wi; state.kind = 'd'; state.item = di; return true; }
    const m = hash.match(new RegExp(`^f-${pre}-(\\d+)$`));
    if (m) { state.ws = wi; state.kind = 'f'; state.item = +m[1]; return true; }
    if (hash === `r-${pre}`) { state.ws = wi; state.kind = 'r'; state.item = 0; return true; }
  }
  return false;
}

document.addEventListener('click', (e) => {
  const t = e.target;
  const item = t.closest('.nav-item');
  if (item && item.dataset.open) { const i = ws().diagrams.findIndex((d) => d.name === item.dataset.open); state.kind = 'd'; state.item = i; state.zoom = 0; render(); return; }
  if (item) { state.kind = item.dataset.kind; state.item = +item.dataset.item; state.zoom = 0; render(); return; }
  const ws_ = t.closest('#spaces button');
  if (ws_) { state.ws = +ws_.dataset.ws; state.kind = 'd'; state.item = 0; state.zoom = 0; document.querySelectorAll('#spaces button').forEach((b) => b.setAttribute('aria-pressed', b === ws_)); buildNav(); render(); return; }
  const lay = t.closest('#layout button');
  if (lay) { setLayout(lay.dataset.layout); return; }
  const chip = t.closest('.chip');
  if (chip) { showFile(chip.dataset.file); return; }
  const go = t.closest('[data-go]');
  if (go) { jump(go.dataset.go); return; }
  const fl = t.closest('a.link[data-file]');
  if (fl) { showFile(fl.dataset.file, +fl.dataset.line); return; }
  if (t.closest('#zin')) { const base = state.zoom || 1; state.zoom = Math.min(4, base * 1.25); applyZoom(); return; }
  if (t.closest('#zout')) { const base = state.zoom || 1; state.zoom = Math.max(0.2, base / 1.25); applyZoom(); return; }
  if (t.closest('#zfit')) { state.zoom = 0; applyZoom(); return; }
  if (t.closest('#z100')) { state.zoom = 1; applyZoom(); return; }
  const box = t.closest('[data-id]');
  if (box && t.closest('#canvas')) jump(box.getAttribute('data-id'));
});
window.addEventListener('hashchange', () => { if (openHash()) { buildNav(); render(); } });

const opened = openHash();
$('spaces').innerHTML = DATA.length > 1 ? DATA.map((w, i) => `<button data-ws="${i}" aria-pressed="${i === state.ws}">${esc(w.name)}</button>`).join('') : '';
$('spaces').hidden = DATA.length < 2;
buildNav();
if (!opened) { const first = ws().diagrams.findIndex((d) => d.group === 'hld'); state.item = Math.max(0, first); }
render();
"""

BODY = """
<div class="app">
  <header>
    <h1>{heading}</h1>
    <div class="seg" id="spaces" role="group" aria-label="Diseño"></div>
    <div class="stats" id="stats"></div>
    <div class="seg push" id="layout" role="group" aria-label="Vista">
      <button data-layout="both" aria-pressed="true">Dibujo y texto</button>
      <button data-layout="drawing" aria-pressed="false">Solo dibujo</button>
      <button data-layout="text" aria-pressed="false">Solo texto</button>
    </div>
  </header>
  <nav id="nav" aria-label="Índice"></nav>
  <main id="main">
    <section class="pane drawing">
      <div class="bar"><strong id="title"></strong>
        <span id="tools"><button class="tool" id="zout" aria-label="Alejar">−</button>
        <button class="tool" id="zin" aria-label="Acercar">+</button>
        <button class="tool" id="zfit">ajustar</button>
        <button class="tool" id="z100">100 %</button>
        <span id="zoomv" style="color:var(--muted)"></span></span>
      </div>
      <div class="canvas" id="canvas"></div>
    </section>
    <section class="pane text">
      <div class="bar"><strong id="file"></strong><div class="chips" id="chips"></div></div>
      <div class="code" id="code"></div>
    </section>
  </main>
</div>
"""


def page(workspaces, heading, title, fragment):
    payload = json.dumps(workspaces, ensure_ascii=False).replace("</", "<\\/")
    head = (
        f"<title>{html.escape(title)}</title>\n"
        '<meta name="claude:viewport-fit-sides" content="cover">\n'
        '<link rel="preconnect" href="https://fonts.googleapis.com">\n'
        '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;600'
        '&family=JetBrains+Mono:wght@400;600&display=swap">\n'
        f"<style>{STYLE}</style>\n"
    )
    body = (
        BODY.replace("{heading}", html.escape(heading))
        + f'<script type="application/json" id="atlas-data">{payload}</script>\n'
        + f"<script>{SCRIPT}</script>\n"
    )
    if fragment:
        return head + body
    return (
        '<!doctype html>\n<html lang="es">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
        + head
        + "<style>body { margin: 0 }</style>\n</head>\n<body>\n"
        + body
        + "</body>\n</html>\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", action="append", required=True, metavar="NAME=DIR")
    parser.add_argument("--out", required=True)
    parser.add_argument("--ds", default="ds", help="the ds binary")
    parser.add_argument("--heading", default="Atlas de DriftSpec")
    parser.add_argument("--title", default="Atlas de DriftSpec")
    parser.add_argument("--fragment", action="store_true", help="leave out the document skeleton")
    args = parser.parse_args()
    workspaces = []
    for spec in args.workspace:
        name, _, path = spec.partition("=")
        workspaces.append(workspace_data(args.ds, name, path))
    Path(args.out).write_text(page(workspaces, args.heading, args.title, args.fragment), encoding="utf-8")


if __name__ == "__main__":
    main()
