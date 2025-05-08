***************
Version History
***************

0.26.1 - 2025-05-01
===================

Added
-----

- Support for TES IV: Oblivion Remastered. A game handle can be obtained using :cpp:any:`loot::GameType::oblivionRemastered`.

  - Oblivion Remastered uses a similar load order system to Skyrim (not Skyrim
    Special Edition), so libloot will similarly read and write a
    ``loadorder.txt`` in the same directory as ``Plugins.txt``.
  - Like OpenMW, Oblivion Remastered does not force master plugins to load
    before other plugins: all plugins are treated as non-masters.

Fixed
-----

- A crash could occur when creating a game handle for an OpenMW install that
  does not define any user config paths. An error now occurs instead. Via
  libloadorder.
- A crash could occur when loading plugin files. Via esplugin.
- The ``description_contains()`` metadata condition function did not read the
  description field of OpenMW plugins. Via loot-condition-interpreter.
- No debug info was included for libloot's Rust dependencies (esplugin,
  libloadorder and loot-condition-interpreter). Via libloadorder.
- A few compiler warnings.

Changed
-------

- Paths in condition strings are no longer restricted to staying within the
  directory tree that starts one level above the game's main plugins directory
  (usually the ``Data`` directory). Via loot-condition-interpreter.
- Updated esplugin to v6.1.3.
- Updated loot-condition-interpreter to v5.3.2.
- Updated libloadorder to v18.4.0.

0.26.0 - 2025-04-19
===================

Added
-----

- :cpp:any:`loot::DatabaseInterface::Evaluate()`, which evaluates the given
  condition string.
- :cpp:any:`loot::File::GetConstraint()`, which returns the file's constraint
  string or an empty string if the file has no constraint.

Changed
-------

- ``Database::LoadLists()`` has been split into
  :cpp:any:`loot::DatabaseInterface::LoadMasterlist()`,
  :cpp:any:`loot::DatabaseInterface::LoadMasterlistWithPrelude()` and
  :cpp:any:`loot::DatabaseInterface::LoadUserlist()`.
- :cpp:any:`loot::GameInterface::GetPlugin()` and
  :cpp:any:`loot::GameInterface::GetLoadedPlugins()` now return a plugins as
  ``std::shared_ptr<const PluginInterface>`` instead of
  ``const PluginInterface*``.
- :cpp:any:`loot::PluginInterface::GetBashTags()` now returns a
  ``std::vector<std::string>`` instead of a ``std::vector<Tag>``, as only the
  Tag objects' name values were relevant.
- The callback that is passed to :cpp:any:`loot::SetLoggingCallback()` now takes
  a ``std::string_view`` instead of a ``const char*`` as its second parameter.
- :cpp:any:`loot::Group::DEFAULT_NAME` and
  :cpp:any:`loot::MessageContent::DEFAULT_LANGUAGE` are now ``std::string_view``
  instead of ``const char*``.
- API functions that take string arguments now take them as ``std::string_view``
  instead of ``const std::string&``, with the exception of
  :cpp:any:`loot::GameInterface::IsPluginActive()` and
  :cpp:any:`loot::DatabaseInterface::Evaluate()`.
- The :cpp:any:`loot::File` constructor has been updated to allow a constraint
  value to be provided.
- libloot now supports v0.26 of the metadata syntax.
- The ``/Zc:__cplusplus`` compiler flag is now set when building libloot using
  MSVC.
- The yaml-cpp dependency can no longer be found using CMake's ``find_package``.
  It's now possible to point libloot to an existing yaml-cpp source directory
  by setting the ``FETCHCONTENT_SOURCE_DIR_YAML-CPP`` CMake variable, so offline
  builds are still possible.
- Updated esplugin to v6.1.2.
- Updated fmt to v11.1.4.
- Updated libloadorder to v18.3.0.
- Updated loot-condition-interpreter to v5.3.0.
- Updated spdlog to v1.15.2.
- Updated yaml-cpp to v0.8.0+merge-key-support.2.

0.25.5 - 2025-03-15
===================

Added
-----

- :cpp:any:`loot::SetLogLevel()`, which allows libloot to skip invoking the
  logging callback for log messages with a severity lower than the set level.

Fixed
-----

- The :cpp:any:`loot::GameInterface::LoadPlugins()` fix in v0.25.4 broke loading
  plugins for Morrowind, OpenMW and Starfield when loading a plugin that
  depended on another plugin that was not included in the input but which had
  previously been loaded.
- The public API's ``condition_syntax_error.h`` and ``game_interface.h`` headers
  were missing includes for some of the headers that they rely on.

0.25.4 - 2025-03-04
===================

Fixed
-----

- The changes to groups handling during sorting that were introduced in v0.25.0
  included an optimisation that skipped processing groups that had already been
  processed, but it prematurely skipped groups when the defined groups included
  one that loaded directly after more than one other group.
- :cpp:any:`loot::GameInterface::LoadPlugins()` added loaded plugins to the
  internal cache before resolving their record IDs, so a failure to do the
  latter could result in valid loaded plugin state being replaced by invalid
  plugin state. The cache is now only updated after record IDs have been
  successfully resolved.
- A couple of misleading log statements about edges being skipped during sorting
  to avoid cycles were also written when edges were skipped due to a path
  between the plugins in question already existing.
- The documentation for :cpp:any:`loot::SetLoggingCallback()` incorrectly stated
  that libloot would print mesages to the console until that function was
  called.

0.25.3 - 2025-02-23
===================

Changed
-------

- Updated loot-condition-interpreter to v5.2.0.

0.25.2 - 2025-02-10
===================

Fixed
-----

- OpenMW configuration is now loaded in a similar way to how OpenMW loads it,
  so a wider variety of config file locations are now supported, including the
  locations used by Linux packages linked to at <https://openmw.org/downloads/>.
  Via libloadorder.
- A panic that could occur when loading an OpenMW load order that contains no
  plugins. Via libloadorder.

Changed
-------

- Updated libloadorder to v18.2.2.

0.25.1 - 2025-02-03
===================

Fixed
-----

- :cpp:any:`loot::GameInterface::IsValidPlugin()` returned false for OpenMW
  ``.omwscripts`` plugins.

0.25.0 - 2025-02-02
===================

Added
-----

- Support for OpenMW. A game handle can be obtained for OpenMW using
  :cpp:any:`loot::GameType::openmw`. There are a few notable differences between
  OpenMW and other supported games:

  - OpenMW does not provide any way to persist changes to the load order
    positions of inactive plugins, so getting the load order after setting it
    and reloading the load order state may give an order different to what was
    set.
  - OpenMW does not force master plugins to load before other plugins: all
    plugins are treated as non-masters.
  - OpenMW supports a practically unlimited number of active plugins
    (2147483646).
  - While libloot (via libloadorder) attempts to give the same result as the
    OpenMW Launcher when reading the load order, the Launcher may automatically
    activate some plugins and hide others from view, while libloot does neither.
  - OpenMW makes much greater use of additional data paths than other games: as
    such most plugin paths will probably need to passed to libloot as absolute
    paths to ensure that they are resolved correctly.
  - Ghosted plugins are not supported for OpenMW.

