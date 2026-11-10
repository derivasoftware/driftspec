// The tours the viewer replays (DS-130): ds decides where each call goes on (query::stepsOf) and
// writes each behaviour once; the viewer follows those links when the reader plays a tour. These tests
// run the viewer's own decoding, taken from viewer/scene.html as it ships, over scenes ds writes.
// Run by ctest: DS is the ds binary, VIEWER the page template, ROOT the source tree.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const { DS, VIEWER, ROOT } = process.env;
const html = readFileSync(VIEWER, 'utf8');
const from = html.indexOf('// ---- the scene, decoded (');
const to = html.indexOf('// ---- the scene, decoded: end ----');
assert.ok(from > 0 && to > from, 'the viewer marks the part that decodes the scene');
const decode = new Function(`${html.slice(from, to)}\nreturn decode;`)();

const scene = (ws) => decode(JSON.parse(execFileSync(DS, ['render', ws, '-f', 'json'], { maxBuffer: 1 << 28 }).toString()));
const tour = (s, id) => {
  const t = s.tours.find((x) => x.id === id);
  assert.ok(t, `a tour ${id}`);
  return t;
};

// two classes: A runs, calls B::go, which steps twice and answers
const workspace = () => {
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
  const s = scene(join(ROOT, 'examples', 'branches'));
  for (const scenario of ['frozen', 'refused']) {
    const steps = tour(s, `bank@teller::withdraw / ${scenario}`).steps;
    const inner = steps.filter((x) => x.d === 2 && !x.ret).map((x) => x.m);
    const whole = tour(s, 'bank@teller::withdraw').steps.filter((x) => x.d === 2 && !x.ret).map((x) => x.m);
    assert.notDeepEqual(inner, whole, `${scenario} follows its own branch`);
  }
});

test("every interface command of ds's design is a tour that reaches the model and comes back", () => {
  const s = scene(join(ROOT, 'design'));
  const ids = new Set(s.nodes.map((n) => n.id));
  for (const t of s.tours) {
    for (const step of t.steps) {
      assert.ok(ids.has(step.f), `${t.id}: ${step.f}`);
      assert.ok(ids.has(step.t), `${t.id}: ${step.t}`);
    }
  }
  const check = tour(s, 'driftspec@ds@cli::check');
  assert.equal(check.group, 'interface');
  tour(s, 'driftspec@ds@cli::check / errors'); // a named scenario is a tour of its own
  assert.ok(check.steps.some((x) => x.t.startsWith('driftspec@ds@check::Checker::')));
  assert.ok(Math.max(...check.steps.map((x) => x.d)) >= 4);
  assert.equal(check.steps.at(-1).ret, 1); // the command answers
});

test('the page stays small because each behaviour is written once and the tours replay many more steps', () => {
  const raw = JSON.parse(execFileSync(DS, ['render', join(ROOT, 'design'), '-f', 'json'], { maxBuffer: 1 << 28 }).toString());
  const calls = raw.behaviours.reduce((n, b) => n + b[raw.fields.behaviour.indexOf('calls')].length, 0);
  const steps = decode(raw).tours.reduce((n, t) => n + t.steps.length, 0);
  assert.ok(calls * 5 < steps, `${calls} calls written, ${steps} steps replayed`);
});
