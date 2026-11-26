// The relations the viewer's map draws at rest (DS-504): inheritance alone unless the reader turns a kind on, from a
// list of every kind, the choice kept. These tests run the part of the viewer that decides it, taken from
// viewer/scene.html as it ships. Run by vitest, as tours.test.ts: ROOT is the source tree.
import { describe, test } from 'vitest';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

const ROOT = process.env.ROOT;
assert.ok(ROOT, 'ROOT is set');
const page = readFileSync(join(ROOT, 'viewer', 'scene.html'), 'utf8');
const from = page.indexOf('// ---- relations at rest, decided (');
const to = page.indexOf('// ---- relations at rest, decided: end ----');
assert.ok(from >= 0 && to > from, 'the viewer marks the part that decides the relations at rest');
const decided = new Function(`${page.slice(from, to)}\nreturn { REL_KINDS, relationsOn, restKindOf };`)() as {
  REL_KINDS: string[];
  relationsOn: (saved: string | null) => Record<string, boolean>;
  restKindOf: (kind: string, perspective: string, inOneBox: boolean) => string;
};
const on = (saved: string | null): string[] => Object.entries(decided.relationsOn(saved)).filter(([, v]) => v).map(([k]) => k);

describe('the relations the map draws at rest', () => {
  test('inheritance alone until the reader turns a kind on', () => {
    assert.deepEqual(on(null), ['inherits', 'realises']);
    assert.deepEqual(on('not json'), ['inherits', 'realises'], 'a choice that cannot be read is no choice');
    assert.deepEqual(on('{"calls":true}'), ['inherits', 'realises'], 'nor one that is not a list of kinds');
  });

  test('a choice is kept as the list of the kinds on, inheritance included', () => {
    assert.deepEqual(on('["calls","depends"]'), ['depends', 'calls']);
    assert.deepEqual(on('[]'), [], 'inheritance turned off stays off');
    assert.deepEqual(on('["inherits","nonsense"]'), ['inherits'], 'a name that is no kind is left out');
  });

  test('every relation the map can draw is in the list, each drawn in a style of its own', () => {
    assert.deepEqual(decided.REL_KINDS, ['inherits', 'realises', 'depends', 'owns', 'shares', 'refs', 'uses', 'calls', 'consumes', 'breaks']);
    const style = /const STYLE=\{([\s\S]*?)\};\n/.exec(page);
    assert.ok(style, 'the viewer writes a style per kind');
    for (const k of decided.REL_KINDS) assert.match(style[1], new RegExp(`(^|[,{\\s])${k}:\\{`), `${k} has its stroke`);
  });

  test('across two boxes in structure a relation is drawn as their dependency, unless it inherits; elsewhere as itself', () => {
    assert.equal(decided.restKindOf('calls', 'structure', false), 'depends');
    assert.equal(decided.restKindOf('owns', 'structure', false), 'depends');
    assert.equal(decided.restKindOf('inherits', 'structure', false), 'inherits');
    assert.equal(decided.restKindOf('realises', 'structure', false), 'realises');
    assert.equal(decided.restKindOf('owns', 'structure', true), 'owns');
    assert.equal(decided.restKindOf('calls', 'behaviour', false), 'calls');
    assert.equal(decided.restKindOf('breaks', 'logic', false), 'breaks');
  });
});
