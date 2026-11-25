// The tours the viewer replays (DS-130): ds decides where each call goes on (query::stepsOf) and
// writes each behaviour once; the viewer follows those links when the reader plays a tour. These tests
// run the viewer's own decoding, taken from viewer/scene.html as it ships, over scenes ds writes.
// Run by vitest (DS-329), from the viewer-js job and from ctest: DS is the ds binary, VIEWER the
// page template, ROOT the source tree.
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
const VIEWER = env('VIEWER');
const ROOT = env('ROOT');
const html = readFileSync(VIEWER, 'utf8');
const from = html.indexOf('// ---- the scene, decoded (');
const to = html.indexOf('// ---- the scene, decoded: end ----');
assert.ok(from > 0 && to > from, 'the viewer marks the part that decodes the scene');
// the viewer is plain JavaScript: its scene is read untyped
type Scene = any;
const decode = new Function(`${html.slice(from, to)}\nreturn decode;`)() as (raw: unknown) => Scene;

const scene = (ws: string): Scene => decode(JSON.parse(execFileSync(DS, ['render', ws, '-f', 'json'], { maxBuffer: 1 << 28 }).toString()));
const tour = (s: Scene, id: string): Scene => {
  const t = s.tours.find((x: Scene) => x.id === id);
  assert.ok(t, `a tour ${id}`);
  return t;
};

// two classes: A runs, calls B::go, which steps twice and answers
const workspace = (): string => {
  const dir = mkdtempSync(join(tmpdir(), 'ds-tours-'));
  writeFileSync(join(dir, 'p.ds'), `package p {
  module m {
    class A {
      - b: B
      + run() -> none
    }
    class B {
      + go() -> int
      + step() -> none
    }
    sequence A::run {
      self -> b.go() -> goed
    }
    sequence B::go {
      self -> step()
      self -> step()
      return 1
    }
  }
}
`);
  return dir;
};

describe('the tours the viewer replays', () => {
  test('a call expands into the behaviour of what it calls, and its answer comes back', () => {
    const steps = tour(scene(workspace()), 'p@m::A::run').steps;
    assert.equal(steps.length, 4); // run -> go, go -> step twice, go's answer back to run
    assert.deepEqual([steps[0].f, steps[0].t, steps[0].m, steps[0].d], ['p@m::A::run', 'p@m::B::go', 'go()', 1]);
    assert.deepEqual([steps[1].f, steps[1].t, steps[1].d], ['p@m::B::go', 'p@m::B::step', 2]);
    assert.equal(steps[1].ret, undefined);
    assert.deepEqual([steps[3].f, steps[3].t, steps[3].m, steps[3].ret], ['p@m::B::go', 'p@m::A::run', 'goed', 1]);
  });

  test('a step goes one level deeper for each behaviour entered', () => {
    const steps = tour(scene(workspace()), 'p@m::A::run').steps;
    assert.equal(steps[0].d + 1, steps[1].d);
  });

  test('a call that names a scenario goes on into that branch, not the default path', () => {
    const s = scene(join(ROOT, 'tests', 'corpus', 'branches'));
    for (const scenario of ['frozen', 'refused']) {
      const steps = tour(s, `bank@teller::withdraw / ${scenario}`).steps;
      const inner = steps.filter((x: Scene) => x.d === 2 && !x.ret).map((x: Scene) => x.m);
      const whole = tour(s, 'bank@teller::withdraw').steps.filter((x: Scene) => x.d === 2 && !x.ret).map((x: Scene) => x.m);
      assert.notDeepEqual(inner, whole, `${scenario} follows its own branch`);
    }
  });

  // ds's own design is loaded and checked whole to write its scene: about 2.3 s with the machine idle, nearly all of
  // it in ds (the decoding and every check here take some 15 ms), past vitest's 5 s twice at a load of 10-12, and
  // growing with the design. The claim is about ds's live design, so the test keeps it and is given the time (DS-457).
  test("every interface command of ds's design is a tour that reaches the model and comes back", () => {
    const s = scene(join(ROOT, 'design'));
    const ids = new Set(s.nodes.map((n: Scene) => n.id));
    for (const t of s.tours) {
      for (const step of t.steps) {
        assert.ok(ids.has(step.f), `${t.id}: ${step.f}`);
        assert.ok(ids.has(step.t), `${t.id}: ${step.t}`);
      }
    }
    const check = tour(s, 'driftspec@ds@cli::check');
    assert.equal(check.group, 'interface');
    tour(s, 'driftspec@ds@cli::check / errors'); // a named scenario is a tour of its own
    // the load holds the checker as the IChecker it is (DS-404): the tour reaches the check through the interface
    assert.ok(check.steps.some((x: Scene) => x.t === 'driftspec@ds@check::IChecker::run'));
    assert.ok(Math.max(...check.steps.map((x: Scene) => x.d)) >= 4);
    assert.equal(check.steps.at(-1).ret, 1); // the command answers
  }, 30_000);

  // one behaviour of STEPS calls, called from CALLERS places: each caller's tour replays it
  test('a behaviour written once is replayed by every tour that calls it', () => {
    const CALLERS = 4;
    const STEPS = 5;
    const dir = mkdtempSync(join(tmpdir(), 'ds-tours-'));
    const callers = Array.from({ length: CALLERS }, (_, i) => `C${i + 1}`);
    writeFileSync(join(dir, 'p.ds'), `package p {
  module m {
    class Leaf {
      + work() -> none
      + step() -> none
    }
${callers.map((c) => `    class ${c} {\n      - leaf: Leaf\n      + run() -> none\n    }\n    sequence ${c}::run {\n      self -> leaf.work()\n    }\n`).join('')}    sequence Leaf::work {
${'      self -> step()\n'.repeat(STEPS)}    }
  }
}
`);
    const raw = JSON.parse(execFileSync(DS, ['render', dir, '-f', 'json']).toString());
    const calls = raw.behaviours.reduce((n: number, b: Scene) => n + b[raw.fields.behaviour.indexOf('calls')].length, 0);
    assert.equal(calls, CALLERS + STEPS); // each caller's one call, and work's steps written once
    const s = decode(raw);
    for (const c of callers) {
      const steps = tour(s, `p@m::${c}::run`).steps;
      assert.equal(steps.length, 1 + STEPS + 1, c); // the call, work's steps, work's answer back
      assert.equal(steps.filter((x: Scene) => x.f === 'p@m::Leaf::work' && x.t === 'p@m::Leaf::step').length, STEPS, c);
    }
    const replayed = s.tours.reduce((n: number, t: Scene) => n + t.steps.length, 0);
    assert.equal(replayed, CALLERS * (STEPS + 2) + STEPS); // and work's own tour
  });
});
