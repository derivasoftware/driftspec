"""driftspec-live: a design seen live while it is written.

    python -m pyds.live serve <workspace> [--port 4747] [--open]   the viewer in the browser, following the files
    python -m pyds.live focus <identity>                            take the reader to an identity
    python -m pyds.live note <identity> <text> [--tone t]           a bubble on an identity (empty text removes it)
    python -m pyds.live highlight <identity>... [--tone t] [--add]  light identities of any perspective
    python -m pyds.live clear [notes|highlights|all]
    python -m pyds.live selection                                   what the reader has selected
    python -m pyds.live state                                       version, the findings
    python -m pyds.live mcp [--port 4747]                           the same, as MCP tools over stdio

The design stays text: whoever edits it edits the .ds files. The session watches them, loads the
model on every change and pushes the new state to every page
that follows it. Focus, notes, highlights and the reader's selection go both ways, so the one who
writes and the one who reads point at the same identities.

The contract (CONTRACT below) is what a client relies on, whether a browser page or an editor's
panel: the routes it calls, the events it receives and the shape of the state. Notes and highlights
are kept in <workspace>/.driftspec/live.json, beside the design and never in it.
"""

from __future__ import annotations

import json
import os
import queue
import sys
import threading
import time
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import pyds

PORT = 4747

# The fields of the state, in the order a client reads them: the contract says them and state() gives
# exactly these, so adding one is a new version a client can see (DS-123).
STATE = ["contract", "workspace", "title", "version", "boot", "findings", "counts", "history", "selection",
         "error", "notes", "highlights", "rules", "comments", "isolated"]

CONTRACT = {
    "version": 4,  # 4: what changed and the baseline left with ds diff (DS-277); 3: seen notes, comments, isolation
    "state": STATE,
    "routes": {
        "GET /": "the page: the viewer that follows the session",
        "GET /page": "the viewer of the current model, one self-contained page",
        "GET /state": "the state: " + ", ".join(STATE),
        "GET /events": "server-sent events: model (the state, on every change), focus, annotate, isolate, error",
        "GET /selection": "what the reader has selected",
        "GET /contract": "this contract",
        "POST /selection": "{id, label}: the reader selected an identity",
        "POST /focus": "{id, also}: take the reader to an identity",
        "POST /note": "{id, text, tone}: a bubble on an identity; empty text removes it",
        "POST /highlight": "{ids, tone, add}: light identities; replaces the highlight unless add",
        "POST /clear": "{what}: notes, highlights or all",
        "POST /seen": "{ids, seen}: the reader marks notes seen (or not)",
        "POST /comment": "{id, text}: the reader writes a comment on an identity for the agent",
        "POST /unread": "{mark}: the comments the agent has not read; marked read unless mark is false",
        "POST /isolate": "{ids, tell}: show only these boxes; empty shows everything",
    },
    "tones": ["accent", "info", "ok", "bad", "req", "you"],
}