- :cpp:any:`loot::GameInterface::ClearLoadedPlugins()`

Fixed
-----

- Several cases where plugin group memberships would cause cyclic interaction
  errors during sorting.
- Running the tests using ctest.
- The minimum required CMake version given in CMakeLists.txt is now 3.24, which
  should more accurately reflect reality.

Changed
-------

- :cpp:any:`loot::GameInterface::LoadPlugins()` no longer clears the data of
  previously-loaded plugins, though if any of the given paths have filenames
  that match previously-loaded plugins, the previously-loaded data will be
  still be replaced.
- :cpp:any:`loot::GameInterface::SortPlugins()` now takes a vector of filenames
  instead of a vector of strings, and no longer loads the given plugins. It
  instead expects the plugins to have already been loaded.
- The application of plugin groups as part of the sorting process has been
  overhauled. As well as fixing several known bugs, the new approach avoids
  causing cyclic interaction errors, handles groups more consistently and is
  easier to understand. As a result of these changes, sorting may now give
  different results compared to v0.24.5 and earlier.
- The performance of :cpp:any:`loot::GameInterface::SortPlugins()` has
  significantly improved, particularly for larger load orders, with over 2x
  v0.24.5's performance observed.
- When resolving a given plugin path, libloot may add a ``.ghost`` file
  extension is the path doesn't exist, and will now log a debug message when it
  does so.
- The build no longer uses CMake's bundled ``FindBoost`` module as it is now
  deprecated, so setting ``BOOST_ROOT`` to locate Boost no longer has any
  effect. Instead, include the Boost path in ``CMAKE_PREFIX_PATH``.
- Updated Boost to v1.87.0.
- Updated fmt to v11.1.3.
- Updated libloadorder to v18.2.1.
- Updated loot-condition-interpreter to v5.0.0.
- Updated spdlog to v1.15.1.

Removed
-------

- ``loot::GameInterface::IdentifyMainMasterFile()``: callers should instead
  call :cpp:any:`loot::GameInterface::LoadPlugins()` with the main master file
  to load only its headers, and omit the main master file when calling
  :cpp:any:`loot::GameInterface::LoadPlugins()` to fully load plugins.
- Prebuilt Linux release binaries are no longer provided, as the binaries that
  were previously provided were not very portable beyond the Linux distribution
  versions that they were built on.

0.24.5 - 2024-10-24
===================

Fixed
-----

- Starfield's ``ShatteredSpace.esm`` is now placed after ``OldMars.esm``, to
  match where it is hardcoded to load by the game.

Changed
-------

- Updated libloadorder to 18.1.3.

0.24.4 - 2024-10-08
===================

Fixed
-----

- A performance regression introduced in v0.24.0 when interacting with the load
  order, particularly when using
  :cpp:any:`loot::GameInterface::LoadCurrentLoadOrderState()`. Via libloadorder.

Changed
-------

- Updated esplugin to 6.1.1.
- Updated libloadorder to 18.1.2.
- Updated loot-condition-interpreter to 4.0.2.

0.24.3 - 2024-10-05
===================

Fixed
-----

- An incorrect validation error when setting the load order and a game's CCC
  file contains a plugin that has a hardcoded load order index.

0.24.2 - 2024-10-05
===================

Added
-----

- ``ShatteredSpace.esm`` is now recognised as a Starfield plugin with a
  hardcoded load order index of ``01``. Via libloadorder.

Changed
-------

- Starfield's official plugins were treated as being implicitly active, but now
  ``Starfield.esm``, ``Constellation.esm``, ``OldMars.esm``, ``SFBGS003.esm``,
  ``SFBGS004.esm``, ``SFBGS006.esm``, ``SFBGS007.esm`` and ``SFBGS008.esm`` have
  hardcoded load order indexes, to match the behaviour of Starfield v1.14.70.0.
  Via libloadorder.
- Updated libloadorder to v18.1.0.

0.24.1 - 2024-09-15
===================

Changed
-------

- spdlog's bundled copy of the fmt library is no longer used: instead, the fmt
  dependency is managed separately. If fmt is not found by CMake when building
  libloot, fmt v11.0.2 will be downloaded from GitHub.

0.24.0 - 2024-09-14
===================

Added
-----

- Support for Starfield's blueprint master plugin type.
- :cpp:any:`loot::PluginInterface::IsBlueprintPlugin()`
- :cpp:any:`loot::EdgeType::blueprintMaster`
- :cpp:any:`loot::loot_condition_interpreter_category()`, which returns the
  ``std::error_category`` that is used when throwing loot-condition-interpreter
  errors as :cpp:any:`loot::ConditionSyntaxError` exceptions.

Fixed
-----

- Cross-compiling from Linux to Windows using MinGW-w64 was broken in 0.23.1.

Changed
-------

- Blueprint master plugins are now sorted after all other plugins, to match
  Starfield's behaviour. If a non-blueprint-master plugin has a blueprint master
  as one of its masters, sorting logs a warning but does not fail. Other cyclic
  dependencies (e.g. involving requirement metadata) will cause sorting to fail.
- :cpp:any:`loot::ConditionSyntaxError` now inherits from ``std::system_error``.
- The Linux shared library no longer has a runpath set. It was previously set to
  ``.``.
- Updated esplugin to v6.1.0.
- Updated libloadorder to v18.0.0.

0.23.1 - 2024-08-24
===================

Added
-----

- :cpp:any:`loot::esplugin_category()`, which returns the
  ``std::error_category`` that is used when throwing esplugin errors as
  exceptions.

Fixed
-----

- Inaccurate log messages when getting early loading plugins.

Changed
-------

- When an esplugin function returns an error, it is now thrown as a
  ``std::system_error`` using the error category returned by
  :cpp:any:`loot::esplugin_category()`, instead of as a
  :cpp:any:`loot::FileAccessError`.

0.23.0 - 2024-06-29
===================

Added
-----

- Support for Starfield's new medium plugin type (via esplugin and
  libloadorder).
- :cpp:any:`loot::PluginInterface::IsMediumPlugin()`
- :cpp:any:`loot::PluginInterface::IsValidAsMediumPlugin()`

Fixed
-----

- A couple of use-after-free bugs that could happen when configuring additional
  data paths (e.g. for Fallout 4 installs from the Microsoft Store).
- :cpp:any:`loot::PluginInterface::IsMaster()` incorrectly checked the file
  extension of Morrowind plugins instead of their header's master flag (via
  esplugin).
- :cpp:any:`loot::PluginInterface::DoRecordsOverlap()` was not able to reliably
  check if two Starfield plugins had overlapping records (via esplugin).
- The sorting process was unable to reliably count the number of records that a
  Starfield plugin overrode, which could negatively impact the quality of the
  sorted load order (via esplugin).
