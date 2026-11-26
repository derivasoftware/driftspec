// A drawing at rest and lit on every page (DS-491): viewer/drawings.js is the one script the playground and the viewer
// show ds's drawings with. These tests run the part of it that decides a drawing at rest, taken from the file as it
// ships, over drawings ds writes, and read the viewer's page ds writes to find the script in it. Run by vitest, as
// tours.test.ts: DS is the ds binary, ROOT the source tree.
import { describe, test } from 'vitest';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { inflateSync } from 'node:zlib';

const env = (name: string): string => {
  const value = process.env[name];
  assert.ok(value, `${name} is set`);
  return value;
};
const DS = env('DS');
const ROOT = env('ROOT');
const js = readFileSync(join(ROOT, 'viewer', 'drawings.js'), 'utf8');
const from = js.indexOf('// ---- a drawing at rest, decided (');
const to = js.indexOf('// ---- a drawing at rest, decided: end ----');
assert.ok(from >= 0 && to > from, 'the script marks the part that decides a drawing at rest');
type Rest = { rk: string; quiet: boolean };
const decided = new Function(`${js.slice(from, to)}\nreturn { relationAtRest, colourOfCard, tipSize };`)() as {
  relationAtRest: (edgeClasses: string[], fromClass: string | null, toClass: string | null) => Rest;
  colourOfCard: (cardClass: string | null) => string | null;
  tipSize: (written: string | null) => number;
};

// a class that inherits, owns, refers and uses, among classes and an enum; and a walk through its diagram
const workspace = (walk: boolean): string => {
  const dir = mkdtempSync(join(tmpdir(), 'ds-drawings-'));
  writeFileSync(join(dir, 'p.ds'), `package p {
  module m {
    class Base { }
    class Child : Base {
      - part: Part
      - refs other: Other
    }
    class Part { }
    class Other { }
    enum Colour { red }
    class User {
      + paint(c: Colour) -> void
    }
  }
}
diagram CL_P {
  kind: class
  include: p@m::*
}
${walk ? `diagram WALK_P {
  kind: walkthrough
  title: The classes
  step All of them {
    show: CL_P
    text: Every class of the module.
  }
}
` : ''}`);
  return dir;
};
const svg = (ws: string, diagram: string): string => execFileSync(DS, ['render', ws, diagram, '-f', 'svg']).toString();
const attr = (tag: string, name: string): string | null => new RegExp(`\\b${name}="([^"]*)"`).exec(tag)?.[1] ?? null;

// each relation of a drawing as the script meets it: its edge's classes and the classes of the boxes at its ends
const relations = (drawing: string) => {
  const boxes = new Map<string, string>();
  for (const m of drawing.matchAll(/<g [^>]*\bclass="pr-box[^"]*"[^>]*>/g)) boxes.set(attr(m[0], 'data-id')!, attr(m[0], 'class')!);
  const out: { from: string; to: string; edge: string[]; at: [string | null, string | null] }[] = [];
  for (const m of drawing.matchAll(/<g class="pr-edge-group"[^>]*>([\s\S]*?)<\/g>/g)) {
    const edge = /<path class="(pr-edge(?: [^"]*)?)"/.exec(m[1]);
    const f = attr(m[0], 'data-from')!, t = attr(m[0], 'data-to')!;
    out.push({ from: f, to: t, edge: edge ? edge[1].split(' ') : [], at: [boxes.get(f) ?? null, boxes.get(t) ?? null] });
  }
  return out;
};

