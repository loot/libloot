*********************
Miscellaneous Details
*********************

String Encoding
===============

* All output strings are encoded in UTF-8.
* Metadata files are written encoded in UTF-8.
* Input strings are expected to be encoded in UTF-8.
* Metadata files read are expected to be encoded in UTF-8.
* File paths are case-sensitive if and only if the underlying file system is
  case-sensitive.

Language Codes
==============

All language strings in the API are codes of the form ``ll`` or ``ll_CC``, where
``ll`` is an ISO 639-1 language code and ``CC`` is an ISO 3166 country code. For
example, the default language for metadata message content is English,
identified by the code ``en``, and Brazilian Portuguese is ``pt_BR``.

Errors
======

All errors encountered are thrown as exceptions that inherit from
``std::exception``.

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
if another plugin is also present. The API's
:cpp:func:`loot::DatabaseInterface::LoadLists()` method parses metadata files
into memory, but does not evaluate these conditions, so the loaded metadata may
contain metadata that is invalid for the installed game that the
:cpp:class:`loot::DatabaseInterface` object being operated on was created for.

Caching
=======

All unevaluated metadata is cached between calls to :cpp:func:`loot::DatabaseInterface::LoadLists`.

The results of evaluating metadata conditions are cached between calls to
:cpp:func:`loot::GameInterface::LoadPlugins`,
:cpp:func:`loot::GameInterface::SortPlugins` and
:cpp:func:`loot::DatabaseInterface::GetGeneralMessages`.

Plugin content is cached between calls to
:cpp:func:`loot::GameInterface::LoadPlugins` and
:cpp:func:`loot::GameInterface::SortPlugins`.

Load order is cached between calls to
:cpp:func:`loot::GameInterface::LoadCurrentLoadOrderState`.

Performance
===========

The following may involve filesystem access and reading/parsing or writing of
data from the filesystem:

- Any function that takes a ``std::filesystem::path``
- :cpp:any:`loot::GameInterface::IsValidPlugin()`
- :cpp:any:`loot::GameInterface::LoadPlugins()`
- :cpp:any:`loot::GameInterface::LoadCurrentLoadOrderState()`
- :cpp:any:`loot::GameInterface::SetLoadOrder()`

Evaluating conditions may also involve filesystem read access.

:cpp:any:`loot::GameInterface::SortPlugins()` is expensive, as it involves loading
all the content of all the plugins, apart from the game's main master file, which is skipped as an optimisation (it doesn't depend on anything else and is much bigger than any other plugin, so is unnecessary and slow to load).

:cpp:any:`loot::DatabaseInterface::GetGroupsPath()` involves building a graph of all defined groups and
then using it to search for the shortest path between the two given groups,
which may be relatively slow given a sufficiently large and/or complex set of
group definitions.

All other API functions should be relatively fast.
