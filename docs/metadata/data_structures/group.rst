Group
=====

Groups represent sets of plugins, and are a way to concisely and extensibly
load sets of plugins after other sets of plugins.

This structure can be used to hold group definitions. It is a key-value map.

.. describe:: name

  ``string``

  **Required.** A case-sensitive name that identifies the group.

.. describe:: after

  ``string set``

  The names of groups that this group loads after. Group names are
  case-sensitive. If undefined, the set is empty. The named groups must be
  defined when LOOT sorts plugins, but they don't need to be defined in the same
  metadata file. If at sort time a group is defined to load after a group that
  does not exist, a sorting error will occur.

Merging Groups
--------------

When a group definition for an already-defined group is encountered, the
``after`` sets of the two definitions are merged.

The ``default`` Group
---------------------

There is one predefined group named ``default`` that all plugins belong to by
default. It is defined with an empty ``after`` set, as no other predefined
groups exist for it to load after.

Like any other group, the ``default`` group can be redefined to add group names
to its ``after`` set.

Equality
--------

Two group data structures are equal if the values of their ``name`` keys are identical.

Examples
--------

.. code-block:: yaml

  # Create a group for map marker plugins that loads after the predefined
  # 'default' group.
  name: 'Map Markers'
  after:
    - 'default'

.. code-block:: yaml

  # Extend the predefined 'default' group to load after an 'Unofficial Patches'
  # group that is defined elsewhere.
  name: 'default'
  after:
    - 'Unofficial Patches'
