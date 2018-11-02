***************
Version History
***************

0.14.0 - Unreleased
===================

Added
-----

- :cpp:any:`GetHeaderVersion()` to get the value of the version field in the
  ``HEDR`` subrecord of a plugin's ``TES4`` record.
- :cpp:any:`IsValidAsLightMaster()` to check if a light master is valid or if a
  non-light-master plugin would be valid with the light master flag or ``.esl``
  extension. Validity is defined as having no new records with a FormID object
  index greater than 0xFFF.
- :cpp:any:`GetGroupsPath()` to return the path between two given groups that
  maximises the user metadata and minimises the masterlist metadata involved.
- :cpp:any:`loot::Vertex` to represent a plugin or group vertex in a sorting
  graph path.
- :cpp:any:`loot::EdgeType` to represent the type of the edge between two vertices
  in a sorting graph. Each edge type indicates the type of data it was sourced
  from.

Changed
-------

- Renamed the library from "the LOOT API" to "libloot" to avoid confusion
  between the name of the library and the API that it provides. The library
  filename is changed so that the ``loot_api`` part is now ``loot``, e.g.
  ``loot.dll`` on Windows and ``libloot.so`` on Linux.
- :cpp:any:`CyclicInteractionError` has had its constructor and methods
  completely replaced to provide a more detailed and flexible representation of
  the cyclic path that it reports.
- ``UndefinedGroupError::getGroupName()`` has been renamed to ``UndefinedGroupError::GetGroupName()`` for consistency with other API method names.
- ``LootVersion::string()`` has been renamed to ``LootVersion::GetVersionString()`` for consistency with other API method names.
- :cpp:any:`GetPluginMetadata()` and :cpp:any:`GetPluginUserMetadata()` now
  return ``std::optional<PluginMetadata>`` to differentiate metadata being found
  or not. Note that the ``PluginMetadata`` value may still return true for
  :cpp:any:`HasNameOnly()` if a metadata entry exists but has no content other
  than the plugin name.
- :cpp:any:`GetGroup()` now returns ``std::optional<std::string>`` to
  indicate when there is no group metadata explicitly set, to simplify
  distinguishing between explicit and implicit default group membership.
- :cpp:any:`GetVersion()` now returns ``std::optional<std::string>`` to
  differentiate between there being no version and the version being an empty
  string, though the latter should never occur.
- :cpp:any:`GetCRC()` now returns ``std::optional<uint32_t>`` to differentiate
  between there being no CRC calculated and the CRC somehow being zero (which
  should never occur).
- Filesystem paths are now represented in the API by ``std::filesystem::path``
  values instead of ``std::string`` values. This affects the following
  functions:

  - :cpp:any:`loot::CreateGameHandle()`
  - :cpp:any:`LoadLists()`
  - :cpp:any:`WriteUserMetadata()`
  - :cpp:any:`WriteMinimalList()`
  - :cpp:any:`UpdateMasterlist()`
  - :cpp:any:`GetMasterlistRevision()`
  - :cpp:any:`IsLatestMasterlist()`

- The metadata condition parsing, evaluation and caching code and the pseudosem
  dependency have been replaced by a dependency on
  `loot-condition-interpreter`_, which provides more granular caching and more
  opportunity for future enhancements.
- The API now supports v0.14 of the metadata syntax.
- Updated C++ version required to C++17. This means that Windows builds
  now require the MSVC 2017 runtime redistributable to be installed.
- Updated esplugin to v2.1.1.
- Updated libloadorder to v12.0.0.
- Updated libgit2 to v0.27.7.
- Updated spdlog to v1.2.1.

.. _loot-condition-interpreter: https://github.com/loot/loot-condition-interpreter

Removed
-------

- ``PluginInterface::GetLowercasedName()``, as the case folding behaviour LOOT
  uses is not necessarily appropriate for all use cases, so it's up to the
  client to lowercase according to their own needs.

Fixed
-----

- BSAs/BA2s loaded by non-ASCII plugins for Oblivion, Fallout 3, Fallout: New
  Vegas and Fallout 4 may not have been detected due to incorrect
  case-insensitivity handling.
- Fixed incorrect case-insensitivity handling for non-ASCII plugin filenames and
  ``File`` metadata names.
- ``FileVersion`` and ``ProductVersion`` properties were not set in the DLL
  since v0.11.0.
- Path equivalence checks could be inaccurate as they were using case-insensitive
  string comparisons, which may not match filesystem behaviour. Filesystem
  equivalence checks are now used to improve correctness.
- Errors due to filesystem permissions when cloning a new masterlist repository
  into an existing game directory. Deleting the temporary directory is now
  deferred until after its contents have been copied into the game directory,
  and if an error is encountered when deleting the temporary directory, it is
  logged but does not cause the masterlist update to fail.
- An error creating a game handle for Skyrim if ``loadorder.txt`` is not encoded
  in UTF-8. In this case, libloadorder will now fall back to interpreting its
  contents as encoded in Windows-1252, to match the behaviour when reading the
  load order state.

0.13.8 - 2018-09-24
===================

Fixed
-----

- Filesystem errors when trying to set permissions during a masterlist update
  that clones a new repository.

0.13.7 - 2018-09-10
===================

Changed
-------

- Significantly improve plugin loading performance by scanning for BSAs/BA2s
  once instead of for each plugin.
- Improve performance of metadata evaluation by caching CRCs with the same
  cache lifetime as condition results.
- Improve performance of sorting when it involves long plugin interaction
  chains.
- Updated esplugin to v2.0.1.
- Updated libgit2 to v0.27.4.
- Updated libloadorder v11.4.1.
- Updated spdlog to v1.1.0.
- Updated yaml-cpp to 0.6.2+merge-key-support.2.

Fixed
-----

- Fallout 4's `DLCUltraHighResolution.esm` is now handled as a hardcoded plugin
  (via libloadorder).

0.13.6 - 2018-06-29
===================

Changed
-------

- Tweaked masterlist repository cloning to avoid undefined behaviour.
- Updated Boost to v1.67.0.
- Updated esplugin to v2.0.0.
- Updated libgit2 to v0.27.2.
- Updated libloadorder to v11.4.0.

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

.. _regression: https://github.com/rust-lang/cargo/issues/5518

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