class Live:
    """A live session over one workspace: its model version by version, and what both sides point at."""

    def __init__(self, workspace: str):
        self.workspace = os.path.realpath(workspace)
        base = os.path.basename(self.workspace.rstrip("/"))
        self.title = os.path.basename(os.path.dirname(self.workspace)) if base in ("design", "designs", "") else base
        self.lock = threading.Lock()
        self.version = 0
        self.page = ""
        self.loaded: pyds.Workspace | None = None  # the version shown, loaded once in the engine (DS-173)
        self.history: list[dict] = []
        self.findings: list[dict] = []
        self.counts: dict = {}
        self.selection: dict = {}
        self.notes: list[dict] = []
        self.highlights: list[dict] = []
        self.rules: list[dict] = []
        self.comments: list[dict] = []  # what the reader writes for the agent (DS-147)
        self.isolated: list[str] = []  # what the reader isolates, kept across reloads (DS-139)
        self.listeners: list[queue.Queue] = []
        self.stamp = None
        self.error = ""
        self.boot = str(time.time())  # a page from an older server reloads itself
        self.store = os.path.join(self.workspace, ".driftspec", "live.json")
        self.load()

    # ---- what both sides point at, kept beside the design
    def load(self) -> None:
        try:
            with open(self.store) as fh:
                kept = json.load(fh)
            self.notes, self.highlights = list(kept.get("notes", [])), list(kept.get("highlights", []))
            self.comments = list(kept.get("comments", []))
        except (OSError, ValueError):
            pass

    def save(self) -> None:
        try:
            os.makedirs(os.path.dirname(self.store), exist_ok=True)
            with open(self.store, "w") as fh:
                json.dump({"notes": self.notes, "highlights": self.highlights, "comments": self.comments}, fh, indent=1)
        except OSError:
            pass  # a read-only workspace keeps them for the session only

    def annotations(self, frame: list[str]) -> None:
        self.save()
        self.broadcast("annotate", {"notes": self.notes, "highlights": self.highlights, "comments": self.comments, "frame": frame})

    def focus(self, identity: str, also: list[str] | None = None) -> int:
        self.broadcast("focus", {"id": identity, "also": also or []})
        return len(self.listeners)

    def note(self, identity: str, text: str, tone: str = "accent", frame: bool = True) -> int:
        # a note the agent writes again comes back unseen (DS-102)
        self.notes = [n for n in self.notes if n["id"] != identity] + (
            [{"id": identity, "text": text, "tone": tone, "seen": False, "at": time.strftime("%H:%M:%S")}] if text else [])
        self.annotations([identity] if frame and text else [])
        return len(self.notes)

    def highlight(self, ids: list[str], tone: str = "accent", add: bool = False, frame: bool = True) -> int:
        self.highlights = ([h for h in self.highlights if h["id"] not in ids] if add else []) + [{"id": i, "tone": tone} for i in ids]
        self.annotations(list(ids) if frame else [])
        return len(self.highlights)

    def clear(self, what: str = "all") -> None:
        if what in ("all", "notes"):
            self.notes = []
        if what in ("all", "highlights"):
            self.highlights = []
        self.annotations([])

    def seen(self, ids: list[str], seen: bool = True) -> int:
        for n in self.notes:
            if n["id"] in ids:
                n["seen"] = seen
        self.annotations([])
        return sum(1 for n in self.notes if n.get("seen"))

    def comment(self, identity: str, text: str) -> int:
        self.comments.append({"n": len(self.comments) + 1, "id": identity, "text": text, "at": time.strftime("%H:%M:%S"), "read": False})
        self.annotations([])
        return len(self.comments)

    def unread(self, mark: bool = True) -> list[dict]:
        out = [c for c in self.comments if not c.get("read")]
        if mark and out:
            for c in self.comments:
                c["read"] = True
            self.annotations([])
        return out

    def isolate(self, ids: list[str], tell: bool = True) -> list[str]:
        self.isolated = list(ids)
        if tell:
            self.broadcast("isolate", {"ids": self.isolated})
        return self.isolated

    def select(self, identity: str | None, label: str = "") -> None:
        self.selection = {"id": identity, "label": label, "at": time.strftime("%H:%M:%S")}

    # ---- the model
    def files_stamp(self):
        stamp = []
        for root, dirs, files in os.walk(self.workspace):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for f in files:
                if f.endswith(".ds"):
                    p = os.path.join(root, f)
                    try:
                        st = os.stat(p)
                        stamp.append((p, st.st_mtime_ns, st.st_size))
                    except OSError:
                        pass
        return tuple(sorted(stamp))

    def rules_of(self, model, findings) -> list[dict]:
        """The rules and definitions as written, what each answers to and what breaks it (DS-108).

        Read from the model: its entities, its relations and the breaches the evaluator recorded."""
        rel = lambda path: os.path.relpath(path, self.workspace) if path.startswith("/") else path
        rules = []
        for e in sorted(model.entities.values(), key=lambda x: x.id):
            if e.expected or e.kind not in ("rule", "definition"):
                continue
            lines = [[key, c.strip()] for key in ("for", "where", "holds", "never", "if")
                     for c in e.fields.get(key, "").split("; ") if c.strip()]
            breaches = [{"bindings": [list(p) for p in b.bindings], "witness": [list(p) for p in b.witness],
                         "file": rel(b.location.file), "line": b.location.line} for b in model.breaches.get(e.id, [])]
            problems = [{"code": f.code, "message": f.message[len(e.id) + 1:]} for f in findings
                        if f.message.startswith(e.id + " ") and f.code.startswith(("E-RULE", "W-RULE", "W-DEFINITION")) and f.code != "E-RULE-BROKEN"]
            rules.append({"id": e.id, "kind": e.kind, "name": e.name, "title": e.fields.get("title", ""), "scope": e.scope.rstrip("@"),
                          "lines": lines, "requirements": [r.target for r in model.relations if r.kind == "requires" and r.source == e.id],
                          "broken": breaches, "problems": problems, "file": rel(e.location.file), "line": e.location.line})
        return rules

    def reload(self) -> bool:
        stamp = self.files_stamp()
        if stamp == self.stamp:
            return False
        self.stamp = stamp
        t0 = time.perf_counter()
        try:
            files = pyds.read(self.workspace) if stamp else {}
            loaded = pyds.Workspace(files)  # once: the model, the page and the check all ask it (DS-173)
            model = loaded.model()
            page = loaded.page(self.title, [])
            report = loaded.report()
        except Exception as exc:  # a half-written file must not stop the session
            self.error = str(exc)
            self.broadcast("error", {"message": self.error})
            return True
        with self.lock:
            first = self.version == 0
            self.loaded, self.page, self.error = loaded, page, ""
            self.version += 1
            self.findings = [{"code": f.code, "severity": f.severity, "message": f.message,
                              "file": os.path.relpath(f.location.file, self.workspace) if f.location.file.startswith("/") else f.location.file,
                              "line": f.location.line} for f in report.findings]
            self.counts = dict(report.counts)
            self.rules = self.rules_of(model, report.findings)
            if not first:
                self.history.append({"version": self.version, "at": time.strftime("%H:%M:%S"), "ms": round((time.perf_counter() - t0) * 1000)})
                self.history = self.history[-30:]
        self.broadcast("model", self.state())
        return True

    def state(self) -> dict:
        return {"contract": CONTRACT["version"], "workspace": self.workspace, "title": self.title, "version": self.version,
                "boot": self.boot, "findings": self.findings,
                "counts": self.counts, "history": self.history, "selection": self.selection, "error": self.error,
                "notes": self.notes, "highlights": self.highlights, "rules": self.rules, "comments": self.comments,
                "isolated": self.isolated}

    def broadcast(self, event: str, data: dict) -> None:
        for q in list(self.listeners):
            q.put((event, data))

    def watch(self, every: float = 0.35) -> None:
        while not getattr(self, "stopped", False):
            try:
                self.reload()
            except Exception as exc:  # keep watching whatever happens
                self.error = str(exc)
            time.sleep(every)


