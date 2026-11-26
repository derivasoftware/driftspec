// The walkthroughs' mode of the viewer (DS-470, built as DS-447 v3): ds puts this file into a page only when its
// workspace writes a walkthrough, inside the viewer's own code, and DRAWINGS beside the scene (every diagram as
// `ds render <ws> <D> -f svg` draws it). The cards are every diagram; the shown one grows into the stage; the coach
// speaks beside what the step lights and never over it; a sequence step hands its behaviour to the tour replay.
// ---- the walkthroughs, decoded (tests/integration/viewer runs this part as it is: keep it free of the page) ----
// the scene's walkthroughs come columnar like the rest (DS-471): a step names its diagram as diagrams[] does, what it
// lights by identity as the drawing's data-id names it, and the behaviour a sequence step replays by its index in
// behaviours[] (-1 for none)
function walkDecode(r){if(!r||!r.walkthroughs) return [];
  const col=name=>{const f=r.fields[name]; return row=>{const o={}; f.forEach((k,i)=>{o[k]=row[i];}); return o;};};
  const id=i=>i>=0&&r.nodes[i]?r.nodes[i][r.fields.node.indexOf('id')]:'';
  const behaviour=i=>{const b=i>=0&&r.behaviours[i]; if(!b) return ''; const f=r.fields.behaviour, s=id(b[f.indexOf('subject')]), sc=b[f.indexOf('scenario')];
    return s+(sc?' / '+sc:'');};
  return r.walkthroughs.map(col('walkthrough')).map(w=>({name:w.name,title:w.title,
    steps:w.steps.map(col('step')).map(s=>({title:s.title,diagram:s.diagram,behaviour:behaviour(s.behaviour),
      focus:s.focus,highlight:s.highlight,at:s.at,text:s.text}))}));}
// ---- the walkthroughs, decoded: end ----
const WALKS=walkDecode(await unpack(PACKED));
const DRAWN=await (async()=>{try{return JSON.parse(await new Response(new Blob([Uint8Array.from(atob(DRAWINGS),c=>c.charCodeAt(0))]).stream()
  .pipeThrough(new DecompressionStream('deflate'))).text());}catch(e){return {};}})();
