#!/usr/bin/env bash
# The home page's clip of the live viewer (DS-510), made again: playground/web/live/live.webm and live.mp4.
#   playground/live/make_clip.sh [chrome] [seconds]
# It copies the shop into a scratch workspace, serves it with `driftspec-live serve` (127.0.0.1:$LIVE_PORT, 4791) and
# this folder (127.0.0.1:$STAGE_PORT, 4790), both refused if already taken, lets record.mjs drive rec.html through Chrome's DevTools protocol (an editor on the left, the
# viewer on the right, an agent's MCP calls posted to the live server), saves shop.after.ds halfway, and encodes the
# frames with ffmpeg. Needs node 22 or newer, a Chrome or Chromium, ffmpeg with libvpx-vp9 and libx264, and the
# driftspec package (pip install driftspec, or PYTHONPATH pointing at a build's python folder).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
chrome=${1:-$(command -v chromium || command -v chromium-browser || command -v google-chrome)}
secs=${2:-30}
live_port=${LIVE_PORT:-4791}
stage_port=${STAGE_PORT:-4790}
for p in "$live_port" "$stage_port"; do  # a server already there would be recorded in place of this one
  if python3 -c "import socket,sys; s=socket.socket(); sys.exit(s.connect_ex(('127.0.0.1', $p)) != 0)"; then
    echo "port $p is taken: set LIVE_PORT or STAGE_PORT" >&2; exit 1; fi
done
work=$(mktemp -d)
trap 'kill "${srv:-}" "${web:-}" 2>/dev/null || true; rm -rf "$work"' EXIT
mkdir -p "$work/ws" && cp "$here/shop.before.ds" "$work/ws/shop.ds"
python3 -m driftspec.live serve "$work/ws" --port "$live_port" >"$work/serve.log" 2>&1 & srv=$!
python3 -m http.server "$stage_port" --bind 127.0.0.1 -d "$here" >"$work/web.log" 2>&1 & web=$!
sleep 2
kill -0 "$srv" 2>/dev/null || { cat "$work/serve.log" >&2; exit 1; }  # the live server must be this one
node "$here/record.mjs" "$chrome" "http://127.0.0.1:$stage_port/rec.html?live=$live_port" "$work/frames" "$work/ws/shop.ds" \
  "$here/shop.after.ds" "$here/shop.before.ds" 9333 "http://127.0.0.1:$live_port" "$secs"
frames=$(ls "$work/frames" | wc -l)
rate=$(python3 -c "print(round($frames / $secs, 2))")  # the pace the frames were taken at, so the clip runs in real time
out="$here/../web/live"
ffmpeg -y -loglevel error -framerate "$rate" -i "$work/frames/f%04d.jpg" -c:v libvpx-vp9 -b:v 0 -crf 40 -pix_fmt yuv420p -an "$out/live.webm"
ffmpeg -y -loglevel error -framerate "$rate" -i "$work/frames/f%04d.jpg" -vf fps=24 -c:v libx264 -preset slow -crf 28 \
  -pix_fmt yuv420p -movflags +faststart -an "$out/live.mp4"
echo "clip: $frames frames at $rate fps -> $out/live.webm, $out/live.mp4"
