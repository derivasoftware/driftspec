"""The page around the viewer: the shell a browser opens, which follows the session and swaps in each
new version of the viewer without losing the reader's camera, selection, notes or isolation."""

from __future__ import annotations

import html
import json

import driftspec

SHELL = r"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>driftspec live · __TITLE__</title>
<style>
:root{--bg:#f4f6f8;--panel:#fff;--fg:#1b2129;--muted:#5a6573;--line:#d5dbe2;--accent:#d9822b;--ok:#2f8a4a;--err:#c2410c;--warn:#a16207;--info:#0e7490;--sel:#2f62c9;
 font-family:system-ui,"Segoe UI",sans-serif;font-size:13px;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#12161b;--panel:#1a2027;--fg:#e4e9ef;--muted:#9aa6b4;--line:#2f3943;--accent:#f0a35a;--ok:#6cc68a;--err:#f08a5d;--warn:#e3b341;--info:#5cc8d9;--sel:#7aa2f2;color-scheme:dark}}
*{box-sizing:border-box} html,body{height:100%;margin:0;background:var(--bg);color:var(--fg)}
body{display:grid;grid-template-rows:auto 1fr;grid-template-columns:1fr 300px;height:100%}
header{grid-column:1/-1;display:flex;align-items:center;gap:14px;padding:8px 14px;background:var(--panel);border-bottom:1px solid var(--line);flex-wrap:wrap}
header b{font-size:15px} .dot{width:9px;height:9px;border-radius:50%;background:var(--muted);display:inline-block} .dot.on{background:var(--ok)} .dot.err{background:var(--err)}
.pill{font-family:ui-monospace,Consolas,monospace;font-size:12px;border:1px solid var(--line);border-radius:999px;padding:2px 9px}
.pill.err{border-color:var(--err);color:var(--err)} .pill.warn{border-color:var(--warn);color:var(--warn)} .pill.sel{border-color:var(--sel);color:var(--sel)}
#views{position:relative;min-width:0} #views iframe{position:absolute;inset:0;width:100%;height:100%;border:0;background:var(--bg)}
aside{border-left:1px solid var(--line);background:var(--panel);overflow:auto;padding:10px 12px;display:flex;flex-direction:column;gap:12px}
h3{margin:0;font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}
ul{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:3px}
li button{all:unset;cursor:pointer;font-family:ui-monospace,Consolas,monospace;font-size:11.5px;word-break:break-all} li button:hover{text-decoration:underline}
.v{font-size:11px;color:var(--muted)} .ch{color:var(--accent)} .rm{color:var(--err);text-decoration:line-through}
.f{font-size:12px} .f code{font-family:ui-monospace,Consolas,monospace;font-size:11px;color:var(--err)} .f code.warn{color:var(--warn)} .f code.info{color:var(--info)}
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
<details data-k="find"><summary><h3>Findings</h3><span class="n" id="findn"></span></summary><ul id="findings"></ul></details><details data-k="cov"><summary><h3>Coverage</h3><span class="n" id="covn"></span></summary><ul id="coverage"></ul></details><details data-k="hist"><summary><h3>History</h3></summary><ul id="hist"></ul></details></aside>
<script>
const SEVERITIES=__SEVERITIES__;  // the severity table, as the engine writes it (DS-386): a finding is coloured by its row
const sevOf=name=>SEVERITIES.find(s=>s.name===name)||{}; const tone=name=>{const s=sevOf(name); return s.fails?'':(s.keptApart?'info':'warn');};
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
  const fs=s.findings||[], n=fs.length, failing=fs.filter(f=>sevOf(f.severity).fails).length;
  const rest=SEVERITIES.filter(x=>!x.fails).map(x=>[x.name,fs.filter(f=>f.severity===x.name).length]).filter(([,k])=>k).map(([nm,k])=>`${k} ${nm}${k===1?'':'s'}`).join(', ');
  $('find').textContent=n?`${failing} failing`+(rest?` · ${rest}`:''):'no findings'; $('find').className='pill'+(failing?' err':(n?' warn':'')); $('findn').textContent=n?String(n):'';
  const cov=s.coverage||[]; $('covn').textContent=cov.length?String(cov.length):'';
  $('coverage').innerHTML=cov.slice(0,60).map(f=>`<li class="f"><code class="${tone(f.severity)}">${esc(f.code)}</code> ${esc(f.file)}:${f.line}<br>${esc(f.message)}</li>`).join('')||'<li class="v">none</li>';
  ISOLATED=s.isolated||ISOLATED; isoPill();
  $('findings').innerHTML=(s.findings||[]).slice(0,60).map(f=>`<li class="f"><code class="${tone(f.severity)}">${esc(f.code)}</code> ${esc(f.file)}:${f.line}<br>${esc(f.message)}</li>`).join('')||'<li class="v">none</li>';
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


def shell(title: str, workspace: str) -> str:
    """The shell for one session, named by its title and its workspace, with the severity table it colours by."""
    table = json.dumps(driftspec.severities()).replace("</", "<\\/")
    return SHELL.replace("__TITLE__", html.escape(title)).replace("__WS__", html.escape(workspace)).replace("__SEVERITIES__", table)