if(WALKS.length){
const wkStyle=document.createElement('style');
wkStyle.textContent=`
#walk{position:fixed;inset:0;z-index:40;background:var(--bg);color:var(--fg);font:14px/1.5 var(--sans);overflow:hidden;display:flex;flex-direction:column;--coach:var(--k-abstract)}
#walk[hidden]{display:none}
#walk header{flex:none}
/* the cards in a column at the side, the drawing and its step beside them, each scrolling on its own (DS-495) */
#wk-body{flex:1;min-height:0;display:grid;grid-template-columns:208px minmax(0,1fr)}
#walk header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:10px;padding:10px 20px;border-bottom:1px solid var(--line);background:var(--panel);flex-wrap:wrap}
#walk header h1{font-size:15px;margin:0;font-weight:600}
#walk header .kind{font:11px var(--mono);color:var(--muted)}
#walk header .sp{flex:1}
#walk header button,#walk header select{font:12px var(--mono);color:var(--muted);background:var(--soft);border:1px solid var(--line);border-radius:6px;padding:4px 10px;cursor:pointer}
#walk header button:hover{color:var(--fg)}
#wk-strip{position:relative;display:flex;flex-direction:column;gap:10px;padding:12px;overflow-y:auto;overflow-x:hidden;border-right:1px solid var(--line);background:var(--panel)}
#walk .wk-card{flex:0 0 auto;background:var(--bg);border:1px solid var(--line);border-radius:8px;overflow:hidden;cursor:pointer;transition:border-color .2s,box-shadow .2s}
#walk .wk-card h3{margin:0;display:flex;align-items:center;gap:6px;padding:4px 8px;font:600 11px var(--mono);border-bottom:1px solid var(--line);white-space:nowrap;overflow:hidden}
#walk .wk-card h3 .k,#walk .wk-card h3 .n{font-weight:400;color:var(--muted)}
#walk .wk-card h3 .n{margin-left:auto}
#walk .wk-card .th{height:104px;padding:6px;background:var(--bg);display:flex;align-items:center;justify-content:center;overflow:hidden}
#walk .wk-card .th svg{max-width:100%;max-height:92px;width:auto;height:auto;pointer-events:none}
#walk .wk-card.off{opacity:.55}
#walk .wk-card.here{border-color:var(--coach);box-shadow:0 0 0 1px var(--coach);opacity:1}
#walk .wk-card.here h3{color:var(--coach)}
#wk-main{display:grid;grid-template-columns:minmax(0,1fr) 340px;gap:16px;padding:16px 20px 40px;align-items:start;overflow-y:auto;min-height:0}
#wk-main.at{grid-template-columns:minmax(0,1fr)}
#wk-box{background:var(--panel);border:1px solid var(--line);border-radius:10px;min-height:420px;display:flex;flex-direction:column;overflow:hidden}
#wk-box h3{margin:0;display:flex;gap:8px;padding:6px 12px;font:600 12px var(--mono);border-bottom:1px solid var(--line)}
#wk-box h3 .k{font-weight:400;color:var(--muted)}
#wk-stage{flex:1;padding:14px;background:var(--bg);display:flex;justify-content:center;align-items:flex-start;overflow:auto;touch-action:pan-x pan-y pinch-zoom}
#wk-stage svg{max-width:100%;height:auto}
#wk-ptr{position:fixed;inset:0;width:100vw;height:100vh;z-index:60;pointer-events:none;opacity:0;transition:opacity .2s ease}
#wk-ptr.on{opacity:1}
#wk-ptr line{stroke:var(--coach);stroke-width:1.5}
#wk-ptr circle{fill:var(--coach)}
#wk-stage.focusing .pr-box,#wk-stage.focusing .pr-lifeline,#wk-stage.focusing .pr-msg-group,#wk-stage.focusing .pr-edge-group{transition:opacity .25s ease;opacity:.22}
#wk-stage.focusing .g-on{opacity:1!important}
#wk-stage .pr-edge-group.quiet.g-on{display:inline}
#wk-stage .g-focus>rect,#wk-stage .g-focus .pr-lifeline-head rect{stroke:var(--coach)!important;stroke-width:2.5px!important}
#wk-stage .pr-msg-group.g-focus path:not(.pr-edge-hit),#wk-stage .pr-msg-group.g-focus line,#wk-stage .pr-edge-group.g-focus path:not(.pr-edge-hit){stroke:var(--coach)!important}
#wk-stage .pr-msg-group.g-focus text{fill:var(--coach)!important;font-weight:600}
#wk-stage .g-hl>rect,#wk-stage .g-hl .pr-lifeline-head rect{stroke:var(--warn)!important;stroke-width:2px!important;stroke-dasharray:4 3}
#wk-stage .pr-msg-group.g-hl path:not(.pr-edge-hit),#wk-stage .pr-msg-group.g-hl line{stroke:var(--warn)!important}
#wk-stage .pr-msg-group.g-hl text{fill:var(--warn)!important;font-weight:600}
#wk-coach{position:relative;z-index:70;background:var(--panel);color:var(--fg);border:1px solid var(--line);border-radius:10px;box-shadow:0 10px 30px var(--shadow-pop);font:13px/1.5 var(--sans)}
#wk-coach::before{content:"";position:absolute;left:0;top:0;bottom:0;width:3px;border-radius:10px 0 0 10px;background:var(--coach)}
#wk-coach.callout{position:fixed;width:320px;margin:0;transition:left .32s cubic-bezier(.25,.1,.25,1),top .32s cubic-bezier(.25,.1,.25,1)}
#wk-coach.callout.settle{transition:none}
#wk-coach .hd{display:flex;align-items:center;gap:8px;padding:9px 12px 0 16px;font:11px var(--mono);color:var(--muted)}
#wk-coach .hd .sp{flex:1}
#wk-coach .dots{display:inline-flex;gap:3px}
#wk-coach .dots i{width:6px;height:6px;border-radius:50%;background:var(--line);cursor:pointer}
#wk-coach .dots i.on{background:var(--coach)}
#wk-coach h2{margin:6px 16px 0;font-size:13.5px;font-weight:600}
#wk-coach p{margin:4px 16px 0}
#wk-coach code{font:12px var(--mono);background:var(--bg);border:1px solid var(--line);border-radius:4px;padding:0 4px}
#wk-coach .lit{margin:8px 16px 0;display:flex;flex-wrap:wrap;gap:4px}
#wk-coach .lit span{font:11px var(--mono);border-radius:4px;padding:1px 6px;border:1px solid var(--coach);color:var(--coach)}
#wk-coach .lit span.hl{border-color:var(--warn);color:var(--warn);border-style:dashed}
#wk-coach .ft{display:flex;align-items:center;gap:8px;padding:10px 12px 12px 16px}
#wk-coach .back{margin-right:auto;font:12px var(--sans);color:var(--muted);background:none;border:0;padding:5px 0;cursor:pointer}
#wk-coach .back:disabled{visibility:hidden}
#wk-coach .replay{font:12px var(--mono);color:var(--coach);background:none;border:1px solid var(--coach);border-radius:6px;padding:4px 10px;cursor:pointer}
#wk-coach .replay[hidden]{display:none}
#wk-coach .next{font:600 12px var(--sans);color:var(--panel);background:var(--coach);border:0;border-radius:6px;padding:5px 12px;cursor:pointer}
#wk-coach.sheet{position:fixed;left:0;right:0;bottom:0;width:auto;margin:0;border-radius:14px 14px 0 0;border-bottom:0;max-height:46vh}
#wk-coach.sheet::before{border-radius:14px 0 0 0}
#wk-coach.sheet .bd{max-height:calc(46vh - 70px);overflow-y:auto}
@media (max-width:860px){#wk-main{grid-template-columns:1fr}}
@media (max-width:720px){
  #walk{display:block;overflow-y:auto;overflow-x:hidden}
  #wk-body{display:block}
  #wk-strip{flex-direction:row;overflow-x:auto;overflow-y:hidden;border-right:0;border-bottom:1px solid var(--line)}
  #wk-main{overflow:visible}
  #walk header{padding:10px 16px;gap:8px}
  #walk header h1{font-size:14px;flex:1 1 100%;order:1}
  #walk header .kind{order:2;font-size:10.5px}
  #walk header .sp{display:none}
  #walk header button,#walk header select{order:3;min-height:36px}
  #walk header #wk-close{margin-left:auto}
  #wk-strip{padding:10px 16px;gap:8px;scroll-snap-type:x mandatory;scrollbar-width:none}
  #wk-strip::-webkit-scrollbar{display:none}
  #walk .wk-card{flex:0 0 132px;scroll-snap-align:center}
  #walk .wk-card .th{height:78px}
  #walk .wk-card .th svg{max-height:66px}
  #wk-main{padding:12px 16px calc(46vh + 24px);gap:0}
  #wk-box{min-height:0}
  #wk-stage{padding:8px}
  #wk-stage svg{width:100%;max-width:100%}
  #wk-coach .ft{padding:8px 16px 14px}
  #wk-coach .back,#wk-coach .next{min-height:44px;min-width:88px;font-size:14px}
  #wk-coach .dots i{width:8px;height:8px}
  #wk-coach h2{font-size:14.5px}
  #wk-coach p{font-size:14px}
}
@media (prefers-reduced-motion:reduce){#wk-coach.callout{transition:none}#wk-ptr{transition:none}}`;
document.head.appendChild(wkStyle);
const wkEl=document.createElement('div'); wkEl.id='walk'; wkEl.hidden=true; wkEl.setAttribute('role','dialog'); wkEl.setAttribute('aria-label','walkthrough');
wkEl.innerHTML=`<header><h1 id="wk-title"></h1><span class="kind" id="wk-kind"></span><span class="sp"></span>`+
  (WALKS.length>1?`<select id="wk-pick" aria-label="walkthrough">${WALKS.map((w,i)=>`<option value="${i}">${esc(w.name)}</option>`).join('')}</select>`:'')+
  `<button id="wk-close" title="back to the map (Esc)">✕ map</button></header>`+
  `<div id="wk-body"><nav id="wk-strip" aria-label="every diagram of the workspace"></nav>`+
  `<main id="wk-main"><section id="wk-box"><h3><span id="wk-shown"></span><span class="k" id="wk-shownkind"></span></h3><div id="wk-stage"></div></section><aside id="wk-dock"></aside></main></div>`;
document.body.appendChild(wkEl);
const wkPtr=document.createElementNS('http://www.w3.org/2000/svg','svg'); wkPtr.id='wk-ptr'; wkPtr.setAttribute('aria-hidden','true');
wkPtr.innerHTML='<line/><circle r="3.5"/>'; wkEl.appendChild(wkPtr);
const wkCoach=document.createElement('div'); wkCoach.id='wk-coach'; wkCoach.setAttribute('aria-live','polite');
wkCoach.innerHTML=`<div class="hd"><span id="wk-count"></span><span class="sp"></span><span class="dots" id="wk-dots"></span></div>`+
  `<div class="bd"><h2 id="wk-step"></h2><p id="wk-text"></p><div class="lit" id="wk-lit"></div></div>`+
  `<div class="ft"><button class="back" id="wk-back">← Back</button><button class="replay" id="wk-replay" hidden title="replay this behaviour step by step over the map">▶ replay</button><button class="next" id="wk-next">Next →</button></div>`;
const $w=id=>document.getElementById(id);
$w('wk-dock').appendChild(wkCoach);
const wkStill=matchMedia('(prefers-reduced-motion: reduce)').matches;
const wkProse=s=>esc(s).replace(/`([^`]+)`/g,'<code>$1</code>');
// a copy of a drawing gets ids of its own, so its markers are its own and not the stage's
const wkCopy=(svg,tag)=>svg.replace(/id="([^"]+)"/g,`id="$1-${tag}"`).replace(/url\(#([^)]+)\)/g,`url(#$1-${tag})`).replace(/href="#([^"]+)"/g,`href="#$1-${tag}"`);
const wkKinds={}; (SCENE.diagrams||[]).forEach(d=>{wkKinds[d.name]=d.kind;});
// every diagram, in the order the scene lists them, then any it drew and does not list
const wkOrder=[...new Set([...(SCENE.diagrams||[]).map(d=>d.name).filter(n=>DRAWN[n]),...Object.keys(DRAWN)])];
let W=null, wkAt=-1;
const wkPick={selected:null};  // what the reader picked in the stage's drawing, kept from step to step
function wkStrip(){
  $w('wk-strip').innerHTML=wkOrder.map(d=>{const n=W.steps.map((s,i)=>s.diagram===d?i+1:0).filter(Boolean);
    return `<div class="wk-card${n.length?'':' off'}" data-d="${esc(d)}"><h3><span>${esc(d)}</span><span class="k">${esc(wkKinds[d]||'')}</span><span class="n">${n.join('·')}</span></h3><div class="th">${wkCopy(DRAWN[d],'c'+wkOrder.indexOf(d))}</div></div>`;}).join('');
  $w('wk-strip').querySelectorAll('.th svg').forEach(svg=>wireDrawing(svg,{selected:null}));
}
const wkCard=d=>[...document.querySelectorAll('#walk .wk-card')].find(c=>c.dataset.d===d);
// what an identity lights in a drawing: a box or a participant by its id; an operation by the messages that call it
function wkLights(svg,id){const out=[]; if(!svg||!id) return out;
  svg.querySelectorAll('[data-id]').forEach(g=>{if(g.dataset.id.split('|')[0]===id) out.push(g);});
  svg.querySelectorAll('.pr-msg-group[data-ref-expands]').forEach(g=>{if(g.dataset.refExpands.startsWith(id+'::')) out.push(g);});
  if(!out.length) svg.querySelectorAll('.pr-msg-group[data-to]').forEach(g=>{const callee=id.slice(0,id.lastIndexOf('::'));
    if(g.dataset.to.split('|')[0]===callee&&!g.classList.contains('pr-msg-return')) out.push(g);});
  return out;}
function wkLight(s){const stage=$w('wk-stage'), svg=stage.querySelector('svg'); if(!svg) return;
  svg.querySelectorAll('.g-on,.g-focus,.g-hl').forEach(g=>g.classList.remove('g-on','g-focus','g-hl'));
  for(const id of s.focus) for(const g of wkLights(svg,id)) g.classList.add('g-on','g-focus');
  for(const id of s.highlight) for(const g of wkLights(svg,id)) g.classList.add('g-on','g-hl');
  const on=new Set([...s.focus,...s.highlight]);
  svg.querySelectorAll('.pr-msg-group.g-on').forEach(m=>[m.dataset.from,m.dataset.to].forEach(end=>
    svg.querySelectorAll('.pr-lifeline').forEach(l=>{if(l.dataset.id===end) l.classList.add('g-on');})));
  svg.querySelectorAll('.pr-edge-group').forEach(e=>{if(on.has(e.dataset.from)&&on.has(e.dataset.to)) e.classList.add('g-on','g-focus');});
  stage.classList.toggle('focusing',s.focus.length>0);}
// a new drawing comes in calmly: the old one fades, the new one fades in from 96 %, grown from its card's side
async function wkSwap(s){const stage=$w('wk-stage'), card=wkCard(s.diagram);
  const box=stage.getBoundingClientRect(), c=card?card.getBoundingClientRect():box;
  const ox=Math.max(0,Math.min(100,((c.left+c.width/2-box.left)/Math.max(1,box.width))*100));
  if(!wkStill&&stage.firstChild) await stage.animate([{opacity:1},{opacity:0}],{duration:120,easing:'ease-in',fill:'forwards'}).finished;
  stage.innerHTML=DRAWN[s.diagram]||`<p style="color:var(--muted);font:12px var(--mono)">${esc(s.diagram)} is not drawn</p>`;
  wireDrawing(stage.querySelector('svg'),wkPick);  // at rest and lit as the playground leaves it (DS-493)
  wkLight(s);
  if(!wkStill){stage.style.transformOrigin=`${ox}% 0%`;
    await stage.animate([{opacity:0,transform:'scale(.96)'},{opacity:1,transform:'scale(1)'}],{duration:280,easing:'cubic-bezier(.25,.1,.25,1)',fill:'forwards'}).finished;}
  stage.getAnimations().forEach(a=>a.cancel());}
function wkReveal(el){const r=el.getBoundingClientRect(), top=$w('wk-main').getBoundingClientRect().top;
  if(r.top>=top&&r.bottom<=innerHeight-40) return Promise.resolve();
  el.scrollIntoView({behavior:wkStill?'auto':'smooth',block:'center',inline:'nearest'});
  return new Promise(ok=>setTimeout(ok,wkStill?0:420));}
// where the callout goes: the side with room that covers least of what is lit, then least of what is around it,
// and stays near its anchor
function wkSpot(anchor,w,h){const svg=$w('wk-stage').querySelector('svg'), rect=el=>el.getBoundingClientRect();
  const lit=[...svg.querySelectorAll('.g-on')].map(rect);
  const others=[...svg.querySelectorAll('.pr-box,.pr-lifeline-head,.pr-msg-group,.pr-edge-group')].filter(el=>!el.classList.contains('g-on')).map(rect);
  const a=rect(anchor), gap=18, cands=[], cy=a.top+a.height/2, cx=a.left+a.width/2, all=svg.getBoundingClientRect();
  const over=(x,y,r)=>Math.max(0,Math.min(x+w,r.right)-Math.max(x,r.left))*Math.max(0,Math.min(y+h,r.bottom)-Math.max(y,r.top));
  for(const dy of [-h/2,-h+30,-30]){cands.push({x:a.right+gap,y:cy+dy}); cands.push({x:a.left-gap-w,y:cy+dy});}
  for(const dx of [-w/2,-40,-w+40]){cands.push({x:cx+dx,y:a.bottom+gap}); cands.push({x:cx+dx,y:a.top-gap-h});}
  for(const dy of [-h/2,-40,-h+40]){cands.push({x:all.right+gap,y:cy+dy}); cands.push({x:all.left-gap-w,y:cy+dy});}
  for(const dx of [-w/2,-40,-w+40]) cands.push({x:cx+dx,y:all.bottom+gap});
  let best=null;
  const st=$w('wk-strip').getBoundingClientRect(), side=st.height>st.width, head=wkEl.querySelector('header').getBoundingClientRect();
  const top=Math.max(56,(side?head.bottom:st.bottom)+8), left=side?st.right+8:8;  // clear of the cards, never over them
  for(const c of cands){if(c.x<left||c.y<top||c.x+w>innerWidth-8||c.y+h>innerHeight-8) continue;
    let score=0; for(const r of lit) score+=100*over(c.x,c.y,r); for(const r of others) score+=8*over(c.x,c.y,r);
    const dx=Math.max(0,c.x-a.right,a.left-(c.x+w)), dy=Math.max(0,c.y-a.bottom,a.top-(c.y+h)); score+=60*Math.hypot(dx,dy);
    if(!best||score<best.score) best={...c,score};}
  return best||{x:Math.min(innerWidth-w-8,a.right+gap),y:Math.min(innerHeight-h-8,Math.max(top,cy-h/2))};}
function wkPoint(anchor){const c=wkCoach.getBoundingClientRect(), a=anchor.getBoundingClientRect();
  const ax=Math.max(a.left,Math.min(a.right,c.left+c.width/2)), ay=Math.max(a.top,Math.min(a.bottom,c.top+c.height/2));
  const px=Math.max(c.left,Math.min(c.right,ax)), py=Math.max(c.top+10,Math.min(c.bottom-10,ay));
  wkPtr.setAttribute('viewBox',`0 0 ${innerWidth} ${innerHeight}`);
  const l=wkPtr.querySelector('line'), d=wkPtr.querySelector('circle');
  l.setAttribute('x1',px); l.setAttribute('y1',py); l.setAttribute('x2',ax); l.setAttribute('y2',ay); d.setAttribute('cx',ax); d.setAttribute('cy',ay);
  wkPtr.classList.add('on');}
const wkNarrow=()=>innerWidth<=720;
const wkTarget=s=>s.at?wkLights($w('wk-stage').querySelector('svg'),s.at)[0]:null;
// on a phone the coach is a bottom sheet; its anchor is brought into the room above it
async function wkSheet(s){wkCoach.classList.remove('callout','settle'); wkCoach.style.left=wkCoach.style.top='';
  if(!wkCoach.classList.contains('sheet')){wkEl.appendChild(wkCoach); wkCoach.classList.add('sheet');}
  $w('wk-main').classList.remove('at'); const t=wkTarget(s); if(!t) return;
  const room=innerHeight-wkCoach.offsetHeight, r=t.getBoundingClientRect();
  if(r.top<60||r.bottom>room-12){wkEl.scrollBy({top:r.bottom-(room-90),behavior:wkStill?'auto':'smooth'}); await new Promise(ok=>setTimeout(ok,wkStill?0:420));}
  const now=t.getBoundingClientRect(); if(now.bottom<room-4&&now.top>0&&room-now.bottom<160) wkPoint(t);}
async function wkPlace(s){wkPtr.classList.remove('on');
  if(wkNarrow()) return wkSheet(s);
  if(wkCoach.classList.contains('sheet')){wkCoach.classList.remove('sheet'); $w('wk-dock').appendChild(wkCoach);}
  const t=innerWidth>860?wkTarget(s):null; $w('wk-main').classList.toggle('at',!!t);
  if(!t){wkCoach.classList.remove('callout'); wkCoach.style.left=wkCoach.style.top=''; $w('wk-dock').appendChild(wkCoach); return;}
  await wkReveal(t);
  const fresh=!wkCoach.classList.contains('callout');
  if(fresh){wkEl.appendChild(wkCoach); wkCoach.classList.add('callout','settle');}
  const c=wkSpot(t,wkCoach.offsetWidth,wkCoach.offsetHeight); wkCoach.style.left=c.x+'px'; wkCoach.style.top=c.y+'px';
  if(fresh){void wkCoach.offsetWidth; wkCoach.classList.remove('settle');}
  setTimeout(()=>wkPoint(t),fresh||wkStill?0:320);}
async function wkGo(i){const n=W.steps.length; i=(i+n)%n; if(i===wkAt) return;
  const before=wkAt>=0?W.steps[wkAt]:null, s=W.steps[i]; wkAt=i;
  document.querySelectorAll('#walk .wk-card').forEach(c=>c.classList.toggle('here',c.dataset.d===s.diagram));
  const here=wkCard(s.diagram), strip=$w('wk-strip');
  if(here){const side=strip.clientHeight>strip.clientWidth;  // the card in the middle of the strip, a column or a row
    strip.scrollTo(side?{top:here.offsetTop-(strip.clientHeight-here.offsetHeight)/2,behavior:wkStill?'auto':'smooth'}:{left:here.offsetLeft-(strip.clientWidth-here.offsetWidth)/2,behavior:wkStill?'auto':'smooth'});}
  wkPtr.classList.remove('on');
  if(before&&before.diagram===s.diagram) wkLight(s); else await wkSwap(s);  // the same drawing stays put: only what is lit changes
  const scen=s.behaviour.includes(' / ')?s.behaviour.slice(s.behaviour.indexOf(' / ')):'';
  $w('wk-shown').textContent=s.diagram+scen; $w('wk-shownkind').textContent=wkKinds[s.diagram]||'';
  $w('wk-count').textContent=`step ${i+1} of ${n}`; $w('wk-step').textContent=s.title; $w('wk-text').innerHTML=wkProse(s.text);
  $w('wk-lit').innerHTML=s.focus.map(x=>`<span>${esc(x)}</span>`).join('')+s.highlight.map(x=>`<span class="hl">${esc(x)}</span>`).join('');
  $w('wk-dots').innerHTML=W.steps.map((_,k)=>`<i class="${k===i?'on':''}" data-i="${k}"></i>`).join('');
  $w('wk-back').disabled=i===0; $w('wk-next').textContent=i===n-1?'Start over':'Next →';
  $w('wk-replay').hidden=!TBY[s.behaviour];
  await wkPlace(s);}
function wkOpen(k,step){W=WALKS[k]||WALKS[0]; wkAt=-1; wkEl.hidden=false; $w('wk-title').textContent=W.title||W.name;
  $w('wk-kind').textContent=`diagram ${W.name} · kind: walkthrough`; if($w('wk-pick')) $w('wk-pick').value=String(WALKS.indexOf(W));
  wkStrip(); $w('wk-stage').innerHTML=''; wkGo(Math.max(0,Math.min(W.steps.length-1,step||0)));}
function wkClose(){wkEl.hidden=true; wkPtr.classList.remove('on'); $w('wk-dock').appendChild(wkCoach); wkCoach.classList.remove('callout','sheet');}
$w('wk-next').onclick=()=>wkGo(wkAt+1); $w('wk-back').onclick=()=>wkGo(wkAt-1);
$w('wk-dots').onclick=e=>{if(e.target.dataset.i) wkGo(+e.target.dataset.i);};
$w('wk-strip').onclick=e=>{const c=e.target.closest('.wk-card'); if(!c) return; const i=W.steps.findIndex(s=>s.diagram===c.dataset.d); if(i>=0) wkGo(i);};
$w('wk-close').onclick=wkClose;
if($w('wk-pick')) $w('wk-pick').onchange=e=>wkOpen(+e.target.value,0);
// a sequence step hands its behaviour to the tour replay, over the map (DS-470: as agreed for DS-471's index)
$w('wk-replay').onclick=()=>{const s=W.steps[wkAt]; if(!TBY[s.behaviour]) return; wkClose(); playAt(s.behaviour,0);};
addEventListener('keydown',e=>{if(wkEl.hidden) return; if(e.key==='ArrowRight'){e.stopImmediatePropagation(); wkGo(wkAt+1);}
  if(e.key==='ArrowLeft'&&wkAt>0){e.stopImmediatePropagation(); wkGo(wkAt-1);} if(e.key==='Escape'){e.stopImmediatePropagation(); wkClose();}},true);
let wkTouch=null;
wkEl.addEventListener('touchstart',e=>{wkTouch=e.touches.length===1&&!e.target.closest('#wk-strip')?{x:e.touches[0].clientX,y:e.touches[0].clientY,t:Date.now()}:null;},{passive:true});
wkEl.addEventListener('touchend',e=>{if(!wkTouch||e.changedTouches.length!==1||(window.visualViewport&&visualViewport.scale>1.05)) return;
  const dx=e.changedTouches[0].clientX-wkTouch.x, dy=e.changedTouches[0].clientY-wkTouch.y;
  if(Math.abs(dx)>60&&Math.abs(dx)>1.6*Math.abs(dy)&&Date.now()-wkTouch.t<700){if(dx<0) wkGo(wkAt+1); else if(wkAt>0) wkGo(wkAt-1);} wkTouch=null;},{passive:true});
addEventListener('resize',()=>{if(!wkEl.hidden&&wkAt>=0) wkPlace(W.steps[wkAt]);});
wkEl.addEventListener('scroll',()=>{if(wkEl.hidden||wkAt<0) return; const t=wkTarget(W.steps[wkAt]); if(t&&wkCoach.classList.contains('callout')) wkPoint(t);},{passive:true});
// the mode's button, beside tours and requirements
const wkBtn=document.createElement('button'); wkBtn.id='walks'; wkBtn.textContent='walkthroughs'; wkBtn.title='a guided walk through the design, diagram by diagram';
const reqBtn=document.getElementById('reqs'); if(reqBtn) reqBtn.after(wkBtn); else document.body.appendChild(wkBtn);
wkBtn.onclick=()=>wkOpen(0,0);
// a page opened on a walkthrough plays it: ds render <ws> WALK_X -f html, or ?walk=WALK_X&step=N
const wkQ=new URLSearchParams(location.search), wkName=wkQ.get('walk')||(FOCUS.length===1&&WALKS.find(w=>'diagram:'+w.name===FOCUS[0])?FOCUS[0].slice(8):'');
const wkK=WALKS.findIndex(w=>w.name===wkName); if(wkK>=0) wkOpen(wkK,Math.max(0,(+wkQ.get('step')||1)-1));
}
