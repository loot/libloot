*****************
Condition Strings
*****************

Condition strings can be used to ensure that data is only acted on by LOOT under certain circumstances. They are very similar to boolean conditional expressions in programming languages such as Python, though more limited.

Omitting optional parentheses (see below), their `EBNF`_ grammar is:

.. productionlist::
  expression: condition, { logical_or, compound_condition }
  compound_condition: condition, { logical_and, condition }
  condition: ( [ logical_not ], function ) | ( [ logical_not ], "(", expression, ")" )
  logical_and: "and"
  logical_or: "or"
  logical_not: "not"

.. _EBNF: https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_Form

Spaces, tabs, carriage returns and line feeds (i.e. line breaks) can be used on either side of:

- ``logical_and``, as defined above
- ``logical_or``, as defined above
- ``logical_not``, as defined above
- parentheses around conditions and expressions
- commas separating function arguments

Types
=====

.. describe:: filesystem_path

  A double-quoted filesystem path.

.. describe:: file_path

  A double-quoted file path.

.. describe:: file_size

  A string of decimal digits representing an unsigned integer number of bytes.

.. describe:: regular_expression

  A double-quoted file path, with a regular expression in place of a filename. The path must use ``/`` for directory separators, not ``\``. The regular expression must be written in a `modified Perl <https://docs.rs/regex/1.0.5/regex/index.html#syntax>`_ syntax.

  Only the filename path component will be evaluated as a regular expression. For example, given the regex file path ``Meshes/Resources(1|2)/(upperclass)?table.nif``, LOOT will look for a file named ``table.nif`` or ``upperclasstable.nif`` in the ``Meshes\Resources(1|2)`` folder, rather than looking in the ``Meshes\Resources1`` and ``Meshes\Resources2`` folders.

.. describe:: checksum

  A string of hexadecimal digits representing an unsigned integer that is the data checksum of a file. LOOT displays the checksums of plugins in its user interface after running.

.. describe:: version

  A double-quoted string of characters representing the version of a plugin or executable. LOOT displays the versions of plugins in its user interface after running.

.. describe:: comparison_operator

  One of the following comparison operators.

  .. describe:: ==

    Is equal to

  .. describe:: !=

    Is not equal to

  .. describe:: <

    Is less than

  .. describe:: >

    Is greater than

  .. describe:: <=

    Is less than or equal to

  .. describe:: >=

    Is greater than or equal to

Functions
=========

There are several conditions that can be tested for using the functions detailed below. All functions return a boolean. For functions that take a path or regex, the argument is treated as regex if it contains any of the characters ``:\*?|``.

.. describe:: file(filesystem_path path)

  Returns true if ``path`` is installed, and false otherwise.

.. describe:: file(regular_expression regex)

  Returns true if a file matching ``regex`` is found, and false otherwise.

.. describe:: file_size(file_path path, file_size size)

  Returns true if the file at the given ``path`` has the given ``size``, and
  false otherwise.

.. describe:: readable(filesystem_path path)

  Returns true if ``path`` is a readable directory or file, and false otherwise.

  This is particularly useful when writing conditions for games that are available from the Microsoft Store and/or Xbox app, as games installed using them have executables that have heavily restricted permissions, and attempts to read them result in permission denied errors. You can use this function to guard against such errors by calling it before the ``checksum``, ``version`` or ``product_version`` functions.

.. describe:: active(file_path path)

  Returns true if ``path`` is an active plugin, and false otherwise.

.. describe:: active(regular_expression regex)

  Returns true if an active plugin matching ``regex`` is found, and false otherwise.

.. describe:: many(regular_expression regex)

  Returns true if more than one file matching ``regex`` is found, and false otherwise.

.. describe:: many_active(regular_expression regex)

  Returns true if more than one active plugin matching ``regex`` is found, and false otherwise.

.. describe:: is_master(file_path path)

  Returns true if ``path`` is an installed master plugin, and false otherwise. This returns false for all OpenMW plugins, as OpenMW does not force master plugins to load before others.

.. describe:: is_executable(file_path path)

  Returns true if ``path`` is a Windows executable (PE) file, and false
  otherwise.

.. describe:: checksum(file_path path, checksum expected_checksum)

  Returns true if the calculated CRC-32 checksum of ``path`` matches ``expected_checksum``, and false otherwise. Returns false if ``path`` does not exist.

.. describe:: version(file_path path, version given_version, comparison_operator comparator)

  Returns true if the boolean expression::

    actual_version comparator given_version

  (where ``actual version`` is the version read from ``path``) holds true, and
  false otherwise.

  * If ``path`` is a plugin, its version is read from its description field.
  * If ``path`` is not a plugin, it will be assumed to be an executable (e.g.
    ``*.exe`` or ``*.dll``), and its version is read from its File Version field.
  * If ``path`` does not exist or does not have a version number, the condition
    evaluates to true for the ``!=``, ``<`` and ``<=`` comparators, i.e. a
    missing version is always less than the given version.
  * If ``path`` is not readable or is not a plugin or an executable, an error
    will occur.

  The supported version syntax and precedence rules are detailed in the section
  below.

.. describe:: product_version(file_path path, version given_version, comparison_operator comparator)

  Returns true if the boolean expression::

    actual_version comparator given_version

  (where ``actual version`` is the version read from ``path``) holds true, and
  false otherwise. ``path`` must be an executable (e.g. ``*.exe`` or ``*.dll``),
  and its version is read from its Product Version field.

  * If ``path`` does not exist or does not have a version number, the condition
    evaluates to true for the ``!=``, ``<`` and ``<=`` comparators, i.e. a
    missing version is always less than the given version.
  * If ``path`` is not a readable executable, an error will occur.

  The supported version syntax and precedence rules are detailed in the section
  below.

