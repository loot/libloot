import test from 'ava'

import { liblootVersion, isCompatible, Group } from '../index.js'

test('liblootVersion', t => {
  t.is(liblootVersion(), process.env.npm_package_version)
});

test('isCompatible', t => {
  t.is(isCompatible(0, 26, 0), true)
});

test('Group equality', t => {
  const group1 = new Group("A");
  const group2 = new Group("A");
  const group3 = new Group("B");

  t.deepEqual(group1.name, group2.name);
  t.notDeepEqual(group1.name, group3.name);

  t.deepEqual(group1, group2);

  // The objects have no public fields so appear to be equal.
  t.deepEqual(group1, group3);
})