- If a non-master plugin was a master of two master plugins, it would be hoisted
  to load before the master that loaded second instead of the master that loaded
  first (via libloadorder).
- If more than one plugin needed to be hoisted while reading the load order,
  some plugins could be moved too late in the load order (via libloadorder).
- When getting the load order, the result will now correctly reflect the
  supported games' ability to hoist master files to load above other master
  files that depend on them (via libloadorder).


Changed
-------

- Support for getting and setting Starfield's load order has been updated to
  reflect the changes in the game's behaviour between its initial release and
  the arrival of the Creation Kit alongside Starfield v1.12.30.0 (via
  libloadorder).

  - Starfield's ``Starfield.ccc`` file will now be read from the
    ``My Games\Starfield`` directory as well as its install directory, with the
    former taking precedence over the latter.
  - Starfield plugins will now be loaded from the ``My Games\Starfield\Data``
    directory as well as the game install path's ``Data`` directory, but only
    for plugins that are present in both directories.
  - Starfield's ``Starfield.esm``, ``Constellation.esm`` and ``OldMars.esm`` are
    no longer treated as hardcoded: instead, they are now treated as implicitly
    active, along with ``BlueprintShips-Starfield.esm``, ``SFBGS003.esm``,
    ``SFBGS006.esm``, ``SFBGS007.esm`` and ``SFBGS008.esm``.
  - Plugins that have the update flag (introduced by Starfield) set are no
    longer given special treatment when checking active plugin limits, to match
    Starfield's current behaviour. Previously such plugins would not count
    towards the maximum number of plugins you could have active at the same
    time.

- :cpp:any:`loot::GameInterface::LoadPlugins()` will now throw if attempting to
  fully load Morrowind or Starfield plugins without also loading all their
  masters.
- Renamed ``PluginInterface::IsOverridePlugin()`` to
  :cpp:any:`loot::PluginInterface::IsUpdatePlugin()` to reflect the terminology
  used by Starfield's Creation Kit.
- Renamed ``PluginInterface::IsValidAsOverridePlugin()`` to
  :cpp:any:`loot::PluginInterface::IsValidAsUpdatePlugin()` to reflect the
  terminology used by Starfield's Creation Kit.
- Updated esplugin to v6.0.0.
- Updated libloadorder to v17.0.1.
- Updated loot-condition-interpreter to v4.0.1.

0.22.4 - 2024-05-03
===================

Added
-----

- Support Fallout 4 installs from the Epic Games Store.
- Support for Fallout 4's new BA2 versions.
- A ``LIBLOOT_BUILD_TESTS`` CMake option that defaults to ``ON`` and allows you
  to disable building tests and their dependencies.
- A ``LIBLOOT_INSTALL_DOCS`` CMake option that defaults to ``ON`` and allows you
  to skip installing the docs.
- ``ESPLUGIN_URL``, ``ESPLUGIN_HASH``, ``LIBLOADORDER_URL``,
  ``LIBLOADORDER_HASH``, ``LOOT_CONDITION_INTERPRETER_URL`` and
  ``LOOT_CONDITION_INTERPRETER_HASH`` CMake variables for overriding the URLs
  and archive hashes used to fetch esplugin, libloadorder and
  loot-condition-interpreter.

Changed
-------

- It's now possible to use existing builds of Google Test, spdlog and yaml-cpp
  that CMake can find installed.
- The build archives now include the necessary CMake config to be found by
  CMake's ``FindPackage``.
- The build archives produced by CPack now follow the GNU directory structure
  (e.g. ``bin``, ``include``, ``lib``, ``share``).
- Updated esplugin to v5.0.1.
- Updated libloadorder v16.0.0.
- Updated loot-condition-interpreter to v4.0.0.
- Updated spdlog to v1.14.1.
- Updated yaml-cpp to v0.8.0+merge-key-support.2.

Removed
-------

- The Linux build no longer links to ``stdc++fs``.

0.22.3 - 2023-12-06
===================

Added
-----

- :cpp:any:`loot::PluginInterface::IsValidAsLightPlugin()` now supports the
  extended FormID range introduced in Skyrim SE v1.6.1130.0 for light plugins
  with a header version of 1.71. Via esplugin.

Fixed
-----

- :cpp:any:`loot::PluginInterface::IsValidAsLightPlugin()` used Fallout 4's
  extended range when validating FormIDs, even when the plugin's header version
  was less than 1.0. Plugins with header versions less than 1.0 are now
  validated using the appropriate range (0x800 to 0xFFF). Via esplugin.

Changed
-------

- Updated esplugin to v4.1.1.

0.22.2 - 2023-11-25
===================

Fixed
-----

- libloot could error when reading the load order if it encountered a game ini
  file containing single or double quote or backslash characters, as it
  attempted to treat them as special characters. Via libloadorder.

Changed
-------

- Updated libloadorder to v15.0.2.

0.22.1 - 2023-10-06
===================

Changed
-------

- Updated Boost to v1.83.0.
- Updated libloadorder to v15.0.1.
- Updated spdlog to v1.12.0.
- Updated yaml-cpp to v0.8.0+merge-key-support.1.

Fixed
-----

- :cpp:any:`loot::GameInterface::LoadCurrentLoadOrderState()` now correctly
  handles the case where plugin timestamps matter for load order and two plugins
  have the same timestamp. The plugins are sorted in ascending filename order
  for Starfield and descending filename order for all other games. Via
  libloadorder.
- :cpp:any:`loot::GameInterface::LoadCurrentLoadOrderState()` now sorts
  installed plugins by timestamp instead of filename before they are added to
  the load order. This ensures that plugins that do not have an explicit load
  order position are given a position that matches the game's behaviour and the
  behaviour of xEdit and Wrye Bash. Via libloadorder.
- :cpp:any:`loot::GameInterface::IsLoadOrderAmbiguous()` now always returns
  false for Morrowind, Oblivion, Fallout 3 and Fallout New Vegas. Via
  libloadorder.
- :cpp:any:`loot::GameInterface::IsLoadOrderAmbiguous()` no longer requires
  implicitly active plugins to be listed in ``plugins.txt`` for the load order
  to be unambiguous for Skyrim Special Edition, Skyrim VR, Fallout 4,
  Fallout 4 VR and Starfield. Via libloadorder.
- :cpp:any:`loot::GameInterface::SetLoadOrder()` now modifies plugin file
  modification timestamps to match the given load order if the game is
  Fallout 4, Fallout 4 VR or Starfield, and if ``plugins.txt`` is being ignored.
  Via libloadorder.

0.22.0 - 2023-09-29
===================

Added
-----

- Support for Starfield. A game handle can be obtained for Starfield using
  :cpp:any:`loot::GameType::starfield`.
- :cpp:any:`loot::PluginInterface::IsOverridePlugin()` and
  :cpp:any:`loot::PluginInterface::IsValidAsOverridePlugin()` to support
  Starfield's new override plugin type, which does not use up a mod index when
  active. Override plugins cannot contain any new records, they can only
  override records added by their masters.
