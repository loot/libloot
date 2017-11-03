***************
Version History
***************

0.12.0 - 2017-11-03
===================

Added
-----

- Support for light master (``.esl``) plugins.
- :cpp:any:`LoadCurrentLoadOrderState()` in :cpp:any:`loot::GameInterface` to
  expose load order cache management to clients, as libloadorder no longer
  internally manages it.
- :cpp:any:`loot::SetLoggingCallback()` to allow clients to handle the LOOT
  API's logging statements themselves.
- Logging of libloadorder error details.

Changed
-------

- :cpp:any:`LoadPlugins()` now loads the current load order
  state before loading plugins.
- Added a `condition` string field to :cpp:any:`SimpleMessage`.
- Replaced libespm dependency with esplugin v1.0.6. This significantly improves
  safety and sorting performance, especially for large load orders.
- Updated libloadorder to v10.0.3. This significantly improves safety and the
  performance of load order operations, at the expense of exposing cache
  management to the client.
- Replaced Boost.Log with spdlog v0.14.0, removing dependencies on several other
  Boost libraries in the process.
- Updated libgit2 to v0.26.0.
- Update Boost to v1.65.1.

Removed
-------

- ``DatabaseInterface::EvalLists()`` as it was superseded in v0.11.0 by the
  ability to evaluate conditions when getting general messages and individual
  plugins' metadata, which is more efficient.
- ``SetLoggingVerbosity()`` and ``SetLogFile()`` as they have been superseded
  by the new :cpp:any:`loot::SetLoggingCallback()` function.
