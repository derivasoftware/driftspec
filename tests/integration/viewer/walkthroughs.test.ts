// The walkthroughs the viewer plays (DS-470): ds writes each step's diagram by name, what it lights by identity and
// the behaviour a sequence step replays; the viewer's mode (viewer/walk.js, put into a page only when its workspace
// has a walkthrough) decodes them. These tests run that decoding, taken from walk.js as it ships, over scenes ds
// writes. Run by vitest, as tours.test.ts: DS is the ds binary, ROOT the source tree.
import { describe, test } from 'vitest';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const env = (name: string): string => {
  const value = process.env[name];
  assert.ok(value, `${name} is set`);
  return value;
};
const DS = env('DS');
const ROOT = env('ROOT');
const js = readFileSync(join(ROOT, 'viewer', 'walk.js'), 'utf8');
const from = js.indexOf('// ---- the walkthroughs, decoded (');
const to = js.indexOf('// ---- the walkthroughs, decoded: end ----');
assert.ok(from >= 0 && to > from, 'the mode marks the part that decodes the walkthroughs');
type Scene = any;
const walkDecode = new Function(`${js.slice(from, to)}\nreturn walkDecode;`)() as (raw: unknown) => Scene[];
const raw = (ws: string): Scene => JSON.parse(execFileSync(DS, ['render', ws, '-f', 'json'], { maxBuffer: 1 << 28 }).toString());

// a desk that sells, a requirement it answers, a sequence with a scenario written apart, and a walk through them
const workspace = (): string => {
  const dir = mkdtempSync(join(tmpdir(), 'ds-walks-'));
  writeFileSync(join(dir, 'p.ds'), `package p {
  interface shell {
    kind: cli
    operation buy(item: string) -> int
  }
  sequence shell::buy {
    local desk: p@m::Desk
    self -> desk.sell(item) -> code
    return code
  }
  sequence shell::buy / late {
    local desk: p@m::Desk
    self -> desk.sell(item) -> code
    return code
  }
  module m {
    requirement REQ-00001-1 {
      title: The desk shall sell
      status: draft
    }
    class Desk {
      @requirement REQ-00001-1
      + sell(item: string) -> int
    }
  }
}
diagram REQ_P {
  kind: requirement
  include: p@*
}
diagram SEQ_Buy {
  kind: sequence
  include: p@shell::buy
}
diagram WALK_P {
  kind: walkthrough
  title: The desk
  step The promise {
    show: REQ_P
    focus: REQ-00001-1
    at: REQ-00001-1
    text: It sells.
  }
  step Late {
    show: SEQ_Buy / late
    focus: p@m::Desk::sell
    highlight: p@shell::buy
    text: Late, it still sells.
  }
}
`);
  return dir;
};

describe('the walkthroughs the viewer plays', () => {
  test('each step comes with its diagram by name and what it lights by identity, in the order written', () => {
    const walks = walkDecode(raw(workspace()));
    assert.equal(walks.length, 1);
    assert.equal(walks[0].name, 'WALK_P');
    assert.equal(walks[0].title, 'The desk');
    const [promise, late] = walks[0].steps;
    assert.deepEqual([promise.title, promise.diagram, promise.focus, promise.at], ['The promise', 'REQ_P', ['REQ-00001-1'], 'REQ-00001-1']);
    assert.deepEqual([late.diagram, late.focus, late.highlight, late.at], ['SEQ_Buy', ['p@m::Desk::sell'], ['p@shell::buy'], '']);
  });

  test('a step that shows a sequence along a scenario names the behaviour the tour replay plays', () => {
    const [promise, late] = walkDecode(raw(workspace()))[0].steps;
    assert.equal(promise.behaviour, '');
    assert.equal(late.behaviour, 'p@shell::buy / late');
  });

  test('a scene with no walkthrough has none to play', () => {
    assert.deepEqual(walkDecode(raw(join(ROOT, 'examples', 'hola'))), []);
  });
});