- libloot can now detect the correct game local path for Microsoft Store
  installs of Skyrim Special Edition and Fallout 4, and Epic Games Store
  installs of Fallout: New Vegas. Via libloadorder.

Fixed
-----

- Only lowercase plugin file extensions were recognised as plugin file
  extensions when evaluating conditions. Via loot-condition-interpreter.
- Fallout: New Vegas plugins with corresponding ``.nam`` files are now
  identified as being active. Via libloadorder.
- Plugins activated using the ``sTestFile1`` through ``sTestFile10`` ini file
  properties are now recognised as being active for all games other than
  Morrowind, which does not support those properties. The properties are used by
  default in Fallout 3, Fallout: New Vegas and Skyrim Special Edition. Via
  libloadorder.
- Fallout 4's ``Fallout4.ccc`` and ``plugins.txt`` and Fallout 4 VR's
  ``plugins.txt`` are now ignored when the game has plugins activated using
  the ``sTestFile1`` through ``sTestFile10`` ini file properties, to match the
  games' behaviour. Via libloadorder.
- When deciding where to look for Oblivion's ``plugins.txt``, the
  ``bUseMyGamesDirectory`` ini property is now correctly expected in the
  ``[General]`` section of ``Oblivion.ini``, instead of anywhere in the file.
  Via libloadorder.

0.21.0 - 2023-09-13
===================

Added
-----

- :cpp:any:`loot::GameInterface::GetType()`
- A ``const`` overload of :cpp:any:`loot::GameInterface::GetDatabase()`
- :cpp:any:`loot::GameInterface::GetAdditionalDataPaths()`
- :cpp:any:`loot::GameInterface::SetAdditionalDataPaths()`

Changed
-------

- libloot now supports v0.21 of the metadata syntax.
- :cpp:any:`loot::GameInterface::IsValidPlugin()`,
  :cpp:any:`loot::GameInterface::LoadPlugins()` and
  :cpp:any:`loot::GameInterface::SortPlugins()` now take plugin paths as
  ``std::filesystem::path`` instead of ``std::string``.
- It is now possible to create a game handle with a ``game_local_path`` that
  does not exist.
- It is now possible to create a game handle with an empty ``game_local_path``
  on Linux if the ``game`` is Morrowind.
- Updated ICU (used by Linux builds) to v71.1.
- Updated libloadorder to v14.2.1.
- Updated loot-condition-interpreter to v3.0.0.

Fixed
-----

- Condition evaluation would only recognise plugin files as plugins if they had
  lowercase file extensions.
- When reading the list of active plugins for Oblivion, libloot would look for a
  file named ``plugins.txt``, which caused inaccurate results on case-sensitive
  filesystems, as Oblivion writes the file as ``Plugins.txt``.
- The doc comment for :cpp:any:`loot::GameInterface::SortPlugins()` used the
  wrong parameter name.
- Cross-compiling from Linux to Windows using MinGW-w64.

Removed
-------

- The ``loot::SimpleMessage`` struct.
- The ``loot::ToSimpleMessage()`` function.
- The ``loot::ToSimpleMessages()`` function.

0.19.4 - 2023-05-06
===================

Added
-----

- Support for the Microsoft Store's Fallout 4 DLC. The Microsoft Store installs
  Fallout 4's DLC to separate directories outside of the Fallout 4 install path,
  and the Microsoft Store's version of Fallout 4 knows to look for plugins and
  resources to load in those other directories.

  - libloot detects if a copy of Fallout 4 is from the Microsoft Store by
    checking for the existence of an ``appxmanifest.xml`` file in the given
    install path, and if found will look for Fallout 4 DLC directories at their
    install paths. The DLC install paths used are relative to the game install
    path because those relative paths are assumed by the game.
  - If a DLC data path exists, load order operations will include plugins in
    that directory, i.e. DLC plugins will appear as part of the load order
    that libloot reads and writes.
  - Metadata conditions will check for files in DLC data paths as well as the
    game's data path, with DLC paths checked before the game's data path to
    match the order in which the game checks paths.

Changed
-------

- :cpp:any:`loot::GameInterface::IsValidPlugin()`,
  :cpp:any:`loot::GameInterface::LoadPlugins()` and
  :cpp:any:`loot::GameInterface::SortPlugins()` now take plugin paths instead of
  plugin filenames. Relative paths are interpreted as relative to the game's
  data path, so this change is backwards-compatible. Absolute paths are used as
  given. The functions take plugin paths as strings to avoid breaking libloot's
  binary interface, but they will be changed to take ``std::filesystem::path``
  in a future release.
- :cpp:any:`loot::GameInterface::LoadPlugins()` and
  :cpp:any:`loot::GameInterface::SortPlugins()` now check that all filenames in
  the given paths are unique. This was previously implicitly required for
  correct behaviour but not explicitly enforced.

0.19.3 - 2023-03-18
===================

Added
-----

- Support for the Steam and GOG distributions of Enderal: Forgotten Stories and
  Enderal: Forgotten Stories (Special Edition), which are total conversion mods
  for Skyrim and Skyrim Special Edition respectively. This support means that
  the game local path does not need to be specified when creating a game handle:
  when libloot is given the path to a Skyrim or Skyrim SE installation that is
  actually an Enderal installation, it is now able to look up the correct
  game local path. Via libloadorder.

Fixed
-----

- libloot would deactivate plugins when setting the load order if too many
  plugins were active. This could cause unexpected behaviour if later-loading
  active plugins were sorted to load earlier.
- The path returned by :cpp:any:`loot::CyclicInteractionError::GetCycle()`
  could include too many vertices, including repeated vertices.

Changed
-------

- Updated Boost to v1.81.0.
- Updated libloadorder to v14.0.0.

0.19.2 - 2023-01-13
===================

Fixed
-----

- libloot v0.19.1 did not take user groups into account when avoiding cycles
  during sorting, causing unnecessary cyclic interaction errors.

Changed
-------

- Sorting will once more throw a cyclic interaction error if there is any plugin
  data or metadata that would try to load a master-flagged plugin after a
  non-master-flagged plugin. This behaviour was removed as a side-effect of
  sorting changes made in libloot v0.19.0.

0.19.1 - 2023-01-09
===================

Fixed
-----

- Sorting and applying and then sorting again will no longer give a different
  result for the second sort. libloot v0.19.0 changed the order in which group
  and overlap edges were processed to be the current load order: it has now
  reverted back to the lexicographical order of plugin filenames.

0.19.0 - 2023-01-07
===================

Added
-----

- Sorting now takes into account overlapping assets in BSAs/BA2s that are loaded
  by plugins. If two plugins don't make changes to the same record but load BSAs
  (or BA2s for Fallout 4) that contain data for the same asset path, the plugin
  that loads more assets will load first (unless that's contradicted by
  higher-priority data and metadata).