- The ``loot/yaml/*`` headers containing LOOT's internal YAML conversion
  functions are no longer exposed alongside the API headers.
- The ``loot/windows_encoding_converters.h`` header is no longer exposed
  alongside the API headers.

Fixed
-----

- Formatting in metadata documentation.
- Saving metadata wrote entries in an inconsistent order.
- Clang build errors.

0.11.1 - 2017-06-19
===================

Fixed
-----

- A crash would occur when loading an plugin that had invalid data past its
  header. Such plugins are now just silently ignored.
- :cpp:any:`loot::CreateGameHandle()` would not resolve game or local data paths
  that are junction links correctly, which caused problems later when trying to
  perform actions such as loading plugins.
- Performing a masterlist update on a branch where the remote and local
  histories had diverged would fail. The existing local branch is now discarded
  and the remote branch checked out anew, as intended.

0.11.0 - 2017-05-13
===================

Added
-----

- New functions to :cpp:class:`loot::DatabaseInterface`:

  - :cpp:any:`WriteUserMetadata()`
  - :cpp:any:`GetKnownBashTags()`
  - :cpp:any:`GetGeneralMessages()`
  - :cpp:any:`GetPluginMetadata()`
  - :cpp:any:`GetPluginUserMetadata()`
  - :cpp:any:`SetPluginUserMetadata()`
  - :cpp:any:`DiscardPluginUserMetadata()`
  - :cpp:any:`DiscardAllUserMetadata()`
  - :cpp:any:`IsLatestMasterlist()`

- A :cpp:any:`loot::GameInterface` pure abstract class that exposes methods for
  accessing game-specific functionality.
- A :cpp:any:`loot::PluginInterface` pure abstract class that exposes methods
  for accessing plugin file data.
- The :cpp:any:`loot::SetLoggingVerbosity()` and :cpp:any:`loot::SetLogFile()`
  functions and :cpp:any:`loot::LogVerbosity` enum for controlling the API's
  logging behaviour.
- An :cpp:any:`loot::InitialiseLocale()` function that must be called to
  configure the API's locale before any of its other functionality is used.
- LOOT's internal metadata classes are now exposed as part of the API.

Changed
-------

- Renamed ``loot::CreateDatabase()`` to :cpp:any:`loot::CreateGameHandle()`, and
  changed its signature so that it returns a shared pointer to a
  :cpp:any:`loot::GameInterface` instead of a shared pointer to a
  :cpp:any:`loot::DatabaseInterface`.
- Moved :cpp:any:`SortPlugins()` into :cpp:any:`loot::GameInterface`.
- Some :cpp:any:`loot::DatabaseInterface` methods are now const:

  - :cpp:any:`WriteMinimalList()`
  - :cpp:any:`GetMasterlistRevision()`

- LOOT's internal YAML conversion functions have been refactored into the
  ``include/loot/yaml`` directory, but they are not really part of the API.
  They're only exposed so that they can be shared between the API and LOOT
  application without introducing another component.
- LOOT's internal string encoding conversion functions have been refactored into
  the ``include/loot/windows_encoding_converters.h`` header, but are not really
  part of the API. They're only exposed so that they can be shared between the
  API and LOOT application without introducing another component.
- Metadata is now cached more efficiently, reducing the API's memory footprint.
- Log timestamps now have microsecond precision.
- Updated to libgit2 v0.25.1.
- Refactored code only useful to the LOOT application out of the API internals
  and into the application source code.

Removed
-------

- ``DatabaseInterface::GetPluginTags()``,
  ``DatabaseInterface::GetPluginMessages()`` and
  ``DatabaseInterface::GetPluginCleanliness()`` have been removed as they have
  been superseded by ``DatabaseInterface::GetPluginMetadata()``.
- The ``GameDetectionError`` class, as it is no longer thrown by the API.
- The ``PluginTags`` struct, as it is no longer used.
- The ``LanguageCode`` enum, as the API now uses ISO language codes directly
  instead.
- The ``PluginCleanliness`` enum. as it's no longer used. Plugin cleanliness
  should now be checked by getting a plugin's evaluated metadata and checking
  if any dirty info is present. If none is present, the cleanliness is unknown.
  If dirty info is present, check if any of the English info strings contain the
  text "Do not clean": if not, the plugin is dirty.
- The LOOT API no longer caches the load order, as this is already done more
  accurately by libloadorder (which is used internally).

Fixed
-----

- Libgit2 error details were not being logged.
- A FileAccessError was thrown when the masterlist path was an empty string. The
  API now just skips trying to load the masterlist in this case.
- Updating the masterlist did not update the cached metadata, requiring a call
  to :cpp:any:`LoadLists()`.
- The reference documentation was broken due to an incompatibility between
  Sphinx 1.5.x and Breathe 4.4.

0.10.3 - 2017-01-08
===================

Added
-----

- Automated 64-bit API builds.

Changed
-------

- Replaced ``std::invalid_argument`` exceptions thrown during condition evaluation with ``ConditionSyntaxError`` exceptions.
- Improved robustness of error handling when calculating file CRCs.

Fixed
-----

- Documentation was not generated correctly for enums, exceptions and structs exposed by the API.
- Added missing documentation for ``CyclicInteractionError`` methods.

0.10.2 - 2016-12-03
===================

Changed
-------

- Updated libgit2 to 0.24.3.

Fixed
-----

- A crash could occur if some plugins that are hardcoded to always load were missing. Fixed by updating to libloadorder v9.5.4.
- Plugin cleaning metadata with no ``info`` value generated a warning message with no text.


0.10.1 - 2016-11-12
===================

No API changes.

0.10.0 - 2016-11-06
===================

Added
-----

* Support for TES V: Skyrim Special Edition.

Changed
-------

* Completely rewrote the API as a C++ API. The C API has been reimplemented as
  a wrapper around the C++ API, and can be found in a `separate repository`_.
* Windows builds now have a runtime dependency on the MSVC 2015 runtime
  redistributable.
* Rewrote the API documentation, which is now hosted online at `Read The Docs`_.
* The Windows release archive includes the ``.lib`` file for compile-time linking.
* LOOT now supports v0.10 of the metadata syntax. This breaks compatibility with existing syntax. See :doc:`the syntax version history <../metadata/changelog>` for the details.
* Updated libgit2 to 0.24.2.

Removed
-------

* The ``loot_get_tag_map()`` function has no equivalent in the new C++ API as it
  is obsolete.
* The ``loot_apply_load_order()`` function has no equivalent in the new C++ API
  as it just passed through to libloadorder, which clients can use directly
  instead.

Fixed
-----

* Database creation was failing when passing paths to symlinks that point to
  the game and/or game local paths.
* Cached plugin CRCs causing checksum conditions to always evaluate to false.
* Updating the masterlist when the user's ``TEMP`` and ``TMP`` environmental variables point to a different drive than the one LOOT is installed on.

.. _separate repository: https://github.com/loot/loot-api-c
.. _Read The Docs: https://loot.readthedocs.io

0.9.2 - 2016-08-03
==================

Changed
-------

* libespm (2.5.5) and Pseudosem (1.1.0) dependencies have been updated to the
  versions given in brackets.

Fixed
-----

* The packaging script used to create API archives was packaging the wrong
  binary, which caused the v0.9.0 and v0.9.1 API releases to actually be
  re-releases of a snapshot build made at some point between v0.8.1 and v0.9.0:
  the affected API releases were taken offline once this was discovered.
* ``loot_get_plugin_tags()`` remembering results and including them in the
  results of subsequent calls.
* An error occurred when the user's temporary files directory didn't
  exist and updating the masterlist tried to create a directory there.
* Errors when reading some Oblivion plugins during sorting, including
  the official DLC.

0.9.1 - 2016-06-23
==================

No API changes.

0.9.0 - 2016-05-21
==================

Changed
-------

* Moved API header location to the more standard ``include/loot/api.h``.
* Documented LOOT's masterlist versioning system.
* Made all API outputs fully const to make it clear they should not be
  modified and to avoid internal const casting.
* The ``loot_db`` type is now an opaque struct, and functions that used to take
  it as a value now take a pointer to it.

Removed
-------

* The ``loot_cleanup()`` function, as the one string it used to destroy
  is now stored on the stack and so destroyed when the API is unloaded.
* The ``loot_lang_any`` constant. The ``loot_lang_english`` constant
  should be used instead.

0.8.1 - 2015-09-27
==================

Changed
-------

* Safety checks are now performed on file paths when parsing conditions (paths
  must not reference a location outside the game folder).
* Updated Boost (1.59.0), libgit2 (0.23.2) and CEF (branch 2454) dependencies.

Fixed
-----

* A crash when loading plugins due to lack of thread safety.
* The masterlist updater and validator not checking for valid condition
  and regex syntax.
* The masterlist updater not working correctly on Windows Vista.

0.8.0 - 2015-07-22
==================

Added
-----

* Support for metadata syntax v0.8.

Changed
-------

* Improved plugin loading performance for computers with weaker multithreading
  capabilities (eg. non-hyperthreaded dual-core or single-core CPUs).
* LOOT no longer outputs validity warnings for inactive plugins.
* Updated libgit2 to v0.23.0.

Fixed
-----

* Many miscellaneous bugs, including initialisation crashes and
  incorrect metadata input/output handling.
* LOOT silently discarding some non-unique metadata: an error will now
  occur when loading or attempting to apply such metadata.
* LOOT's version comparison behaviour for a wide variety of version string
  formats.

0.7.1 - 2015-06-22
==================

Fixed
-----

* "No existing load order position" errors when sorting.
* Output of Bash Tag removal suggestions in ``loot_write_minimal_list()``.

0.7.0 - 2015-05-20
==================

Initial API release.
