************************
LOOT's Sorting Algorithm
************************

LOOT's sorting algorithm consists of the following stages:

.. contents::
  :local:

Load plugin data
================

In this first stage, the plugins to be sorted are parsed and their record IDs
(which are FormIDs for all games apart from Morrowind) are stored. When parsing
plugins, all subrecords are skipped over for efficiency, apart from the
subrecords of the ``TES4`` header record, and some Morrowind plugin subrecords.

Due to how their record IDs work, Morrowind and Starfield plugins can only have
their record IDs properly understood if all of their masters can also be parsed,
so sorting will fail at this point if any Morrowind or Starfield plugin has a
master that is not installed.

Loading plugin data also involves loading any metadata that the plugin may have
in the masterlist and userlist.

Create plugin graph vertices
============================

Once the plugins have been loaded, they are sorted into lexicographical order.
This ensures that the sorting process operates on the plugins in the same order
every time, so that it gives consistent results.

After that, three graphs are created, and the plugins are added to them as
vertices in their sorted order:

- One graph contains plugins that are not masters (meaning master plugins in
  their own right, not that they are listed as a master within another plugin's
  header).
- One graph contains plugins that are blueprint masters: blueprint masters are
  a type of plugin specific to Starfield, so for all other games this graph will
  be empty.
- One graph contains plugins that are non-blueprint masters.