- :cpp:any:`loot::GameInterface::GetActivePluginsFilePath()`, which returns the
  path of the file libloot reads to determine which plugins are active.
- :cpp:any:`loot::EdgeType::masterlistGroup`,
  :cpp:any:`loot::EdgeType::userGroup`,
  :cpp:any:`loot::EdgeType::recordOverlap` and
  :cpp:any:`loot::EdgeType::assetOverlap`.

Fixed
-----

- Building libloot using CMake versions older than 3.24.
- A few potential null pointer dereferences.

Changed
-------

- Sorting has been heavily optimised, leading to sorting being about 58 times
  faster than libloot 0.18.3 with large load orders:

  - The plugin graph used during sorting has been split in two. As a result,
    any plugin data or metadata that would previously caused a cyclic
    interaction error due to contradicting a plugin's master flag being set is
    now silently ignored instead.
  - The tie-breaking stage has been completely overhauled. As a result, some
    ties may now be broken differently to how they were broken in previous
    versions of libloot.
  - :cpp:any:`loot::GameInterface::LoadPlugins()` now checks plugin validity in
    parallel.

- Cyclic interaction errors now distinguish between group edges that involve
  user metadata and those that don't.
- ``PluginInterface::DoFormIDsOverlap()`` has been renamed to
  :cpp:any:`loot::PluginInterface::DoRecordsOverlap()`.
- :cpp:any:`loot::CyclicInteractionError::GetCycle()` is now ``const``.
- :cpp:any:`loot::UndefinedGroupError::GetGroupName()` is now ``const``.
- Linux builds are now built using GCC 10 and now link against the ``tbb``
  library.

Removed
-------

- ``EdgeType::group``
- ``EdgeType::overlap``

0.18.3 - 2022-12-13
===================

Fixed
-----

- Resolved a CMake warning relating to policy CMP0135 when building libloot.
- Some of the documentation on ``not`` operators in the metadata syntax was
  outdated.
- The libloot Windows DLL did not include some file info fields that are
  required according to Microsoft's documentation. The ``CompanyName``,
  ``FileDescription``, ``InternalName``, ``OriginalFilename`` and
  ``ProductName`` fields have been added.
- The libloot Windows DLL advertised its ``FILETYPE`` as ``VFT_APP``, which has
  been changed to ``VFT_DLL``.

Changed
-------

- Sorting optimisations mean that sorting is now significantly faster (over 5
  times faster in testing).
- Log message severities have been adjusted to reduce the verbosity at the
  "info" level and to move some messages between "debug" and "trace".
- Release build archive names no longer include the output of ``git describe``.
- Updated spdlog to v1.11.0.

0.18.2 - 2022-10-11
===================

Fixed
-----

- libloot will now use the correct local app data path for the Epic Games Store
  distribution of Skyrim Special Edition when no local app data path is passed
  to :cpp:any:`loot::CreateGameHandle()`. Via libloadorder.

Changed
-------

- Updated libloadorder to v13.3.0.

0.18.1 - 2022-10-01
===================

Fixed
-----

- libloot will now use the correct local app data path for the GOG distribution
  of Skyrim Special Edition when no local app data path is passed to
  :cpp:any:`loot::CreateGameHandle()`. Via libloadorder.
- If Oblivion's ``Oblivion.ini`` could not be found or read, or if it did not
  contain the ``bUseMyGamesDirectory`` setting, the game's install path would be
  used as the parent directory for ``plugins.txt``. libloot now correctly
  defaults to using the game's local app data directory, and only uses the
  install path if ``bUseMyGamesDirectory=0`` is found. Via libloadorder.

Changed
-------

- When serialising plugin metadata as YAML, LOOT now:

  - Puts ``url`` before ``group``
  - Serialises single-element lists using the flow style if the element would be
    serialised as a scalar value
  - Pads CRC hexadecimal values to always be 8 characters long (excluding the
    ``0x`` prefix)
  - Uses uppercase letters in CRC hexadecimal values.

- Updated esplugin to v4.0.0.
- Updated Google Test to v1.12.1.
- Updated libloadorder to v13.2.0.
- Updated loot-condition-interpreter to v2.3.1.
- Updated spdlog to v1.10.0.

0.18.0 - 2022-02-27
===================

Added
-----

- :cpp:any:`loot::Group::DEFAULT_NAME` gives the default group name as a
  compile-time constant.
- :cpp:any:`loot::ToSimpleMessages()` turns a ``std::vector<Message>`` into a
  ``std::vector<SimpleMessage>`` for a given language.
- :cpp:any:`loot::GameInterface::IsLoadOrderAmbiguous()` exposes libloadorder's
  ``lo_is_ambiguous()`` function.

Fixed
-----

- :cpp:any:`loot::SimpleMessage` now uses an in-class initialiser to ensure that
  its ``type`` member variable is always initialised.
- Added missing virtual destructors to :cpp:any:`loot::GameInterface`,
  :cpp:any:`loot::DatabaseInterface` and :cpp:any:`loot::PluginInterface`.
- Two versions that only differ by the presence and absence of pre-release
  identifiers were not correctly compared according to Semantic Versioning,
  which states that 1.0.0-alpha is less than 1.0.0. Via
  loot-condition-interpreter.
- Some missing API documentation and formatting issues.

Changed
-------

- :cpp:any:`loot::CreateGameHandle()` now returns a
  ``std::unique_ptr<GameInterface>`` instead of a
  ``std::shared_ptr<GameInterface>``.
- :cpp:any:`loot::GameInterface::GetDatabase()` now returns a
  ``DatabaseInterface&`` instead of a ``std::shared_ptr<DatabaseInterface>``.
- :cpp:any:`loot::GameInterface::GetPlugin()` now returns a
  ``const PluginInterface*`` instead of a
  ``std::shared_ptr<const PluginInterface>``.
- :cpp:any:`loot::GameInterface::GetLoadedPlugins()` now returns a
  ``std::vector<const PluginInterface*>`` instead of a
  ``std::vector<std::shared_ptr<const PluginInterface>>``.
- ``MessageContent::defaultLanguage`` has been replaced with
  :cpp:any:`loot::MessageContent::DEFAULT_LANGUAGE`, which is a compile-time
  constant.
- ``File::ChooseDetail()``,
  ``Message::GetContent(const std::string& language)``,
  ``MessageContent::Choose()`` and ``PluginCleaningData::ChooseDetail()`` have
  been replaced with :cpp:any:`loot::SelectMessageContent`.
- ``Message::ToSimpleMessage()`` has been replaced with
  :cpp:any:`loot::ToSimpleMessage()`.
- ``LootVersion`` has been replaced with :cpp:any:`loot::LIBLOOT_VERSION_MAJOR`,
  :cpp:any:`loot::LIBLOOT_VERSION_MINOR`,
  :cpp:any:`loot::LIBLOOT_VERSION_PATCH`, :cpp:any:`loot::GetLiblootVersion()`
  and :cpp:any:`loot::GetLiblootRevision()`.
