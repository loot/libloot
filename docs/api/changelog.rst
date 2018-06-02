***************
Version History
***************

0.13.5 - 2018-06-02
===================

Changed
-------

- Sorting now enforces hardcoded plugin positions, sourcing them through
  libloadorder. This avoids the need for often very verbose metadata entries,
  particularly for Creation Club plugins.
- Updated libgit2 to v0.27.1. This includes a security fix for CVE-2018-11235,
  but LOOT API's usage is not susceptible. libgit2 is not susceptible to
  CVE-2018-11233, another Git vulnerability which was published on the same day.
- Updated libloadorder to v11.3.0.
- Updated spdlog to v0.17.0.
- Updated esplugin to v1.0.10.

0.13.4 - 2018-06-02
===================

Fixed
-----

- :cpp:any:`NewMetadata()` now uses the passed plugin's group if the calling
  plugin's group is implicit, and sets the group to be implicit if the two
  plugins' groups are equal.

0.13.3 - 2018-05-26
===================

Changed
-------

- Improved cycle avoidance when resolving evaluating plugin groups during
  sorting. If enforcing the group difference between two plugins would cause a
  cycle and one of the plugins' groups is the default group, that plugin's group
  will be ignored for all plugins in groups between default and the other
  plugin's group.
- The masterlist repository cloning process no longer moves LOOT's game folders,
  so if something goes wrong the process fails more safely.
- The LOOT API is now built with debugging information on Windows, and its PDB
  is included in build archives.
- Updated libloadorder to v11.2.2.

Fixed
-----

- Various filesystem-related issues that could be encountered when updating
  masterlists, including failure due to file handles being left open while
  attempting to remove.
- Building the esplugin and libloadorder dependencies using Rust 1.26.0, which
  included a `regression`_ to workspace builds.

.. regression: https://github.com/rust-lang/cargo/issues/5518

0.13.2 - 2018-04-29
===================

Changed
-------

- Updated libloadorder to v11.2.1.

Fixed
-----

- Incorrect load order positions were given for light-master-flagged ``.esp``
  plugins when getting the load order (via libloadorder).

0.13.1 - 2018-04-09
===================

Added
-----

- Support for Skyrim VR using ``GameType::tes5vr``.

Changed
-------

- Updated libloadorder to v11.2.0.

0.13.0 - 2018-04-02
===================

Added
-----

- Group metadata as a replacement for priority metadata. Each plugin belongs to
  a group, and a group can load after other groups. Plugins belong to the
  ``default`` group by default.

  - Added the :cpp:any:`loot::Group` class to represent a group.
  - Added :cpp:any:`loot::UndefinedGroupError`.
  - Added :cpp:any:`GetGroups()`, :cpp:any:`GetUserGroups()` and :cpp:any:`SetUserGroups()`.
  - Added :cpp:any:`GetGroup()`, :cpp:any:`IsGroupExplicit()`
    and :cpp:any:`SetGroup()`.
  - Updated :cpp:any:`MergeMetadata()` to replace the existing
    group with the given object's group if the latter is explicit.
  - Updated :cpp:any:`NewMetadata()` to return an object using
    the called object's group.
  - Updated :cpp:any:`HasNameOnly()` to check the group is
    implicit.
  - Updated :cpp:any:`SortPlugins()` to take into account plugin
    groups.

Changed
-------

- :cpp:any:`LoadPlugins()` and
  :cpp:any:`SortPlugins()` no longer load the current load order
  state, so :cpp:any:`LoadCurrentLoadOrderState()` must be called
  separately.
- Updated libgit2 to v0.27.0.
- Updated libloadorder to v11.1.0.

Removed
-------

- Support for local and global plugin priorities.

  - Removed the ``loot::Priority`` class.
  - Removed ``PluginMetadata::GetLocalPriority()``,
    ``PluginMetadata::GetGlobalPriority()``,
    ``PluginMetadata::SetLocalPriority()`` and
    ``PluginMetadata::SetGlobalPriority()``
  - Priorities are no longer taken into account when sorting plugins.

Fixed
-----

- An error when applying a load order for Morrowind, Oblivion, Fallout 3 or
  Fallout: New Vegas when a plugin had a timestamp earlier than 1970-01-01
  00:00:00 UTC (via libloadorder).
- An error when loading the current load order for Skyrim with a
  ``loadorder.txt`` incorrectly encoded in Windows-1252 (via libloadorder).


0.12.5 - 2018-02-17
===================

Changed
-------

- Updated esplugin to v1.0.9.
- Updated libgit2 to v0.26.3. This enables TLS 1.2 support on Windows 7, so
  users shouldn't need to manually enable it themselves.

0.12.4 - 2018-02-17
===================

Fixed
-----

- Loading or saving a load order could be very slow because the plugins
  directory was scanned recursively, which is unnecessary. In the reported case,
  this fix caused saving a load order to go from 23 seconds to 43 milliseconds
  (via libloadorder).
- Plugin parsing errors were being logged with trace severity, they are now
  logged as errors.
- Saving a load order for Oblivion, Fallout 3 or Fallout: New Vegas now updates
  plugin access times to the current time for correctness (via libloadorder).

Changed
-------

- ``GameInterface::SetLoadOrder()`` now errors if passed a load order that does
  not contain all installed plugins. The previous behaviour was to append any
  missing plugins, but this was undefined and could cause unexpected results
  (via libloadorder).
- Performance improvements for load order operations, benchmarked at 2x to 150x
  faster (via libloadorder).
- Updated mentions of libespm in error messages to mention esplugin instead.
- Updated libloadorder to v11.0.1.
- Updated spdlog to v0.16.3.

0.12.3 - 2018-02-04
===================

Added
-----

- Support for Fallout 4 VR via the new :cpp:any:`loot::GameType::fo4vr` game type.

Fixed
-----

- :cpp:any:`loot::CreateGameHandle()` no longer accepts an empty game path
  string, and no longer has a default value for its game path parameter, as
  using an empty string as the game path is invalid and always causes an
  exception to be thrown.

Changed
-------

- Added an empty string as the default value of
  :cpp:any:`loot::InitialiseLocale()`'s string parameter.
- Updated esplugin to v1.0.8.
- Updated libloadorder to v10.1.0.

0.12.2 - 2017-12-24
===================

Fixed
-----

- Plugins with a ``.esp`` file extension that have the light master flag set are
  no longer treated as masters when sorting, so they can have other ``.esp``
  files as masters without causing cyclic interaction sorting errors.

Changed
-------

- Downgraded Boost to 1.63.0 to take advantage of pre-built binaries on AppVeyor.

0.12.1 - 2017-11-23
===================

Added
-----

- Support for identifying Creation Club plugins using ``Skyrim.ccc`` and ``Fallout4.ccc`` (via libloadorder).

Changed
-------

- Update esplugin to v1.0.7.
- Update libloadorder to v10.0.4.

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