.. describe:: filename_version(regular_expression path, version given_version, comparison_operator comparator)

  The regex in ``path`` must contain a single capturing group.

  Returns true if a file matching ``path`` is found for which the boolean
  expression::

    actual_version comparator given_version

  (where ``actual_version`` is the value captured by the regex) holds true, and
  false otherwise.

  Unlike the other version functions, it always returns false if it cannot find
  a version to compare against the given version, irrespective of the given
  comparison operator.

.. describe:: description_contains(file_path path, regular_expression regex)

  Returns true if ``path`` is a plugin file with a description that contains
  text that matches ``regex``, and false otherwise (including if the path does
  not exist, is not a plugin, or has no description).

Version Syntax & Comparison Rules
---------------------------------

Version parsing and comparison is compatible with
`Semantic Versioning <http://semver.org/>`_, with the following exceptions:

* Pre-release identifiers may not include hyphens (``-``), as they are treated
  as separators. For example, a SemVer-compliant parser would treat
  ``1.0.0-alpha.1.x-y-z.--`` as ``([1, 0, 0], ["alpha", 1, "x-y-z", "--"])`` but
  libloot treats it as ``([1, 0, 0], ["alpha", 1, "x", "y", "z", "", ""])``.
* Identifiers that contain non-digit characters are lowercased before being
  compared lexically, so that their comparison is case-insensitive instead of
  case-sensitive. For example, SemVer specifies that ``1.0.0-alpha`` is greater
  than ``1.0.0-Beta``, but libloot compares them with the opposite result.

These exceptions are necessary to support an extended range of real-world
versions that do not conform to SemVer. The supported extensions are:

* Leading zeroes are allowed and ignored in major, minor and patch version
  numbers and numeric pre-release IDs. For example, ``01.02.03`` and ``1.2.3``
  are equal.
* An arbitrary number of version numbers is allowed. To support this, the major,
  minor and patch version numbers are treated as a sequence of numeric release
  IDs, and any subsequent version numbers are just additional release IDs that
  get appended to the sequence. For example, ``1.2.3`` may be represented as the
  sequence ``[1, 2, 3]``, and ``1.2.3.4`` would be represented as
  ``[1, 2, 3, 4]``.

  If two versions with a different number of release identifiers are compared,
  the version with fewer release identifiers is padded with zero values until
  they are the same length. Each release identifier in one version is then
  compared against the release identifier in the same position in the other
  version. For example, ``1-beta`` is padded to ``1.0.0-beta`` before being
  compared against ``1.0.1-beta``, and the result is that ``1.0.1-beta`` is
  greater than ``1-beta``.
* Release IDs may be separated by a period (``.``) or a comma (``,``). For
  example, ``1.2.3.4`` and ``1,2,3,4`` are equal.
* The separator between release IDs and pre-release IDs may be a hyphen (``-``),
  a space (" "), a colon (``:``) or an underscore (``_``). For example,
  ``1.2.3-alpha``, ``1.2.3 alpha``, ``1.2.3:alpha`` and ``1.2.3_alpha`` are all
  equal.
* Pre-release IDs may be separated by a period (``.``), a hyphen (``-``), a
  space (" "), a colon (``:``) or an underscore (``_``). For example,
  ``1.2.3-alpha.1``, ``1.2.3-alpha-1``, ``1.2.3-alpha 1``, ``1.2.3-alpha:1`` and
  ``1.2.3-alpha_1`` are all equal.
* Non-numeric release IDs are allowed. A non-numeric release ID may contain any
  character (not just ASCII characters) that is not one of the separators listed
  above or a plus sign (``+``). For example, ``0.78b.1`` is allowed.

  Non-numeric release IDs use the same comparison rules as non-numeric
  pre-release IDs, with the exception that a non-numeric release ID is not
  always greater than a numeric release ID:

  * If the non-numeric release ID has no leading digits, it is greater than the
    numeric release ID. For example, ``1.A`` is greater than ``1.1``.
  * If the non-numeric release ID has leading digits, they are parsed as a
    number, and this is compared against the numeric release ID:

    * If the two numbers are equal then the non-numeric release ID is greater
      than the numeric release ID. For example, ``1.1A`` is greater than
      ``1.1``.
    * Otherwise, the result of comparing the two numbers is used as the result
      of comparing the two release IDs. For example, ``1.2`` is greater than
      ``1.1A`` and ``1.1A`` is greater than ``1.0``.

* Pre-release IDs may contain any character (not just ASCII characters) that is
  not one of the pre-release ID separators listed above or a plus sign (``+``).
* Before non-numeric IDs (release or pre-release) are compared, they are
  lowercased according to Unicode's lowercasing rules.
* As a special case, version strings that are four comma-and-space-separated
  sequences of digits are interpreted as if the comma-and-space separators were
  periods (``.``). For example, ``0, 2, 0, 12`` and ``0.2.0.12`` are equal.

Logical Operators
=================

The ``and``, ``or`` and ``not`` operators have their usual definitions.

Order of Evaluation
-------------------

Condition strings are evaluated according to the usual C-style operator precedence rules, and parentheses can be used to override these rules. For example::

  function and function or not function

is evaluated as::

  ( function and function ) or ( not function )

but::

  function and ( function or not function )

is evaluated as::

  function and ( function or ( not function ) )

Performance
===========

LOOT caches the results of condition evaluations. A regular expression check will still take longer than a file check though, so use the former only when appropriate to do so.