- :cpp:any:`loot::File::GetDisplayName()` is now a trivial accessor that only
  ever returns the value of the display name member variable and performs no
  character escaping.
- :cpp:any:`loot::CyclicInteractionError` and
  :cpp:any:`loot::UndefinedGroupError` have had their ``const`` member variables
  made non-``const``.
- :cpp:any:`loot::ConditionalMetadata`, :cpp:any:`loot::File`,
  :cpp:any:`loot::Filename`, :cpp:any:`loot::Group`, :cpp:any:`loot::Location`,
  :cpp:any:`loot::Message`, :cpp:any:`loot::MessageContent`,
  :cpp:any:`loot::PluginCleaningData`, :cpp:any:`loot::PluginMetadata` and
  :cpp:any:`loot::Tag` have had their user-defined default constructors replaced
  by use of in-class initialisers and defaulted default constructors.
- The ``<`` and ``==`` operator overloads for :cpp:any:`loot::File`,
  :cpp:any:`loot::Group`, :cpp:any:`loot::Location`, :cpp:any:`loot::Message`,
  :cpp:any:`loot::MessageContent`, :cpp:any:`loot::PluginCleaningData` and
  :cpp:any:`loot::Tag` have become non-member functions.
- The performance of :cpp:any:`loot::PluginMetadata::NameMatches()` has been
  greatly improved by not constructing a new regex object every time the
  function is called.
- Mentions of GitHub Flavored Markdown have been replaced with CommonMark, as
  LOOT now uses the latter instead of the former.
- Updated loot-condition-interpreter to v2.3.0.

Removed
-------

- ``ConditionalMetadata::ParseCondition()``
- ``PluginMetadata::NewMetadata()``
- All Git-related functionality has been removed, including the libgit2
  dependency and the following API items:

  - ``loot::UpdateFile()``
  - ``loot::GetFileRevision()``
  - ``loot::IsLatestFile()``
  - ``loot::libgit2_category()``
  - ``loot::GitStateError``
  - ``loot::FileRevision``


0.17.3 - 2022-01-02
===================

Added
-----

- :cpp:any:`PluginMetadata::AsYaml()` can be used to serialise plugin metadata as YAML.

Changed
-------

- Plugin name regular expression objects are now cached between calls to :cpp:any:`DatabaseInterface::LoadLists()`.

0.17.2 - 2021-12-24
===================

Fixed
-----

- A missing ``<string>`` include in ``include/loot/struct/simple_message.h``.
- Invalid configuration causing Read The Docs to fail to build the
  documentation.

Changed
-------

- Updated libgit2 to v1.3.0.

0.17.1 - 2021-11-13
===================

Fixed
-----

- Out-of-bounds array access that could occur in some situations and which could
  cause crashes in Linux builds.

0.17.0 - 2021-09-24
===================

Added
-----

- :cpp:any:`DatabaseInterface::LoadLists()` now accepts an optional third
  parameter that is the path to a masterlist prelude file to load. If loaded, it
  will be used to replace the value of the prelude in the loaded masterlist
  (if the masterlist has a prelude).
- The :cpp:any:`Message` class has gained a constructor that takes a
  :cpp:any:`SimpleMessage`.
- The :cpp:any:`File` class has been gained support for the metadata structure's
  new ``detail`` field, adding:

  - An optional ``const std::vector<MessageContent>&`` parameter to the
    multiple-parameter constructor.
  - A new :cpp:any:`File::GetDetail()` member function.
  - A new :cpp:any:`File::ChooseDetail()` member function.

Changed
-------

- ``MasterlistInfo`` has been renamed to :cpp:any:`FileRevision`, and its
  ``revision_id`` and ``revision_date`` fields are now named ``id`` and ``date``
  respectively.
- The ``UpdateMasterlist``, ``GetMasterlistRevision`` and ``IsLatestMasterlist``
  member functions have been moved out of ``DatabaseInterface`` and are now free
  functions named :cpp:any:`UpdateFile`, :cpp:any:`GetFileRevision` and
  :cpp:any:`IsLatestFile` respectively.
- :cpp:any:`PluginInterface::GetHeaderVersion()` now returns a
  ``std::optional<float>`` instead of a ``float``. The return value is
  ``std::nullopt`` if no header version field was found or if its value was NaN.
- Sorting now checks for cycles before adding overlap edges, so that any cycles
  are caught before the slowest steps in the sorting process.
- ``PluginCleaningData::GetInfo()`` has been renamed to
  ``PluginCleaningData::GetDetail()``.
- ``PluginCleaningData::ChooseInfo()`` has been renamed to
  ``PluginCleaningData::ChooseDetail()``.
- All API functions that returned a ``MessageContent`` or ``SimpleMessage`` now
  return a ``std::optional<MessageContent>`` or ``std::optional<SimpleMessage>``
  respectively. This affects the following member functions:

  - :cpp:any:`Message::GetContent()`
  - :cpp:any:`Message::ToSimpleMessage()`
  - :cpp:any:`MessageContent::Choose()`
  - :cpp:any:`PluginCleaningData::ChooseDetail()`

- Updated libgit2 to v1.1.1.
- Updated Google Test to v1.11.0.
- Updated spdlog to v1.9.2.
- Updated yaml-cpp to v0.7.0+merge-key-support.1.

Removed
-------

- :cpp:any:`PluginInterface::IsLightMaster()`
- :cpp:any:`PluginInterface::IsValidAsLightMaster()`
- Updating the masterlist no longer reloads it, the masterlist must now be
  reloaded separately.
- Masterlist update no longer supports rolling back through revisions until a
  revision that can be successfully loaded is found.

0.16.3 - 2021-05-06
===================

Added
-----

- :cpp:any:`PluginInterface::IsLightPlugin()` as a more accurately named
  equivalent to :cpp:any:`PluginInterface::IsLightMaster()`.
- :cpp:any:`PluginInterface::IsValidAsLightPlugin()` as a more accurately named
  equivalent to :cpp:any:`PluginInterface::IsValidAsLightMaster()`.
- Support for parsing inverted metadata conditions (``not (<expression>)``).
  Note however that this is not yet part of any released version of LOOT's
  metadata syntax and must not be used where compatibility with older releases
  of LOOT is required. Via loot-condition-interpreter.

Changed
-------

- :cpp:any:`loot::MessageContent::Choose()` now compares locale and language
  codes so that if an exact match is not present but a more or less specific
  match is present, that will be preferred over the default language message
  content.
- Regular expression functions in metadata conditions now handle ghosted plugins
  in the same way as their path function counterparts.
- Updated esplugin to v3.5.0.
- Updated libloadorder to v13.0.0.
- Updated loot-condition-interpreter to v2.2.1.
- Updated spdlog to v1.8.5.

Fixed
-----

- ``.ghost`` file extensions are no longer recursively trimmed when checking if
  a file has a valid plugin file extension during metadata condition evaluation.
  Via loot-condition-interpreter.
