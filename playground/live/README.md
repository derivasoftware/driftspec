# The home page's live clip

`playground/web/live/live.webm` and `live.mp4` show the live viewer beside an editor: an agent focuses a class,
the file is saved and the viewer draws it again with its finding, a note, a tour, a requirement, an isolated box.
The home page plays it in its "Live, beside your editor" section (DS-510).

These files make it again:

| File | What |
|---|---|
| `make_clip.sh` | the whole run: a scratch copy of the shop, the live server, the recording, the encoding |
| `record.mjs` | drives `rec.html` in headless Chrome through the DevTools protocol, posts the agent's calls to the live server on a timed plan, saves the edit, and takes a frame every 80 ms |
| `rec.html` | the stage: the file being edited on the left, the live viewer (`127.0.0.1`, at the port `?live=` gives) on the right, the agent's calls below |
| `shop.before.ds`, `shop.after.ds` | the shop before and after the save: the desk comes to refer to the shelf, which the shop's rule forbids |

```bash
playground/live/make_clip.sh            # or: make_clip.sh /path/to/chrome 30
```

It needs node 22 or newer, a Chrome or Chromium, ffmpeg with VP9 and H.264, and the driftspec package. The servers
listen on 127.0.0.1 only, at 4791 and 4790 unless `LIVE_PORT` and `STAGE_PORT` say otherwise; a port already taken stops
the run. The scratch workspace is removed at the end.
