************************
LOOT's Sorting Algorithm
************************

LOOT's sorting algorithm consists of four stages:

.. contents::
  :local:

Load plugin data
================

In this first stage, the plugins to be sorted are parsed and their record IDs
(which are FormIDs for all games apart from Morrowind) are stored. Parsing is
multithreaded by dividing the plugins into buckets with roughly equal total file
sizes, and loading each bucket's plugins in a separate thread. The number of
buckets created is equal to the number of concurrent threads that are
hardware-supported (e.g. a dual-core CPU without hyperthreading may report that
it supports two threads).

When parsing plugins, all subrecords are skipped over for efficiency, apart from
the subrecords of the ``TES4`` header record.

Create plugin graph vertices
=================================

Once loaded, a directed graph is created and the plugins are added to it in
lexicographical order as vertices. Any metadata a plugin has in the masterlist
and userlist are then merged into its vertex's data store. Plugin group
dependencies are also resolved and added as group-derived plugins.

Create plugin graph edges
==============================

In this section, the terms *vertex* and *plugin* are used interchangeably, and
the iteration order 'for each plugin' is the order in which the vertices were
added to the graph.

For each plugin:

1. If the plugin is a master file, add edges going to all non-master files. If
   the plugin is a non-master file, add edges coming from all master files.
2. Add edges coming from all the plugin's masters. Missing masters have no edges
   added.
3. Add edges coming from all the plugin's requirements. Missing requirements
   have no edges added.
4. Add edges coming from all the plugin's load after files that are installed
   plugins.

Group-derived interdependencies are then evaluated. Each plugin's group-derived
plugins are iterated over and individually checked to see if adding an edge from
the group-derived plugin to the plugin would cause a cycle, and if not the edge
is recorded. Once all potential edges have been checked, the recorded edges are
added to the graph.

Plugin overlap edges are then added. Two plugins overlap if they contain the
same record, i.e. if they both edit the same record or if one edits a record the
other plugin adds.

For each plugin, skip it if it overrides no records, otherwise iterate over all
other plugins.

* If the plugin and other plugin override the same number of records, or do not
  overlap, skip the other plugin.
* Otherwise, add an edge from the plugin which overrides more records to the
  plugin that overrides fewer records, unless that edge would cause a cycle.

For Morrowind, identifying which records override others requires all of a
plugin's masters to be installed, so if a plugin has missing masters, its total
record count is used in place of its override record count.

Finally, tie-break edges are added to ensure that sorting is consistent. For
each plugin, iterate over all other plugins and add an edge between each pair of
plugins in the direction given by the tie-break comparison function, unless that
edge would cause a cycle.

The tie-break comparison function compares current plugin load order positions,
falling back to plugin names.

* If both plugins have positions in the current load order, the function
  preserves their existing relative order.
* If one plugin has a position and the other does not, the edge added goes from
  the plugin with a position to the plugin without a position.
* If neither plugin has a load order position, a case-insensitive
  lexicographical comparison of their filenames without file extensions is used
  to decide their order.

Topologically sort the plugin graph
===================================

Note that edges for explicit interdependencies are the only edges allowed to
create cycles: this is because the first step of this stage is to check the
plugin graph for cycles, and throw an error if any are encountered, so that
metadata (or indeed plugin data) that cause them can be corrected.

Once the graph is confirmed to be cycle-free, a topological sort is performed on
the graph, outputting a list of plugins in their newly-sorted load order.
