import assert from 'node:assert';
import { test } from 'node:test';

import { liblootVersion, isCompatible, Group } from '../index.js';

test('liblootVersion', t => {
  assert.strictEqual(liblootVersion(), process.env.npm_package_version);
});

test('isCompatible', t => {
  assert.strictEqual(isCompatible(0, 29, 0), true);
});

test('Group equality', t => {
  const group1 = new Group("A");
  const group2 = new Group("A");
  const group3 = new Group("B");

  assert.deepStrictEqual(group1.name, group2.name);
  assert.notDeepStrictEqual(group1.name, group3.name);

  assert.deepStrictEqual(group1, group2);

  // The objects have no public fields so appear to be equal.
  assert.deepStrictEqual(group1, group3);
})
