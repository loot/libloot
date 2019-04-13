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

All language strings in the API are codes of the form ``ll`` or ``ll_CC``, where ``ll`` is an ISO 639-1 language code and ``CC`` is an ISO 3166 country code. For example, the default language for metadata message content is English, identified by the code ``en``, and Brazilian Portuguese is ``pt_BR``.

Errors
======

All errors encountered are thrown as exceptions that inherit from
``std::exception``.

Metadata Files
==============

LOOT stores plugin metadata in YAML files. It distinguishes between *masterlist*
and *userlist* files: each game has a single masterlist, which is a public,
curated metadata store, and each LOOT user has a private userlist, which can
contain metadata added by the user. The two files use the same syntax, but
metadata in the userlist extends or replaces metadata sourced from the
masterlist.

LOOT's plugin metadata can be conditional, eg. a plugin may require a patch only
if another plugin is also present. The API's :cpp:func:`LoadLists` method parses
metadata files into memory, but does not evaluate these conditions, so the
loaded metadata may contain metadata that is invalid for the installed game that
the :cpp:class:`loot::DatabaseInterface` object being operated on was created for.

Caching
=======

All unevaluated metadata is cached between calls to :cpp:func:`LoadLists`.

Plugin content is cached between calls to :cpp:func:`LoadPlugins` and
:cpp:func:`SortPlugins`.

Load order is cached between calls to :cpp:func:`LoadPlugins`,
:cpp:func:`SortPlugins` and :cpp:func:`LoadCurrentLoadOrderState`.

Performance
===========

Loading metadata lists is a relatively costly operation, as is updating the
masterlist (which involves loading it).

Sorting plugins is expensive, as it involves loading all the content of all
the plugins, apart from the game's main master file, which is skipped as an
optimisation (it doesn't depend on anything else and is much bigger than any
other plugin, so is unnecessary and slow to load).

Getting plugin metadata once loaded is cheap, as is getting a masterlist's
revision.

Loading the current load order state is relatively cheap and can take < 1 ms
depending on hardware and the size of the load order, but involves filesystem
access and should not be done more often than necessary to avoid a performance
impact.