Three graphs are used because blueprint master plugins always load after all
other plugins and non-masters always load after non-blueprint masters (not
quite, but LOOT doesn't currently support hoisting non-masters), and it's
much more efficient to sort them separately and then combine their load orders
than to enforce those relationships within a single graph.

A consequence of using three separate graphs is that any plugin data or metadata
that involves a pair of plugins that go in different graphs will be silently
ignored. For example: if plugin A is a master and plugin B is not, and
plugin A has metadata saying it must load after plugin B, then that metadata
will be ignored because the two plugins are sorted independently, as if the
other plugin is not installed.

To help catch invalid metadata, there's also a validation step that checks for
things like requirement metadata that tries to load a blueprint master before
another plugin, and sorting will fail if any such invalid metadata is found.

A fourth graph is created to represent which groups must load after
which other groups, with groups being added in lexicographical order and
masterlist groups before userlist groups.

Create plugin graph edges
=========================

The steps described in this section are run on all graphs independently.

In this section, the terms *vertex* and *plugin* are used interchangeably, and
the iteration order 'for each plugin' is the order in which the vertices were
added to the graph.

For each plugin:

1. If the plugin is a master file, add edges going to all non-master files. If
   the plugin is a non-master file, add edges coming from all master files. This
   shouldn't result in any edges being added, since masters and non-masters are
   sorted in separate graphs, but is done for completeness.
2. Add edges coming from all the plugin's masters. Missing masters have no edges
   added.
3. Add edges coming from all the plugin's requirements. Missing requirements
   have no edges added.
4. Add edges coming from all the plugin's load after files that are installed
   plugins.

Hardcoded edges
---------------

Some games hardcode certain plugins to load in certain positions, and this
section adds edges in the correct order between those plugins, and between those
plugins and the rest of the plugins in the graph.

At this point the plugin graph is checked for cycles, and an error is thrown if
any are encountered, so that metadata (or indeed plugin data) that cause them
can be corrected.

Group edges
-----------

A depth-first search of the groups graph is performed for each group in the
graph, in the order that they were added to the graph, except that root vertices
go first, in descending order of their longest path length.

As the groups graph is searched, a stack of edges is used to record the current
path through the graph from the starting group. On each new edge encountered, it
is added to the stack and plugin graph edges are added going from all the
plugins in the current path's groups to the current edge's target group, except
when that would cause a cycle, and for plugins in the ``default`` group, which
are ignored. Once the graph beyond an edge's target group has been fully
explored, that edge is removed from the stack.

Once all the groups have been iterated over, one final depth-first search is
performed, this time starting from the ``default`` group and *not* skipping
edges from its plugins.

In this way all plugins have edges added from them to all the plugins in the
groups that load after their group, unless the edge would cause a cycle.

Overlap edges
-------------

Plugin overlap edges are then added. Two plugins overlap if they contain the
same record, i.e. if they both edit the same record or if one edits a record the
other plugin adds. Plugins also overlap if they both load one or more BSAs (BA2s
for Fallout 4) and the BSAs loaded by one plugin contain data for a file path
that is also included in the BSAs loaded by the other plugin.

For each plugin, skip it if it overrides no records, otherwise iterate over all
other plugins.

* If the plugin and other plugin override the same number of records and the
  same number of assets, or do not overlap, skip the other plugin.
* Otherwise, add an edge from the plugin which overrides more records to the
  plugin that overrides fewer records, unless that edge would cause a cycle. If
  the plugins don't have overlapping records or override the same number of
  records, the edge is added from the plugin that loads more assets via its
  BSAs to the plugin that loads fewer assets.

For Morrowind, identifying which records override others requires all of a
plugin's masters to be installed, so if a plugin has missing masters, its total
record count is used in place of its override record count. Morrowind plugins
also can't load BSAs, so they can't have overlapping assets.

Tie-break edges
---------------

Finally, tie-break edges are added to ensure that sorting is consistent. The
graph's vertices are sorted into their current load order:

* If both plugins have positions in the current load order, the function
  preserves their existing relative order.
* If one plugin has a position and the other does not, the plugin with a
  position goes before the plugin without a position.
* If neither plugin has a load order position, a case-insensitive
  lexicographical comparison of their filenames without file extensions is used
  to decide their order. If they are equal, a case-insensitive lexicographical
  comparison of their file extensions is used.

Once sorted, they are iterated over. Each loop looks at the current vertex and
the next one following it (e.g. the first iteration is for vertices 0 and 1, the
second is for 1 and 2, etc.).

For each (``current``, ``next``) pair of vertices, try to find a path from
``next`` to ``current``.

If sorting makes no changes, then there won't be any paths found and it'll
therefore be possible to add an edge from ``current`` to ``next`` without
causing a cycle, producing the old load order.

If no path is found then that means the old load order can be used for those two
plugins. If the ``current`` vertex has not already been processed (which will be
the case unless it appeared in a path found earlier and had its position pinned,
see below), append it to a list representing the new load order and record the
vertex as having been processed.

If no path is found but the ``current`` vertex has been processed and is not the
last vertex in the new load order list, pin the position of the ``next`` vertex
(see below).

If a path is found then that means the old load order for those two plugins
(which is ``current`` before ``next``) can't be used. If ``current`` is the
first vertex in the iteration order, then ``next`` is simply treated as the
start of the new load order. If ``current`` is not the first vertex,
iterate over the vertices in the path found, going from ``next`` to ``current``,
and pin each vertex's position.

Pinning vertex positions
^^^^^^^^^^^^^^^^^^^^^^^^

A vertex's position needs to be pinned when it must go somewhere before the last
plugin in the new load order list, because although it has a fixed position
relative to that last plugin, it doesn't necessarily have a fixed position
relative to the plugins that come before the last plugin. I.e. it needs to load
earlier, but how much earlier?

To pin a vertex's position, iterate over the new load order list in reverse
order, going from the last vertex towards the first, and stop at the first
load order vertex for which there is no path going from the unpinned vertex to
the load order vertex. This is equivalent to finding the last plugin that the
unpinned vertex's plugin can load after (which is not necessarily the same as
the last plugin it *must* load after).

If such a load order vertex is found, add an edge going from it to the unpinned
vertex. If the found vertex is not the last vertex in the load order list, also
add an edge going from the unpinned vertex to the vertex after the found vertex.
Then record the unpinned vertex's new position in the new load order list: the
vertex is now pinned.

Topologically sort the plugin graphs
====================================

This is done for all graphs independently.

Note that edges for explicit interdependencies are the only edges allowed to
create cycles. However, the graph is again checked for cycles to guard against
potential logic bugs, and if a cycle is encountered an error is thrown.

Once the graph is confirmed to be cycle-free, a topological sort is performed on
the graph, outputting a list of plugins in their newly-sorted load order.

Combine the load orders
=======================

Finally, the sorted load orders are combined in this order:

1. master plugins
2. non-master plugins
3. blueprint master plugins

That gives the complete sorted load order.
