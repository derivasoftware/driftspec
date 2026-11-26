// The first visit and the home page (DS-510), in headless Chromium over the site playground/build.py makes, served on
// 127.0.0.1: node playground/first_visit_check.cjs <site url> <folder for its screenshots>. Local tooling, not CI, like
// lessons/browser_check.sh: it needs Playwright (npm i -D playwright, or NODE_PATH at a folder that has it).
//  1. a first visit to the editor opens the tour, shows the loading animation, and the coach's card that talks about
//     the loaded design carries the timing line;
//  2. the home page shows the live clip, playing;
//  3. the home page alone loads no big design: not on a first visit, nor with a big design chosen on a past visit.
const { chromium } = require("playwright");
const [SITE, OUT] = process.argv.slice(2);
const ok = (c, what) => { console.log(`${c ? "ok  " : "FAIL"} ${what}`); if (!c) process.exitCode = 1; };
(async () => { const b = await chromium.launch(); try {
  // 1. first visit, straight to the editor
  { const ctx = await b.newContext({ viewport: { width: 1360, height: 860 } }); const p = await ctx.newPage(); const errs = []; p.on("pageerror", (e) => errs.push(String(e)));
    await p.addInitScript(() => { window.__spun = false; const look = () => { const el = document.getElementById("bigload"); if (el && el.classList.contains("on")) window.__spun = true; else requestAnimationFrame(look); }; requestAnimationFrame(look); });
    await p.goto(SITE + "?tab=editor"); await p.waitForFunction(() => window.__spun, null, { timeout: 30000 }).catch(() => {});
    const spun = await p.evaluate(() => window.__spun); await p.screenshot({ path: OUT + "/1-loading.png" });
    ok(spun, "a first visit to the editor shows the loading animation");
    await p.waitForFunction(() => !document.getElementById("bigload")?.classList.contains("on") && document.querySelector(".coach"), null, { timeout: 90000 });
    const sel = await p.evaluate(() => document.getElementById("example").value); ok(/tour/i.test(sel), `it opens on the tour (${sel})`);
    let note = null;
    for (let i = 0; i < 14 && !note; i++) {
      note = await p.evaluate(() => document.querySelector(".coach p.c-note")?.textContent || null); if (note) break;
      const clicked = await p.evaluate(() => { const bs = [...document.querySelectorAll(".coach .c-foot button")]; const b = bs[bs.length - 1]; if (!b) return false; b.click(); return true; });
      if (!clicked) break; await p.waitForTimeout(700); }
    await p.screenshot({ path: OUT + "/1-coach.png" });
    ok(!!note && /\d[.,]\d s/.test(note) && /PlantUML/.test(note), `the coach's card carries the timing line: ${note ? note.slice(0, 110) + "…" : "none"}`);
    ok(errs.length === 0, `no page errors ${JSON.stringify(errs)}`); await ctx.close(); }
  // 2 and 3. the home page alone, on a first visit and with a big design chosen before
  for (const [what, chosen] of [["a first visit", null], ["a big design chosen on a past visit", "Example: the design of DriftSpec itself"]]) {
    const ctx = await b.newContext({ viewport: { width: 1360, height: 860 } }); const p = await ctx.newPage();
    if (chosen) await p.addInitScript((c) => { try { localStorage.setItem("ds-playground-chosen", c); } catch (e) {} }, chosen);
    await p.addInitScript(() => { window.__spun = false; window.__long = 0; new PerformanceObserver((l) => l.getEntries().forEach((e) => { window.__long = Math.max(window.__long, e.duration); })).observe({ type: "longtask", buffered: true });
      const look = () => { const el = document.getElementById("bigload"); if (el && el.classList.contains("on")) window.__spun = true; setTimeout(look, 50); }; setTimeout(look, 0); });
    await p.goto(SITE); await p.waitForTimeout(6000);
    await p.evaluate(() => document.querySelector("video.live-clip")?.scrollIntoView({ block: "center" })); await p.waitForTimeout(3000);
    const s = await p.evaluate(() => { const v = document.querySelector("video.live-clip");
      return { tab: document.body.dataset.tab, spun: window.__spun, panes: document.querySelectorAll("main .pane").length, longest: Math.round(window.__long),
        clip: v ? { src: v.currentSrc.split("/").slice(-2).join("/"), playing: v.currentTime > 0 && !v.paused, visible: v.getBoundingClientRect().height > 100 } : null }; });
    if (!chosen) await p.screenshot({ path: OUT + "/2-home-clip.png" });
    ok(s.tab === "home", `${what}: the home page shows`);
    if (!chosen) ok(s.clip && s.clip.playing && s.clip.visible, `${what}: the home page plays the live clip ${JSON.stringify(s.clip)}`);
    ok(!s.spun && s.panes === 0, `${what}: no big design loads behind the home page (animation ${s.spun}, panes ${s.panes}, longest task ${s.longest} ms)`);
    await ctx.close(); }
} finally { await b.close(); } })();
