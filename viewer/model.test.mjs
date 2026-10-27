import { strict as assert } from 'node:assert';
import { readFile } from 'node:fs/promises';
import { partPosition, partVisible, clampExplosion } from './model.mjs';
const metadata = JSON.parse(await readFile(new URL('../cad/assembly-parts.json', import.meta.url)));
for (const part of metadata.parts) {
  assert.deepEqual(partPosition(part, 0), [0, 0, 0]);
  assert.deepEqual(partPosition(part, 1), part.explode);
  assert.deepEqual(partPosition(part, 0.5), part.explode.map(value => value / 2));
}
assert.equal(clampExplosion(NaN), 0);
assert.equal(clampExplosion(-1), 0);
assert.equal(clampExplosion(2), 1);
const base = metadata.parts.find(part => part.id === 'base');
const clearance = metadata.parts.find(part => part.category === 'clearance');
assert.ok(clearance, 'The assembly includes at least one explicit clearance reference.');
const visibility = { electronics: true, clearance: false, spool: true };
assert.equal(partVisible(base, visibility, null, new Set()), true);
assert.equal(partVisible(clearance, visibility, null, new Set()), false);
assert.equal(partVisible(base, visibility, 'carrier', new Set()), false);
assert.equal(partVisible(base, visibility, null, new Set(['base'])), false);
for (const pose of ['min', 'max']) {
  const alternate = metadata.parts.find(part => part.pose === pose);
  assert.ok(alternate, `A ${pose} cable reference is defined.`);
  assert.equal(partVisible(alternate, { ...visibility, clearance: true }, null, new Set()), false);
  assert.equal(partVisible(alternate, { ...visibility, clearance: true }, alternate.id, new Set()), true);
  assert.equal(partVisible(alternate, { ...visibility, clearance: true }, base.id, new Set()), false);
}
console.log(`Viewer transforms and visibility passed for ${metadata.parts.length} component groups.`);
