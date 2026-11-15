#!/usr/bin/env bash
# Play each lesson in headless Chrome with a scripted reader (Got it / Write it for me / Show me)
# and compare the page's final files with replay.py's (DS-204). Local tooling, not CI: Chrome on
# the Windows side of WSL, and the site from playground/build.py served on localhost:$PORT
# (default 8766) from playground/site, e.g. `python3 -m http.server 8766 -d playground/site`.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WEB="$HERE/../site"
PORT="${PORT:-8766}"
CHROME="/mnt/c/Program Files/Google/Chrome/Application/chrome.exe"
TMP=$(mktemp -d)
status=0
for dir in "$HERE"/[0-9]*/; do
  id=$(basename "$dir")
  version=$(python3 -c "import json;print(json.load(open('$dir/lesson.json')).get('version',1))")
  python3 - "$WEB" "$id" "$version" <<'EOF'
import sys
web, lesson, version = sys.argv[1:]
base = open(f"{web}/index.html").read()
bot = """<script>
const seen=[];let n=0;
const tick=setInterval(()=>{
  const c=document.querySelector(".coach");
  if(c){const h=c.querySelector(".c-head").textContent.replace(/\\s+/g," ").trim(); if(seen[seen.length-1]!==h) seen.push(h);
    const a=c.querySelector(".c-ack:not(.c-next)"); const f=c.querySelector(".c-for-me");
    if(a&&!a.disabled) a.click(); else if(f) f.click();}
  if(++n>600) clearInterval(tick);
  const P=window.dsPlayground; if(!P) return;
  document.body.setAttribute("data-dbg", JSON.stringify({seen, done: P.done(), files: P.files(), count: document.getElementById("count").textContent, coach: !!document.querySelector(".coach:not(.next)"), next: (document.querySelector(".coach.next p")||{}).textContent||""}));
},400);
</script></body>""".replace("LESSON", lesson).replace("VERSION", version)
open(f"{web}/bot-{lesson}.html", "w").write(base.replace("<head>", "<head><script>try{localStorage.clear()}catch(e){}</script>", 1).replace("</body>", bot))
EOF
  PROF="C:\\Users\\dcpul\\Desktop\\cap${RANDOM}${RANDOM}"
  "$CHROME" --headless=new --disable-gpu --user-data-dir="$PROF" --window-size=1400,820 --virtual-time-budget=240000 \
    --dump-dom "http://localhost:$PORT/bot-$id.html?example=$id&lang=en" 2>/dev/null > "$TMP/$id.html"
  rm -f "$WEB/bot-$id.html"
  python3 - "$TMP/$id.html" "$dir" <<'EOF' || status=1
import sys, json, html, re, subprocess, pathlib
dom, folder = sys.argv[1:]
m = re.search(r'data-dbg="([^"]*)"', open(dom).read())
data = json.loads(html.unescape(m.group(1)))
replay = subprocess.run(["python3", str(pathlib.Path(folder).parent / "replay.py"), folder, "--print"], capture_output=True, text=True).stdout
parts = re.split(r"^----- (.+)$", replay, flags=re.M)
expected = {parts[i]: parts[i + 1].strip() for i in range(1, len(parts), 2)}
got = {k: v.strip() for k, v in data["files"].items()}
same = expected == got
done = data["done"]
print(f"{pathlib.Path(folder).name}: steps {' > '.join(data['seen'])} | done={done} | {data['count']} | same text as replay={same} | end card: {data['next']}")
if not same:
    import difflib
    for name in sorted(set(expected) | set(got)):
        a, b = expected.get(name, ""), got.get(name, "")
        if a != b:
            print(f"--- {name}")
            print("\n".join(difflib.unified_diff(a.split("\n"), b.split("\n"), "replay", "page", lineterm="")))
sys.exit(0 if same and done else 1)
EOF
done
rm -rf /mnt/c/Users/dcpul/Desktop/cap[0-9]* "$TMP"
exit $status
