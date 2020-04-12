Plugin
======

This is the structure that brings all the others together, and forms the main component of a metadata file. It is a key-value map.

.. describe:: name

  ``string``

  **Required.** Can be an exact plugin filename or a regular expression plugin filename. If the filename contains any of the characters ``:\*?|``, the string will be treated as a regular expression, otherwise it will be treated as an exact filename. For example, ``Example\.esm`` will be treated as a regular expression, as it contains a ``\`` character.

  Regular expression plugin filenames must be written in `modified ECMAScript <https://en.cppreference.com/w/cpp/regex/ecmascript>`_ syntax.

.. describe:: group

  ``string``

  The name of the group the plugin belongs to. If unspecified, defaults to ``default``.

  The named group must be exist when LOOT sorts plugins, but doesn't need to
  be defined in the same metadata file. If at sort time the group does not
  exist, a sorting error will occur.

  The plugin must load after all the plugins in the groups its group is defined
  to load after, resolving them recursively. An exception exists if doing so
  would introduce a cyclic dependency between two plugins without any other
  group loading rules applied.

  For example, if for plugins A.esp, B.esp, C.esp and D.esp:

  - B.esp has A.esp as a master
  - A.esp is in group A
  - B.esp and C.esp are in the default group
  - D.esp is in group D
  - group A loads after the default group
  - the default group loads after group D

  Then the load order must be D.esp, C.esp, A.esp, B.esp. Although A.esp's group
  must load after B.esp's group, this would cause a cycle between A.esp and
  B.esp, so the requirement is ignored for that pair of plugins.

  However, if for plugins A.esp, B.esp and C.esp in groups of the same names:

  1. group B loads after group A
  2. group C loads after group B
  3. A.esp has C.esp as a master

  This will cause a sorting error, as neither group rule introduces a cyclic
  dependency when combined in isolation with the third rule, but having all
  three rules applied causes a cycle.

.. describe:: after

  ``file set``

  Plugins that this plugin must load after, but which are not dependencies. Used to resolve specific compatibility issues. If undefined, the set is empty.

.. describe:: req

  ``file set``

  Files that this plugin requires to be present. This plugin will load after any plugins listed. If any of these files are missing, an error message will be displayed. Intended for use specifying implicit dependencies, as LOOT will detect a plugin's explicit masters itself. If undefined, the set is empty.

.. describe:: inc

  ``file set``

  Files that this plugin is incompatible with. If any of these files are present, an error message will be displayed. If undefined, the set is empty.

.. describe:: msg

  ``message list``

  The messages attached to this plugin. The messages will be displayed in the order that they are listed. If undefined, the list is empty.

.. describe:: tag

  ``tag set``

  Bash Tags suggested for this plugin. If a Bash Tag is suggested for both addition and removal, the latter will override the former when the list is evaluated. If undefined, the set is empty.

.. describe:: url

  ``location set``

  An unordered set of locations for this plugin. If the same version can be found at multiple locations, only one location should be recorded. If undefined, the set is empty. This metadata is not currently used by LOOT.

.. describe:: dirty

  ``cleaning data set``

  An unordered set of cleaning data structures for this plugin, identifying dirty plugins.

.. describe:: clean

  ``cleaning data set``

  An unordered set of cleaning data structures for this plugin, identifying clean plugins. The ``itm``, ``udr`` and ``nav`` fields are unused in this context, as they're assumed to be zero.

Equality
--------

The equality of two plugin data structures is determined by comparing the values of their ``name`` keys.

* If neither or both values are regular expressions, then the plugin data structures are equal if the lowercased values are identical.
* If one value is a regular expression, then the plugin data structures are equal if the other value is an exact match for it.

.. _plugin-merging:

Merging Behaviour
-----------------

===============   ==================================
Key               Merge Behaviour (merging B into A)
===============   ==================================
name              Not merged.
group             Replaced by B's value only if A has no value set.
after             Merged. If B's file set contains an item that is equal to one already present in A's file set, B's item is discarded.
req               Merged. If B's file set contains an item that is equal to one already present in A's file set, B's item is discarded.
inc               Merged. If B's file set contains an item that is equal to one already present in A's file set, B's item is discarded.
msg               Merged. B's message list is appended to A's message list.
tag               Merged.If B's tag set contains an item that is equal to one already present in A's tag set, B's item is discarded.
url               Merged. If B's location set contains an item that is equal to one already present in A's location set, B's item is discarded.
dirty             Merged.If B's dirty data set contain an item that is equal to one already present in A's dirty data set, B's item is discarded.
clean             Merged. If B's clean data set contain an item that is equal to one already present in A's clean data set, B's item is discarded.
===============   ==================================

Examples
--------

.. code-block:: yaml

  name: 'Oscuro''s_Oblivion_Overhaul.esm'
  req:
    - 'Oblivion.esm'  # Don't do this, Oblivion.esm is a master of Oscuro's_Oblivion_Overhaul.esm, so LOOT already knows it's required.
    - name: 'example.esp'
      display: '[Example Mod](http://www.example.com)'
      condition: 'version("Oscuro''s_Oblivion_Overhaul.esm", "15.0", ==)'
  tag:
    - Actors.Spells
    - Graphics
    - Invent
    - Relations
    - Scripts
    - Stats
    - name: -Relations
      condition: 'file("Mart''s Monster Mod for OOO.esm") or file("FCOM_Convergence.esm")'
  msg:
    - type: say
      content: 'Do not clean. "Dirty" edits are intentional and required for the mod to function.'
