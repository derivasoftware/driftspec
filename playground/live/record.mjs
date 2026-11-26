// The live clip's recording (DS-510): node record.mjs <chrome> <rec.html url> <frames dir> <workspace file> <after>
//   <before> <devtools port> <live server url> <seconds>. Run by make_clip.sh. Chrome runs headless with its own
// profile and, for this local stage only, without web security, so the stage can reach into the viewer's frame.
import { spawn } from "node:child_process";
import { writeFileSync, readFileSync, copyFileSync, mkdirSync, rmSync } from "node:fs";
const [chrome, url, outDir, wsFile, afterFile, beforeFile, port, live, secs] = process.argv.slice(2);
rmSync(outDir, { recursive: true, force: true }); mkdirSync(outDir, { recursive: true });
copyFileSync(beforeFile, wsFile);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const post = (path, body) => fetch(live + path, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body) }).catch(() => {});
await post("/clear", { what: "all" }); await post("/isolate", { ids: [], tell: false });
await sleep(1500);
const ch = spawn(chrome, ["--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars", `--remote-debugging-port=${port}`, `--user-data-dir=${outDir}-prof`, "--window-size=1440,810", "--disable-web-security", "--disable-site-isolation-trials", "--disable-features=IsolateOrigins,site-per-process", "about:blank"], { stdio: "ignore" });
for (let i = 0; i < 50; i++) { try { await fetch(`http://127.0.0.1:${port}/json/version`); break; } catch { await sleep(200); } }
const targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d.result); pend.delete(d.id); } };
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (e) => (await send("Runtime.evaluate", { expression: e, returnByValue: true })).result?.value;
await send("Emulation.setDeviceMetricsOverride", { width: 1440, height: 810, deviceScaleFactor: 1, mobile: false });
await send("Page.enable"); await send("Page.navigate", { url }); await sleep(7000);

const CLICK = (re, not) => `(()=>{const all=[];for(const e of document.querySelectorAll("*")){const t=(e.textContent||"").trim(); if(new RegExp(${JSON.stringify(re)}).test(t)&&!(${JSON.stringify(not)}&&new RegExp(${JSON.stringify(not)}).test(t))&&e.getClientRects().length) all.push([t.length,e]);}
 all.sort((a,b)=>a[0]-b[0]); const el=all[0]&&(all[0][1].closest("button,a,li,summary,[role=button]")||all[0][1]); if(!el) return "none"; el.dispatchEvent(new MouseEvent("click",{bubbles:true})); return el.tagName+" :: "+(el.textContent||"").trim().slice(0,40);})()`;
async function inFrames(expr) {
  const tree = (await send("Page.getFrameTree")).frameTree; const ids = [];
  const walk = (n) => { ids.push(n.frame.id); (n.childFrames || []).forEach(walk); }; walk(tree);
  const out = [];
  for (const f of ids.slice(1)) { const w = await send("Page.createIsolatedWorld", { frameId: f, worldName: "rec" }); if (!w?.executionContextId) continue;
    const r = await send("Runtime.evaluate", { expression: expr, contextId: w.executionContextId, returnByValue: true }); out.push(r?.result?.value); }
  return out;
}
const say = (t) => ev(`say(${JSON.stringify(t)})`);
const D = "shop@sales@desk::Desk";
const plan = [
  [1200, () => { say("<b>agent</b> live_focus Desk"); post("/focus", { id: D }); }],
  [3800, () => { say(""); ev("go()"); }],
  [7400, () => { writeFileSync(wsFile, readFileSync(afterFile)); }],
  [9500, () => { say("<b>agent</b> live_note Desk::shelf"); post("/note", { id: D + "::shelf", text: "the desk must not reach the stock", tone: "bad" }); }],
  [13500, () => { say("<b>agent</b> live_focus Desk::sell"); post("/focus", { id: D + "::sell" }); }],
  [14700, () => inFrames(CLICK("Desk\\.sell","sold")).then((ok) => console.log("tour click", ok))],
  [19500, () => { say("<b>agent</b> live_focus REQ-00001-1"); post("/focus", { id: "REQ-00001-1" }); }],
  [23500, () => { say("<b>agent</b> live_isolate warehouse"); post("/isolate", { ids: ["shop@warehouse"], tell: true }); }],
];
let n = 0; const t0 = Date.now(); let k = 0;
while (Date.now() - t0 < Number(secs) * 1000) {
  const el = Date.now() - t0;
  while (k < plan.length && el >= plan[k][0]) { await plan[k][1](); k++; }
  const shot = await send("Page.captureScreenshot", { format: "jpeg", quality: 88 });
  writeFileSync(`${outDir}/f${String(n++).padStart(4, "0")}.jpg`, Buffer.from(shot.data, "base64"));
  const ahead = n * 80 - (Date.now() - t0); if (ahead > 0) await sleep(ahead);
}
console.log(`frames ${n} in ${((Date.now() - t0) / 1000).toFixed(1)} s → ${(n / ((Date.now() - t0) / 1000)).toFixed(1)} fps`);
ws.close(); ch.kill(); copyFileSync(beforeFile, wsFile); await post("/clear", { what: "all" }); await post("/isolate", { ids: [], tell: false });