- When looking for a plugin file matching a path during metadata condition
  evaluation, a ``.ghost`` extension is only added to the path if one was not
  already present. Via loot-condition-interpreter.
- When comparing versions during metadata condition evaluation, the comparison
  now compares numeric against non-numeric release identifiers (and vice versa)
  by comparing the numeric value against the numeric value of leading digits in
  the non-numeric value, and treating the latter as greater if the two numeric
  values are equal. The numeric value is treated as less than the non-numeric
  value if the latter has no leading digits. Previously all non-numeric
  identifiers were always greater than any numeric identifier. For example, 78b
  was previously considered to be greater than 86, but is now considered to be
  less than 86. Via loot-condition-interpreter.
- Linux builds did not correctly handle case-insensitivity of plugin names
  during sorting on filesystems with case folding enabled.

Deprecated
----------

- :cpp:any:`PluginInterface::IsLightMaster()`: use
  :cpp:any:`PluginInterface::IsLightPlugin()` instead.
- :cpp:any:`PluginInterface::IsValidAsLightMaster()`: use
  :cpp:any:`PluginInterface::IsValidAsLightPlugin()` instead.

0.16.2 - 2021-02-13
===================

Changed
-------

- Updated libgit2 to v1.1.0.
- Updated loot-condition-interpreter to v2.1.2.
- Updated Boost to v1.72.0.
- Linux releases are now built on GitHub Actions.
- Masterlist updates can no longer be fetched using SSH URLs. This support was
  never tested or documented.

0.16.1 - 2020-08-22
===================

Fixed
-----

- ``File::GetDisplayName()`` now escapes ASCII punctuation characters when
  returning the file's name, i.e. when no display name is explicitly set. For
  example, ``File("plugin.esp").GetDisplayName()`` will now return
  ``plugin\.esp``.

0.16.0 - 2020-07-12
===================

Added
-----

- The ``!=``, ``>``, ``<=`` and ``>=`` comparison operators are now implemented
  for :cpp:any:`loot::File`, :cpp:any:`loot::Location`,
  :cpp:any:`loot::Message`, :cpp:any:`loot::MessageContent`,
  :cpp:any:`loot::PluginCleaningData` and :cpp:any:`loot::Tag`.
- The ``!=``, ``<``, ``>``, ``<=`` and ``>=`` comparison operators are now
  implemented for :cpp:any:`loot::Group`.
- A new :cpp:any:`Filename` class for representing strings handled as
  case-insensitive filenames.
- ``PluginMetadata::NameMatches()`` checks if the given plugin filename matches
  the plugin name of the metadata object it is called on. If the plugin metadata
  name is a regular expression, the given plugin filename will be matched
  against it, otherwise the comparison is case-insensitive equality.


Changed
-------

- ``File::GetName()`` now returns a :cpp:any:`Filename` instead of a
  ``std::string``.
- :cpp:any:`GetGroups()` and :cpp:any:`GetUserGroups()` now return
  ``std::vector<Group>`` instead of ``std::unordered_set<Group>``.
- :cpp:any:`SetUserGroups()` now takes a ``const std::vector<Group>&`` instead
  of a ``const std::unordered_set<std::string>&``.
- :cpp:any:`loot::Group`'s three-argument constructor now takes a
  ``const std::vector<std::string>&`` instead of a
  ``const std::unordered_set<std::string>&`` as its second parameter.
- :cpp:any:`GetAfterGroups()` now returns a ``std::vector<std::string>``
  instead of a ``std::unordered_set<std::string>``.
- ``std::set<>`` usage has been replaced by ``std::vector<>`` throughout the
  public API. This affects the following functions:

  - ``PluginInterface::GetBashTags()``
  - ``DatabaseInterface::GetKnownBashTags()``
  - ``GameInterface::GetLoadedPlugins()``
  - ``PluginMetadata::GetLoadAfterFiles()``
  - ``PluginMetadata::SetLoadAfterFiles()``
  - ``PluginMetadata::GetRequirements()``
  - ``PluginMetadata::SetRequirements()``
  - ``PluginMetadata::GetIncompatibilities()``
  - ``PluginMetadata::SetIncompatibilities()``
  - ``PluginMetadata::GetTags()``
  - ``PluginMetadata::SetTags()``
  - ``PluginMetadata::GetDirtyInfo()``
  - ``PluginMetadata::SetDirtyInfo()``
  - ``PluginMetadata::GetCleanInfo()``
  - ``PluginMetadata::SetCleanInfo()``
  - ``PluginMetadata::GetLocations()``
  - ``PluginMetadata::SetLocations()``

- :cpp:any:`loot::File`, :cpp:any:`loot::Location`, :cpp:any:`loot::Message`,
  :cpp:any:`loot::MessageContent`, :cpp:any:`loot::PluginCleaningData`,
  :cpp:any:`loot::Tag` and :cpp:any:`loot::Group` now implement their comparison
  operators by comparing all their fields (including inherited fields), using
  the same operator for the fields. For example, comparing two
  :cpp:any:`loot::File` objects using ``==`` will now compare each of their
  fields using ``==``.
- When loading plugins, the speed at which LOOT identifies their corresponding
  archive files (``*.bsa`` or ``.ba2``, depending on the game) has been
  improved.


Removed
-------

- ``PluginMetadata::IsEnabled()`` and ``PluginMetadata::SetEnabled()``, as it is
  no longer possible to disable plugin metadata (though doing so never had any
  effect).
- :cpp:any:`PluginMetadata` no longer implements the ``==`` or ``!=`` comparison
  operators.
- ``std::hash`` is no longer specialised for :cpp:any:`loot::Group`.

Fixed
-----

- :cpp:any:`LoadsArchive()` now correctly identifies the BSAs that a Skyrim SE
  or Skyrim VR loads. This assumes that Skyrim VR plugins load BSAs in the same
  way as Skyrim SE. Previously LOOT would use the same rules as the Fallout
  games for Skyrim SE or VR, which was incorrect.
- Some operations involving loaded plugins or copies of game interface objects
  could potentially cause data races due to a lack of mutex locking in some data
  read operations.
- Copying a game interface object did not copy its cached archive files, leaving
  the new copy with no cached archive files.

0.15.2 - 2020-06-14
===================

Changed
-------

- :cpp:any:`MergeMetadata()` now only uses the group value of the given metadata
  object if there is not already one set, matching the behaviour for all other
  merged metadata.
- Updated esplugin to v3.3.1.
- Updated libgit2 to v1.0.1.
- Updated loot-condition-interpreter to v2.1.1.
- Updated spdlog to v1.6.1.

Fixed
-----

- :cpp:any:`GetPluginMetadata()` preferred masterlist metadata over userlist
  metadata when merging them, which was the opposite of the intended behaviour.

0.15.1 - 2019-12-07
===================

Changed
-------

- The range of FormIDs that are recognised as valid in light masters has been
  extended for Fallout 4 plugins, from between 0x800 and 0xFFF inclusive to
  between 0x001 and 0xFFF inclusive, to reflect the extended range supported by
  Fallout 4 v1.10.162.0.0. The valid range for Skyrim Special Edition plugins is
  unchanged. Via esplugin.