# ---------------------------------------------------------------- the page around the viewer
SHELL = r"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>driftspec live · __TITLE__</title>
<style>
:root{--bg:#f4f6f8;--panel:#fff;--fg:#1b2129;--muted:#5a6573;--line:#d5dbe2;--accent:#d9822b;--ok:#2f8a4a;--err:#c2410c;--sel:#2f62c9;
 font-family:system-ui,"Segoe UI",sans-serif;font-size:13px;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#12161b;--panel:#1a2027;--fg:#e4e9ef;--muted:#9aa6b4;--line:#2f3943;--accent:#f0a35a;--ok:#6cc68a;--err:#f08a5d;--sel:#7aa2f2;color-scheme:dark}}
*{box-sizing:border-box} html,body{height:100%;margin:0;background:var(--bg);color:var(--fg)}
body{display:grid;grid-template-rows:auto 1fr;grid-template-columns:1fr 300px;height:100%}
header{grid-column:1/-1;display:flex;align-items:center;gap:14px;padding:8px 14px;background:var(--panel);border-bottom:1px solid var(--line);flex-wrap:wrap}
header b{font-size:15px} .dot{width:9px;height:9px;border-radius:50%;background:var(--muted);display:inline-block} .dot.on{background:var(--ok)} .dot.err{background:var(--err)}
.pill{font-family:ui-monospace,Consolas,monospace;font-size:12px;border:1px solid var(--line);border-radius:999px;padding:2px 9px}
.pill.err{border-color:var(--err);color:var(--err)} .pill.sel{border-color:var(--sel);color:var(--sel)}
#views{position:relative;min-width:0} #views iframe{position:absolute;inset:0;width:100%;height:100%;border:0;background:var(--bg)}
aside{border-left:1px solid var(--line);background:var(--panel);overflow:auto;padding:10px 12px;display:flex;flex-direction:column;gap:12px}
h3{margin:0;font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}
ul{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:3px}
li button{all:unset;cursor:pointer;font-family:ui-monospace,Consolas,monospace;font-size:11.5px;word-break:break-all} li button:hover{text-decoration:underline}
.v{font-size:11px;color:var(--muted)} .ch{color:var(--accent)} .rm{color:var(--err);text-decoration:line-through}
.f{font-size:12px} .f code{font-family:ui-monospace,Consolas,monospace;font-size:11px;color:var(--err)}
.flash{animation:fl 1.2s ease-out} @keyframes fl{from{background:color-mix(in srgb,var(--accent) 30%,var(--panel))}to{background:var(--panel)}}
body{grid-template-columns:1fr 380px}
.rules{gap:8px} .rule{border:1px solid var(--line);border-radius:8px;padding:7px 9px;display:flex;flex-direction:column;gap:4px;cursor:pointer}
.rule:hover{border-color:var(--muted)} .rule.on{border-color:var(--sel);box-shadow:0 0 0 1px var(--sel)} .rule.dim{opacity:.45}
.rule .hd{display:flex;align-items:center;gap:6px;justify-content:space-between} .rule .nm{font-family:ui-monospace,Consolas,monospace;font-weight:600;font-size:12px}
.st{font-size:11px;border-radius:999px;padding:1px 8px;white-space:nowrap} .st.ok{color:var(--ok);border:1px solid var(--ok)} .st.bad{color:var(--err);border:1px solid var(--err)}
.st.def{color:var(--muted);border:1px solid var(--line)} .st.warn{color:var(--accent);border:1px solid var(--accent)}
.fx{font-family:ui-monospace,Consolas,monospace;font-size:11.5px;line-height:1.45;white-space:pre-wrap;margin:0}
.fx .k{color:var(--sel);font-weight:600} .fx .k.never{color:var(--err)} .fx .k.holds{color:var(--ok)}
.br{font-family:ui-monospace,Consolas,monospace;font-size:11px;color:var(--err);border-left:2px solid var(--err);padding-left:6px}
.rq{font-family:ui-monospace,Consolas,monospace;font-size:10.5px;color:var(--muted)}
.tabs{display:flex;gap:4px} .tabs button{all:unset;cursor:pointer;font-size:11px;padding:2px 8px;border-radius:999px;border:1px solid var(--line)} .tabs button.on{border-color:var(--sel);color:var(--sel)}
details{border-top:1px solid var(--line);padding-top:8px} details>summary{cursor:pointer;list-style:none;display:flex;align-items:center;gap:6px}
details>summary::-webkit-details-marker{display:none} details>summary::before{content:'▸';color:var(--muted);font-size:10px} details[open]>summary::before{content:'▾'}
details>summary h3{flex:1} .n{font-size:11px;color:var(--muted)} .acts{display:flex;gap:4px;flex-wrap:wrap;margin:6px 0}
.acts button,.nt .b{all:unset;cursor:pointer;font-size:11px;padding:1px 8px;border-radius:999px;border:1px solid var(--line)} .acts button:hover,.nt .b:hover{border-color:var(--muted)}
.nt{border:1px solid var(--line);border-radius:8px;padding:6px 8px;display:flex;flex-direction:column;gap:4px} .nt.seen{opacity:.5} .nt.you{border-color:var(--sel)}
.nt .hd{display:flex;gap:6px;align-items:center} .nt .hd button.id{flex:1;text-align:left} .nt .b.on{border-color:var(--ok);color:var(--ok)}
.thread{display:flex;flex-direction:column;gap:6px} textarea{width:100%;min-height:58px;resize:vertical;font:inherit;border:1px solid var(--line);border-radius:8px;padding:6px 8px;background:var(--bg);color:var(--fg)}
.send{display:flex;justify-content:space-between;align-items:center;margin-top:4px} .send button{all:unset;cursor:pointer;font-size:12px;padding:3px 12px;border-radius:999px;background:var(--sel);color:#fff}
.pill.iso{cursor:pointer;border-color:var(--accent);color:var(--accent)}
.add{color:var(--ok)} .chg{color:var(--accent)}
@media (max-width:800px){body{grid-template-columns:1fr;grid-template-rows:auto 1fr 40%} aside{border-left:0;border-top:1px solid var(--line)}}
</style></head><body>
<header><span class="dot" id="dot"></span><b>__TITLE__</b><span class="pill" id="ver">v0</span><span class="pill" id="counts"></span>
<span class="pill" id="find"></span>
<span class="pill iso" id="isop" hidden title="click to show everything again"></span><span class="pill sel" id="sel" hidden></span><span class="v" id="ws">__WS__</span></header>
<div id="views"></div>
<aside id="side">
<details open data-k="sel"><summary><h3>Selected</h3><span class="n" id="seln">nothing</span></summary><div class="thread" id="thread"><div class="v">select a box, a member or a note in the viewer</div></div>
<div id="cbox" hidden><textarea id="ctext" placeholder="a comment for the agent on what is selected (Ctrl+Enter sends)"></textarea><div class="send"><span class="v" id="cto"></span><button id="csend">send</button></div></div></details>
<details open data-k="notes"><summary><h3>Notes</h3><span class="n" id="notesn"></span></summary><div class="acts"><button id="nfold">fold all</button><button id="nopen">open all</button><button id="nseen">all seen</button><button id="nhide">hide seen</button></div><ul id="notes" style="gap:6px"><li class="v">none</li></ul></details>

<div id="rulesbox" hidden><div class="hd" style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px"><h3>Rules</h3>
<div class="tabs"><button data-f="all" class="on">all</button><button data-f="bad">broken</button><button data-f="sel">selected</button></div></div>
<ul class="rules" id="rules"><li class="v">none</li></ul></div>
<details data-k="find"><summary><h3>Findings</h3><span class="n" id="findn"></span></summary><ul id="findings"></ul></details><details data-k="hist"><summary><h3>History</h3></summary><ul id="hist"></ul></details></aside>
<script>
const views=document.getElementById('views'); let cur=null, version=0, keepSel=null, pending=null, busy=false, ann={notes:[],highlights:[]};
const $=id=>document.getElementById(id); const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function view(){return cur&&cur.contentWindow&&cur.contentWindow.DS_VIEW;}
async function swap(state){ // a new viewer loads hidden; it takes the reader's camera and selection, then replaces the old one
  if(busy){pending=state; return;} busy=true;
  const html=await (await fetch('/page?v='+state.version)).text();
  const old=view(); const cam=old?old.cam:null; const sel=old?old.selected:keepSel;
  const f=document.createElement('iframe'); f.style.visibility='hidden'; f.srcdoc=html; views.appendChild(f);
  await new Promise(res=>{const on=e=>{if(e.source===f.contentWindow&&e.data&&e.data.ds==='ready'){removeEventListener('message',on); res();}}; addEventListener('message',on); setTimeout(res,4000);});
  const v=f.contentWindow.DS_VIEW;
  if(v){ if(cam) v.setCam(cam); if(sel&&v.has(sel)) v.select(sel); v.highlight(ann.highlights); v.notes(drawnNotes()); }
  if(v){v.perspective_(PERSP); if(ACTIVE) v.lightRule(ACTIVE); if(v.isolate&&ISOLATED.length) v.isolate(ISOLATED); paint();}
  f.style.visibility='visible'; if(cur) cur.remove(); cur=f; busy=false;
  if(pending){const p=pending; pending=null; swap(p);} }
let RULES=[], ACTIVE=null, FILTER='all', SELID=null, PERSP='structure';
function perspective(p){PERSP=p; $('rulesbox').hidden=p!=='logic'; if(p!=='logic'&&ACTIVE){const v=view(); if(v&&v.rule) v.lightRule(v.rule); ACTIVE=null; rules();}}
function paint(){const v=view(); if(!v) return; v.highlight(ann.highlights); v.notes(drawnNotes());}
// ---- the reader's side (DS-102, DS-139, DS-147): folded and seen notes, comments, isolation
const store={get(k,d){try{return JSON.parse(localStorage.getItem('dslive.'+k))??d;}catch(e){return d;}},set(k,v){try{localStorage.setItem('dslive.'+k,JSON.stringify(v));}catch(e){}}};
let FOLD=new Set(store.get('fold',[])), OPEN=new Set(store.get('open',[])), HIDESEEN=store.get('hideseen',false), COMMENTS=[], ISOLATED=[];
const keyOf=n=>n.n?'c'+n.n:n.id+'@'+(n.at||'');
const foldedOf=n=>FOLD.has(keyOf(n))||(!OPEN.has(keyOf(n))&&(n.seen||!!n.n));
function keep(){store.set('fold',[...FOLD]); store.set('open',[...OPEN]); store.set('hideseen',HIDESEEN);}
function drawnNotes(){const mine=ann.notes.filter(n=>!(HIDESEEN&&n.seen)).map(n=>({id:n.id,text:n.text,tone:n.tone,seen:!!n.seen,folded:foldedOf(n)}));
  const yours=COMMENTS.map(c=>({id:c.id,text:'you: '+c.text,tone:'you',seen:false,folded:foldedOf(c)})); return mine.concat(yours);}
function fold(ids,act){ann.notes.concat(COMMENTS).filter(n=>ids.includes(n.id)).forEach(n=>{const k=keyOf(n); if(act==='fold'){FOLD.add(k); OPEN.delete(k);} else {FOLD.delete(k); OPEN.add(k);}}); keep(); paint(); notesList();}
function seen(ids,on){fetch('/seen',{method:'POST',body:JSON.stringify({ids,seen:on})});}
function notesList(){const list=ann.notes.filter(n=>!(HIDESEEN&&n.seen)); const unseen=ann.notes.filter(n=>!n.seen).length;
  $('notesn').textContent=ann.notes.length?`${unseen} unseen of ${ann.notes.length}`:'';
  $('notes').innerHTML=list.map(n=>`<li class="nt${n.seen?' seen':''}"><div class="hd"><button class="id" data-id="${esc(n.id)}">💬 ${esc(short(n.id))}</button>`+
    `<button class="b" data-fold="${esc(n.id)}">${foldedOf(n)?'open':'fold'}</button><button class="b${n.seen?' on':''}" data-seen="${esc(n.id)}">${n.seen?'✓ seen':'seen'}</button></div><div class="f">${esc(n.text)}</div></li>`).join('')||'<li class="v">none</li>';
  $('nhide').textContent=HIDESEEN?'show seen':'hide seen';}
function thread(){const id=SELID; $('seln').textContent=id?short(id):'nothing'; $('cbox').hidden=!id; $('cto').textContent=id?'on '+short(id):'';
  if(!id){$('thread').innerHTML='<div class="v">select a box, a member or a note in the viewer</div>'; return;}
  const on=x=>x===id||String(x).startsWith(id+'::')||String(x).startsWith(id+'@');
  const items=ann.notes.filter(n=>on(n.id)).map(n=>({at:n.at||'',who:'agent',text:n.text,id:n.id,seen:n.seen})).concat(COMMENTS.filter(c=>on(c.id)).map(c=>({at:c.at,who:'you',text:c.text,id:c.id,read:c.read})))
    .sort((a,b)=>a.at<b.at?-1:1);
  $('thread').innerHTML=items.map(i=>`<div class="nt${i.who==='you'?' you':''}${i.seen?' seen':''}"><div class="v">${i.who==='you'?'you':'agent'} · ${esc(i.at)}${i.id!==id?' · '+esc(short(i.id)):''}${i.who==='you'?(i.read?' · read':' · not read yet'):''}</div><div class="f">${esc(i.text)}</div></div>`).join('')||'<div class="v">no notes or comments here yet</div>';}
async function send(){const t=$('ctext').value.trim(); if(!t||!SELID) return; $('ctext').value=''; await fetch('/comment',{method:'POST',body:JSON.stringify({id:SELID,text:t})});}
$('csend').onclick=send; $('ctext').addEventListener('keydown',e=>{if(e.key==='Enter'&&(e.ctrlKey||e.metaKey)){e.preventDefault(); send();}});
$('nfold').onclick=()=>fold(ann.notes.map(n=>n.id),'fold'); $('nopen').onclick=()=>fold(ann.notes.map(n=>n.id),'open');
$('nseen').onclick=()=>seen(ann.notes.filter(n=>!n.seen).map(n=>n.id),true); $('nhide').onclick=()=>{HIDESEEN=!HIDESEEN; keep(); paint(); notesList();};
$('notes').addEventListener('click',e=>{const f=e.target.closest('[data-fold]'); if(f){const n=ann.notes.find(x=>x.id===f.dataset.fold); fold([n.id],foldedOf(n)?'open':'fold'); e.stopPropagation(); return;}
  const s=e.target.closest('[data-seen]'); if(s){const n=ann.notes.find(x=>x.id===s.dataset.seen); seen([n.id],!n.seen); e.stopPropagation();}},true);
function isoPill(){$('isop').hidden=!ISOLATED.length; $('isop').textContent=`isolated: ${ISOLATED.map(short).join(', ')} ×`;}
$('isop').onclick=()=>{ISOLATED=[]; const v=view(); if(v&&v.isolate) v.isolate([]); fetch('/isolate',{method:'POST',body:JSON.stringify({ids:[],tell:false})}); isoPill();};
document.querySelectorAll('details[data-k]').forEach(d=>{const o=store.get('open.'+d.dataset.k,null); if(o!==null) d.open=o; d.addEventListener('toggle',()=>store.set('open.'+d.dataset.k,d.open));});
function activate(id){const v=view(); ACTIVE=v&&v.lightRule?v.lightRule(id):null; rules();}
const short=s=>String(s).replace(/^.*?::sequence::/,'call ').split(/[@:]+/).slice(-2).join('::');
function applies(r,id){if(!id) return false; const under=x=>x===id||String(x).startsWith(id+'::')||String(x).startsWith(id+'@'); return under(r.scope)||r.broken.some(b=>b.bindings.some(p=>under(p[1])));}
function rules(){const list=RULES.filter(r=>FILTER==='all'||(FILTER==='bad'&&(r.broken.length||r.problems.length))||(FILTER==='sel'&&applies(r,SELID)));
  $('rules').innerHTML=list.map(r=>{const n=r.broken.length;
    const st=r.kind==='definition'?'<span class="st def">definition</span>':r.problems.length?`<span class="st warn">${esc(r.problems[0].code)}</span>`:n?`<span class="st bad">✗ broken ${n}</span>`:'<span class="st ok">✓ holds</span>';
    const fx=r.lines.map(l=>`<span class="k ${l[0]}">${esc(l[0])}</span> ${esc(l[1])}`).join('\n');
    const br=r.broken.slice(0,6).map(b=>`<div class="br">${esc(b.bindings.concat(b.witness).map(p=>p[0]+' = '+short(p[1])).join(', '))} · ${esc(b.file.split('/').pop())}:${b.line}</div>`).join('');
    const pr=r.problems.map(p=>`<div class="br">${esc(p.code)} ${esc(p.message)}</div>`).join('');
    return `<li class="rule${ACTIVE===r.id?' on':''}${ACTIVE&&ACTIVE!==r.id?' dim':''}" data-rule="${esc(r.id)}"><div class="hd"><span class="nm">${esc(r.name)}</span>${st}</div>`+
      (r.title?`<div class="v">${esc(r.title)}</div>`:'')+`<pre class="fx">${fx}</pre>`+br+pr+
      `<div class="rq">${esc(short(r.scope||''))}${r.requirements.length?' · '+esc(r.requirements.join(', ')):''} · ${esc(r.file.split('/').pop())}:${r.line}</div></li>`;}).join('')||
    `<li class="v">${FILTER==='sel'?(SELID?'no rule speaks of it':'select something in the viewer'):'none'}</li>`;}
document.querySelector('.tabs').onclick=e=>{const b=e.target.closest('[data-f]'); if(!b) return; FILTER=b.dataset.f;
  document.querySelectorAll('.tabs button').forEach(x=>x.classList.toggle('on',x===b)); rules();};
function applyAnn(d){ann={notes:d.notes||[],highlights:d.highlights||[]}; COMMENTS=d.comments||COMMENTS; paint(); notesList(); thread();}
function side(s){
  $('ver').textContent='v'+s.version; const c=s.counts||{}; $('counts').textContent=`${c.entities||0} entities · ${c.relations||0} relations`;
  const n=(s.findings||[]).length; $('find').textContent=n?`${n} findings`:'no findings'; $('find').className='pill'+(n?' err':''); $('findn').textContent=n?String(n):'';
  ISOLATED=s.isolated||ISOLATED; isoPill();
  $('findings').innerHTML=(s.findings||[]).slice(0,60).map(f=>`<li class="f"><code>${esc(f.code)}</code> ${esc(f.file)}:${f.line}<br>${esc(f.message)}</li>`).join('')||'<li class="v">none</li>';
  RULES=s.rules||[]; if(ACTIVE&&!RULES.find(r=>r.id===ACTIVE)) ACTIVE=null; rules();
  $('hist').innerHTML=(s.history||[]).slice().reverse().map(h=>`<li class="v">v${h.version} · ${h.at} · ${h.ms} ms</li>`).join('');
  $('side').classList.remove('flash'); void $('side').offsetWidth; $('side').classList.add('flash');}
$('side').addEventListener('click',e=>{const r=e.target.closest('[data-rule]'); if(r) activate(r.dataset.rule);});
$('side').onclick=e=>{const b=e.target.closest('[data-id]'); const v=view(); if(b&&v){const k=v.show(b.dataset.id); if(k!=='node') v.frameAny([b.dataset.id]);}};
addEventListener('message',e=>{if(e.data&&e.data.ds==='perspective'&&cur&&e.source===cur.contentWindow) perspective(e.data.p);});
// the viewer's fullscreen button: the whole page, so the side panel stays (DS-141)
addEventListener('message',e=>{if(e.data&&e.data.ds==='fullscreen'&&cur&&e.source===cur.contentWindow){
  if(document.fullscreenElement) document.exitFullscreen(); else if(document.documentElement.requestFullscreen) document.documentElement.requestFullscreen().catch(()=>{});}});
addEventListener('message',e=>{if(!e.data||!cur||e.source!==cur.contentWindow) return;
  if(e.data.ds==='note'){if(e.data.act==='seen'){const n=ann.notes.find(x=>x.id===e.data.ids[0]); if(n) seen([n.id],!n.seen);} else fold(e.data.ids,e.data.act);}
  if(e.data.ds==='isolate'){ISOLATED=e.data.ids||[]; isoPill(); fetch('/isolate',{method:'POST',body:JSON.stringify({ids:ISOLATED,tell:false})});}});
addEventListener('message',e=>{if(e.data&&e.data.ds==='select'&&cur&&e.source===cur.contentWindow){keepSel=e.data.id; SELID=e.data.id; if(FILTER==='sel') rules(); thread();
  $('sel').hidden=!e.data.id; $('sel').textContent=e.data.id?'◉ '+e.data.id:'';
  fetch('/selection',{method:'POST',body:JSON.stringify({id:e.data.id,label:e.data.label})});}});
function connect(){const es=new EventSource('/events'); es.onopen=()=>$('dot').className='dot on';
  es.addEventListener('model',e=>{const s=JSON.parse(e.data); if(window.BOOT&&s.boot!==window.BOOT){location.reload(); return;} window.BOOT=s.boot; side(s); applyAnn(s); if(s.version!==version){version=s.version; swap(s);}});
  es.addEventListener('focus',e=>{const d=JSON.parse(e.data); const v=view(); if(v){const k=v.show(d.id); if(k!=='node') v.frameAny([d.id].concat(d.also||[]));}});
  es.addEventListener('annotate',e=>{const d=JSON.parse(e.data); applyAnn(d); const v=view(); if(v&&d.frame&&d.frame.length) v.frameAny(d.frame);});
  es.addEventListener('isolate',e=>{const d=JSON.parse(e.data); ISOLATED=d.ids||[]; isoPill(); const v=view(); if(v&&v.isolate) v.isolate(ISOLATED);});
  es.addEventListener('error',e=>{if(e.data){$('dot').className='dot err'; $('findings').innerHTML=`<li class="f"><code>error</code> ${esc(JSON.parse(e.data).message)}</li>`;}});
  es.onerror=()=>{$('dot').className='dot'; };}
connect();
</script></body></html>"""


def handler_for(live: Live):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *a):  # quiet
            pass

        def send(self, code: int, body: bytes, ctype: str) -> None:
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def json(self, obj, code: int = 200) -> None:
            self.send(code, json.dumps(obj).encode(), "application/json")

        def do_GET(self):
            path = self.path.split("?")[0]
            if path == "/":
                self.send(200, SHELL.replace("__TITLE__", live.title).replace("__WS__", live.workspace).encode(), "text/html; charset=utf-8")
            elif path == "/page":
                with live.lock:
                    page = live.page
                self.send(200, page.encode(), "text/html; charset=utf-8")
            elif path == "/state":
                self.json(live.state())
            elif path == "/selection":
                self.json(live.selection)
            elif path == "/contract":
                self.json(CONTRACT)
            elif path == "/events":
                self.events()
            else:
                self.send(404, b"not found", "text/plain")

        def events(self) -> None:
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            q: queue.Queue = queue.Queue()
            live.listeners.append(q)
            q.put(("model", live.state()))
            try:
                while not getattr(live, "stopped", False):
                    try:
                        event, data = q.get(timeout=15)
                        self.wfile.write(f"event: {event}\ndata: {json.dumps(data)}\n\n".encode())
                    except queue.Empty:
                        self.wfile.write(b": keep-alive\n\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                if q in live.listeners:
                    live.listeners.remove(q)

        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
            try:
                data = json.loads(body or b"{}")
            except ValueError:
                data = {}
            if self.path == "/selection":
                live.select(data.get("id"), data.get("label", ""))
                self.json({})
            elif self.path == "/focus":
                self.json({"sent": data.get("id", ""), "listeners": live.focus(data.get("id", ""), data.get("also", []))})
            elif self.path == "/note":
                self.json({"notes": live.note(data.get("id", ""), data.get("text", ""), data.get("tone", "accent"), data.get("frame", True))})
            elif self.path == "/highlight":
                self.json({"highlights": live.highlight(data.get("ids", []), data.get("tone", "accent"), data.get("add", False), data.get("frame", True))})
            elif self.path == "/seen":
                self.json({"seen": live.seen(data.get("ids", []), data.get("seen", True))})
            elif self.path == "/comment":
                self.json({"comments": live.comment(data.get("id", ""), data.get("text", ""))})
            elif self.path == "/unread":
                self.json({"comments": live.unread(data.get("mark", True))})
            elif self.path == "/isolate":
                self.json({"isolated": live.isolate(data.get("ids", []), data.get("tell", True))})
            elif self.path == "/clear":
                live.clear(data.get("what", "all"))
                self.json({})
            else:
                self.json({}, 404)

    return Handler


def browser_open(url: str) -> None:
    import shutil
    import subprocess

    if shutil.which("explorer.exe"):  # WSL: the Windows browser reaches localhost
        subprocess.Popen(["explorer.exe", url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        webbrowser.open(url)


def serve(workspace: str, port: int = PORT, open_browser: bool = False, block: bool = True) -> Live:
    """A session on the workspace, served on the port (0: any free one); the session knows its url and how to stop."""
    live = Live(workspace)
    live.reload()
    server = ThreadingHTTPServer(("127.0.0.1", port), handler_for(live))
    server.daemon_threads = True
    live.port = server.server_address[1]
    live.url = f"http://localhost:{live.port}/"

    def stop() -> None:
        live.stopped = True
        server.shutdown()
        server.server_close()

    live.stop = stop
    threading.Thread(target=live.watch, daemon=True).start()
    print(f"driftspec live: {live.workspace} at {live.url}", file=sys.stderr, flush=True)
    if open_browser:
        browser_open(live.url)
    if block:
        server.serve_forever()
    else:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    return live


# ---------------------------------------------------------------- the client side: the one who writes
def call(port: int, path: str, data: dict | None = None) -> dict:
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=None if data is None else json.dumps(data).encode(),
                                 method="GET" if data is None else "POST", headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read() or b"{}")


# ---------------------------------------------------------------- MCP over stdio: the same contract, for an agent
TOOLS = [
    {"name": "live_open", "description": "Open a DriftSpec workspace in the live viewer (starts the server if needed) and open the browser.",
     "inputSchema": {"type": "object", "properties": {"workspace": {"type": "string"}}, "required": ["workspace"]}},
    {"name": "live_focus", "description": "Take the reader to an identity (a box, class, member, interface, requirement or sequence) in the live viewer: it is selected and framed.",
     "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "also": {"type": "array", "items": {"type": "string"}}}, "required": ["id"]}},
    {"name": "live_note", "description": "Put a speech bubble on an identity in the live viewer; the same identity replaces its bubble, empty text removes it. tone: accent, info, ok, bad, req.",
     "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "text": {"type": "string"}, "tone": {"type": "string"}}, "required": ["id", "text"]}},
    {"name": "live_highlight", "description": "Highlight identities of any perspective in the live viewer with a tone; replaces the previous highlight unless add is true.",
     "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}, "tone": {"type": "string"}, "add": {"type": "boolean"}}, "required": ["ids"]}},
    {"name": "live_clear", "description": "Clear the notes, the highlights or both (what: notes, highlights, all) in the live viewer.",
     "inputSchema": {"type": "object", "properties": {"what": {"type": "string"}}}},
    {"name": "live_comments", "description": "What the reader wrote for the agent in the live viewer since the last call: each comment with the identity it is on; they are marked read unless peek is true. Answer with live_note on the same identity.",
     "inputSchema": {"type": "object", "properties": {"peek": {"type": "boolean"}}}},
    {"name": "live_isolate", "description": "Show only these boxes in the live viewer, with what they use from outside as ghosts; an empty list shows everything.",
     "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}}, "required": ["ids"]}},
    {"name": "live_selection", "description": "What the reader has selected in the live viewer.", "inputSchema": {"type": "object", "properties": {}}},
    {"name": "live_state", "description": "The live session: its version and the findings.", "inputSchema": {"type": "object", "properties": {}}},
]


def tool(port: int, name: str, args: dict, opened: dict) -> object:
    """One MCP tool call, answered through the session's routes."""
    if name == "live_open":
        workspace = os.path.realpath(args["workspace"])
        try:
            st = call(port, "/state")
            if st.get("workspace") != workspace:
                return f"error: port {port} serves {st.get('workspace')}"
            browser_open(f"http://localhost:{port}/")
        except OSError:
            opened["live"] = serve(workspace, port, open_browser=True, block=False)
        return f"live viewer at http://localhost:{port}/ for {workspace}"
    if name == "live_focus":
        return call(port, "/focus", {"id": args["id"], "also": args.get("also", [])})
    if name == "live_note":
        return call(port, "/note", {"id": args["id"], "text": args["text"], "tone": args.get("tone", "accent")})
    if name == "live_highlight":
        return call(port, "/highlight", {"ids": args["ids"], "tone": args.get("tone", "accent"), "add": args.get("add", False)})
    if name == "live_clear":
        return call(port, "/clear", {"what": args.get("what", "all")})
    if name == "live_comments":
        return call(port, "/unread", {"mark": not args.get("peek", False)})
    if name == "live_isolate":
        return call(port, "/isolate", {"ids": args.get("ids", [])})
    if name == "live_selection":
        return call(port, "/selection")
    if name == "live_state":
        st = call(port, "/state")
        st["findings"] = st["findings"][:30]
        return st
    raise KeyError(name)


def mcp(port: int, stdin=None, stdout=None) -> None:
    stdin, stdout = stdin or sys.stdin, stdout or sys.stdout
    opened: dict = {}

    def reply(i, result=None, error=None):
        msg = {"jsonrpc": "2.0", "id": i}
        msg["result" if error is None else "error"] = result if error is None else {"code": -32000, "message": error}
        stdout.write(json.dumps(msg) + "\n")
        stdout.flush()

    for line in stdin:
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        method, i = msg.get("method"), msg.get("id")
        if method == "initialize":
            reply(i, {"protocolVersion": msg.get("params", {}).get("protocolVersion", "2024-11-05"), "capabilities": {"tools": {}},
                      "serverInfo": {"name": "driftspec-live", "version": str(CONTRACT["version"])}})
        elif method == "tools/list":
            reply(i, {"tools": TOOLS})
        elif method == "tools/call":
            name, args = msg["params"]["name"], msg["params"].get("arguments", {})
            try:
                out = tool(port, name, args, opened)
                reply(i, {"content": [{"type": "text", "text": out if isinstance(out, str) else json.dumps(out, indent=1)}]})
            except KeyError:
                reply(i, error=f"no tool {name}")
            except Exception as exc:
                reply(i, {"content": [{"type": "text", "text": f"error: {exc}"}], "isError": True})
        elif i is not None:
            reply(i, {})


def main(argv: list[str] | None = None) -> None:
    args = list(sys.argv[1:] if argv is None else argv)
    port = PORT
    if "--port" in args:
        k = args.index("--port")
        port = int(args[k + 1])
        del args[k:k + 2]
    tone = "accent"
    if "--tone" in args:
        k = args.index("--tone")
        tone = args[k + 1]
        del args[k:k + 2]
    add = "--add" in args
    args = [a for a in args if a != "--add"]
    cmd = args[0] if args else "serve"
    if cmd == "serve":
        serve(args[1] if len(args) > 1 else ".", port, open_browser="--open" in args)
    elif cmd == "focus":
        print(json.dumps(call(port, "/focus", {"id": args[1], "also": args[2:]})))
    elif cmd == "note":
        print(json.dumps(call(port, "/note", {"id": args[1], "text": " ".join(args[2:]), "tone": tone})))
    elif cmd == "highlight":
        print(json.dumps(call(port, "/highlight", {"ids": args[1:], "tone": tone, "add": add})))
    elif cmd == "clear":
        print(json.dumps(call(port, "/clear", {"what": args[1] if len(args) > 1 else "all"})))
    elif cmd == "comments":
        print(json.dumps(call(port, "/unread", {"mark": "--peek" not in args}), indent=1))
    elif cmd == "isolate":
        print(json.dumps(call(port, "/isolate", {"ids": args[1:]})))
    elif cmd == "selection":
        print(json.dumps(call(port, "/selection")))
    elif cmd == "state":
        st = call(port, "/state")
        print(json.dumps({k: st[k] for k in ("version", "counts", "error")} | {"findings": st["findings"][:20]}, indent=1))
    elif cmd == "mcp":
        mcp(port)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
