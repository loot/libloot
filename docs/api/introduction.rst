************
Introduction
************

LOOT is a utility that helps users avoid serious conflicts between their mods by
setting their plugins in an optimal load order. It also provides tens of
thousands of plugin-specific messages, including usage notes, requirements,
incompatibilities, bug warnings and installation mistake notifications, and
thousands of Bash Tag suggestions.

This metadata that LOOT supplies is stored in its masterlist, which is
maintained by the LOOT team using information provided by mod authors and users.
Users can also add to and modify the metadata used by LOOT through the use of
userlist files. libloot provides all of LOOT's non-UI-related functionality, and
can be used by third-party developers to access this metadata for use in their
own programs.

Metadata Files
==============

LOOT stores plugin metadata in YAML files. It distinguishes between three types
of metadata file:

- *masterlist* files: each game has a single masterlist, which is a public,
  curated metadata store
- *masterlist prelude* files: there is a single masterlist prelude, which is a
  public store of common metadata templates that can be shared across all
  masterlists
- *userlist* files: each game has a userlist, which is a private user-specific
  metadata store containing metadata added by the LOOT user.

All three files use the same syntax, but the masterlist prelude file is used to
replace part of a masterlist file before it is parsed, and metadata in the
userlist extends or replaces metadata sourced from the masterlist.

LOOT's plugin metadata can be conditional, eg. a plugin may require a patch only
if another plugin is also present. Conditions are not evaluated when metadata is
loaded by libloot: instead, they can be optionally evaluated when accessing the
loaded metadata.