- Updated esplugin to v3.3.0.

0.15.0 - 2019-11-05
===================

Changed
-------

- libloot now supports v0.15 of the metadata syntax.
- The order of the plugins passed to :cpp:any:`SortPlugins` is now used
  as the current load order during sorting. The order of plugins passed in did
  not previously have any impact.
- Constructors for the following classes and structs are now ``explicit``:

  - :cpp:any:`loot::ConditionalMetadata`
  - :cpp:any:`loot::File`
  - :cpp:any:`loot::Group`
  - :cpp:any:`loot::Location`
  - :cpp:any:`loot::Message`
  - :cpp:any:`loot::MessageContent`
  - :cpp:any:`loot::PluginCleaningData`
  - :cpp:any:`loot::PluginMetadata`
  - :cpp:any:`loot::Tag`
  - :cpp:any:`loot::MasterlistInfo`
  - :cpp:any:`loot::Vertex`

- Updated loot-condition-interpreter to v2.1.0.
- Updated spdlog to v1.4.2.

Removed
-------

- ``InitialiseLocale()``
- ``PluginMetadata::GetLowercasedName()``
- ``PluginMetadata::GetNormalizedName()``

Fixed
-----

- libloot was unable to extract versions from plugin descriptions containing
  ``version:`` followed by whitespace and one or more digits.
- libloot did not error if masterlist metadata defined a group that loaded after
  another group that was not defined in the masterlist, but which was defined in
  user metadata. This was unintentional, and now all groups mentioned in
  masterlist metadata must now be defined in the masterlist.
- Build errors on Linux using GCC 9 and ICU 61+.

0.14.10 - 2019-09-06
====================

Changed
-------

- Improved the sorting process for Morrowind. Previously, sorting was unable to
  determine if a Morrowind plugin contained any records overriding those of its
  masters, and so added no overlap edges between Morrowind plugins when sorting.
  Sorting now counts override records by comparing plugins against their
  masters, giving the same results as for other games.

  However, unlike for other games, this requires all a plugin's masters to be
  installed. If a plugin's masters are missing, the plugin's total record count
  will be used as if it was the plugin's override record count to ensure that
  sorting can still proceed, albeit with potentially reduced accuracy.
- Updated esplugin to v3.2.0.
- Updated libgit2 to v0.28.3.

0.14.9 - 2019-07-23
===================

Fixed
-----

- Regular expressions in condition strings are now prefixed with ``^`` and
  suffixed with ``$`` before evaluation to ensure that only exact matches to the
  given expression are found. Via loot-condition-interpreter.

Changed
-------

- Updated loot-condition-interpreter to v2.0.0.

0.14.8 - 2019-06-30
===================

Fixed
-----

- Evaluating ``version()`` and ``product_version()`` conditions will no longer
  error if the given executable has no version fields. Instead, it will be
  evaluated as having no version. Via loot-condition-interpreter.
- Sorting would not preserve the existing relative positions of plugins that had
  no relative positioning enforced by plugin data or metadata, if one or both of
  their filenames were not case-sensitively equal to their entries in
  ``plugins.txt`` / ``loadorder.txt``. Load order position comparison is now
  correctly case-insensitive.

Changed
-------

- Improved load order sorting performance.
- Updated loot-condition-interpreter to v2.0.0.

0.14.7 - 2019-06-13
===================

Fixed
-----

- Filename comparisons on Windows now has the same locale-invariant case
  insensitivity behaviour as Windows itself, instead of being locale-dependent.
- Filename comparisons on Linux now use ICU case folding to give
  locale-invariant results that are much closer to Windows' case insensitivity,
  though still not identical.

Changed
-------

- Updated libgit2 to v0.28.2.

0.14.6 - 2019-04-24
===================

Added
-----

- Support for TES III: Morrowind using ``GameType::tes3``. The sorting process
  for Morrowind is slightly different than for other games, because LOOT cannot
  currently detect when plugins overlap. As a result, LOOT is much less likely
  to suggest load order changes.

Changed
-------

- Updated esplugin to v2.1.2.
- Updated loot-condition-interpreter to v1.3.0.

Fixed
-----

- LOOT would unnecessarily ignore intermediate plugins in a non-master to master
  cycle involving groups, leading to unexpected results when sorting plugins.

0.14.5 - 2019-02-27
===================

Changed
-------

- Updated libgit2 to v0.28.1.
- Updated libloadorder to v12.0.1.
- Updated spdlog to v1.3.1.

Fixed
-----

- ``HearthFires.esm`` was not recognised as a hardcoded plugin on case-sensitive
  filesystems, causing a cyclic interaction error when sorting Skyrim or Skyrim
  SE (via libloadorder).

0.14.4 - 2019-01-27
===================

Added
-----

- Added :cpp:any:`UnsetGroup()` to ``PluginMetadata``.

0.14.3 - 2019-01-27
===================

Changed
-------

- Condition parsing now errors if it does not consume the whole condition
  string. Via loot-condition-interpreter.
- Removed a few unhelpful log statements and changed the verbosity level of
  others.
- Updated loot-condition-interpreter to v1.2.2.

Fixed
-----

- Conditions were not parsed past the first instance of ``file(<regex>)``,
  ``active(<regex>)``, ``many(<regex>)`` or ``many_active(<regex>)``. Via
  loot-condition-interpreter.
- :cpp:any:`loot::CreateGameHandle()` could crash when trying to check if the
  given paths are symlinks. If a check fails, LOOT will assume the path is not a
  symlink.

0.14.2 - 2019-01-20
===================

Changed
-------

- Updated loot-condition-interpreter to v1.2.1.
- Updated spdlog to v1.3.0.

Fixed
-----

- An error when loading plugins with a file present in the plugins directory
  that has a filename containing characters that cannot be represented in the
  system code page.
- An error when trying to read the version of an executable that does not have
  a US English version information resource. Executable versions are now read
  from the file's first version information resource, whatever its language.
  Via loot-condition-interpreter.

0.14.1 - 2018-12-23
===================

Changed
-------

- Updated loot-condition-interpreter to v1.2.0.

Fixed
-----

- Product version conditions read from executables' ``VS_FIXEDFILEINFO``
  structure, so the versions read did not match the versions displayed by
  Windows' File Explorer. Product versions are now read from executables'
  ``VS_VERSIONINFO`` structure, using the ``ProductVersion`` key. Via
  loot-condition-interpreter.
- The release date in the metadata syntax changelog for v0.14 was "Unreleased".

0.14.0 - 2018-12-09
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
- ``UndefinedGroupError::getGroupName()`` has been renamed to
  ``UndefinedGroupError::GetGroupName()`` for consistency with other API method
  names.
- ``LootVersion::string()`` has been renamed to
  ``LootVersion::GetVersionString()`` for consistency with other API method
  names.
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