describe('a drawing at rest, the same on every page', () => {
  test('each relation says its kind, and one that touches a class waits to be asked unless it inherits', () => {
    const rest = new Map(relations(svg(workspace(false), 'CL_P')).map((r) => [`${r.from} -> ${r.to}`, decided.relationAtRest(r.edge, ...r.at)]));
    assert.deepEqual(Object.fromEntries(rest), {
      'p@m::Child -> p@m::Base': { rk: 'inherits', quiet: false },
      'p@m::Child -> p@m::Part': { rk: 'has', quiet: true },
      'p@m::Child -> p@m::Other': { rk: 'refers', quiet: true },
      'p@m::User -> p@m::Colour': { rk: 'uses', quiet: true },
    });
    // between two interfaces, or ends the drawing does not hold, a relation stays; an edge of no kind is a use
    assert.deepEqual(decided.relationAtRest(['pr-edge', 'pr-edge-dependency'], 'pr-box pr-classifier-interface', null), { rk: 'uses', quiet: false });
    assert.deepEqual(decided.relationAtRest([], null, null), { rk: 'uses', quiet: false });
  });

  test('a tip wears the colour of the kind of card it touches and is drawn at three quarters of its size', () => {
    assert.equal(decided.colourOfCard('pr-box pr-classifier-class'), '--k-class');
    assert.equal(decided.colourOfCard('pr-box pr-classifier-enum pr-stereotype-enum'), '--k-enum');
    assert.equal(decided.colourOfCard('pr-box pr-classifier-package'), undefined);  // a package keeps the tip as drawn
    assert.equal(decided.colourOfCard(null), null);
    const markers = [...svg(workspace(false), 'CL_P').matchAll(/<marker [^>]*>/g)].map((m) => attr(m[0], 'markerWidth'));
    assert.ok(markers.length > 0, 'the drawing has tips');
    for (const w of markers) assert.equal(decided.tipSize(w), Number(w) * 0.75);
    assert.equal(decided.tipSize(null), 9);  // a tip with no width written is a 12 drawn at 9
  });

  test('the viewer carries the script as it ships, ahead of the walkthroughs, and only on a page with one', () => {
    // the viewer's code is packed into the page (deflate, then base64) and inflated when the page opens
    const page = (walk: boolean): string => {
      const html = execFileSync(DS, ['render', workspace(walk), '-f', 'html'], { maxBuffer: 1 << 28 }).toString();
      const packed = /const VIEWER='([^']*)'/.exec(html);
      assert.ok(packed, 'the page packs the viewer\'s code');
      return inflateSync(Buffer.from(packed[1], 'base64')).toString();
    };
    const walked = page(true);
    const at = walked.indexOf(js.trim().split('\n')[0]);
    assert.ok(at >= 0 && walked.includes(js.trim()), 'the page carries drawings.js byte for byte');
    assert.ok(at < walked.indexOf('// The walkthroughs\' mode of the viewer'), 'ahead of the mode that shows the drawings');
    assert.ok(!page(false).includes('// A ds drawing at rest and lit'), 'a page with no walkthrough is written as it was');
  });

  // one look (DS-493): a walkthrough shows a diagram as ds render draws it, the bytes the playground's engine draws, and
  // neither page restyles it: how a drawing looks is its own stylesheet's (DS-490), how it rests and lights drawings.js's
  test('a walkthrough carries each diagram as ds render draws it, and wires it as the playground does', () => {
    const ws = workspace(true);
    const html = execFileSync(DS, ['render', ws, '-f', 'html'], { maxBuffer: 1 << 28 }).toString();
    const packed = /const DRAWINGS='([^']*)'/.exec(html);
    assert.ok(packed, 'the page packs the drawings');
    const drawn = JSON.parse(inflateSync(Buffer.from(packed[1], 'base64')).toString()) as Record<string, string>;
    assert.equal(drawn.CL_P, svg(ws, 'CL_P'), 'byte for byte the drawing ds render writes');
    const walk = readFileSync(join(ROOT, 'viewer', 'walk.js'), 'utf8');
    assert.match(walk, /wireDrawing\(stage\.querySelector\('svg'\)/, 'the stage\'s drawing at rest and lit by drawings.js');
    assert.match(walk, /\.th svg'\)\.forEach\(svg=>wireDrawing\(/, 'each card\'s drawing too');
  });

  test('neither the playground nor the walkthroughs restyle a drawing', () => {
    const styles = (text: string): string => [...text.matchAll(/<style[^>]*>([\s\S]*?)<\/style>/g)].map((m) => m[1]).join('\n');
    const pages: [string, string][] = [
      ['the playground', styles(readFileSync(join(ROOT, 'playground', 'web', 'index.html'), 'utf8'))],
      ['the walkthroughs', readFileSync(join(ROOT, 'viewer', 'walk.js'), 'utf8')],
    ];
    for (const [name, css] of pages) {
      assert.doesNotMatch(css, /--pr-[a-z-]+\s*:/, `${name} sets none of a drawing's tokens`);
      assert.doesNotMatch(css, /\.pr-(sep|badge|head|header|name|type|param|punct|vis)\b[^{]*\{/, `${name} draws no part of a card its own way`);
    }
  });
});
