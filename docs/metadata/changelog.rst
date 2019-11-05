***************
Version History
***************

The version history of the metadata syntax is given below.

0.15 - 2019-11-05
=================

Added
-----

- The condition function ``is_master(file_path path)``, which checks if the
  given file is an installed master plugin.

0.14 - 2018-12-09
=================

Added
-----

- The ``Group`` data structure now has a ``description`` key that takes a string
  value.
- The condition function ``product_version(file_path path, version
  given_version, comparison_operator comparator)``, which checks against the
  Product Version field of an executable.

Changed
-------

- ``clean`` and ``dirty`` metadata are now allowed in regex plugin entries.
- ``Location``, ``Message``, ``MessageContent`` and ``Tag`` equality comparisons
  are now case-sensitive.
- Regular expressions in condition strings now use a `modified Perl grammar`_
  instead of a modified ECMAScript grammar. ``Plugin`` object ``name`` fields
  still use the modified ECMAScript grammar for regex values. To improve
  portability and avoid mistakes, it's best to stick to using the subset of
  regular expression features that are common to both grammars.

Removed
-------

- The change in regular expression grammar means that the following regular
  expression features are no longer supported in condition strings:

  - ``\c<letter>`` control code escape sequences, use ``\x<hex>`` instead
  - The ``\0`` null escape sequence, - use ``\x00`` instead
  - The ``[:d:]``, ``[:w:]`` and ``[:s:]`` character classes,
    use ``[:digit:]``, ``[:alnum:]`` and ``[:space:]`` instead respectively.
  - ``\<number>`` backreferences
  - ``(?=<subpattern>)`` and ``(?!<subpattern>)`` positive and negative lookahead

.. _modified Perl grammar: https://docs.rs/regex/1.0.5/regex/index.html#syntax

0.13 - 2018-04-02
=================

Added
-----

- The ``Group`` data structure.
- The ``groups`` list to the root of the metadata file format.
- The ``group`` key to the plugin data structure.

Removed
-------

- The ``priority`` field from the plugin data structure.
- The ``global_priority`` field from the plugin data structure.

0.10 - 2016-11-06
=================

Added
-----

* The ``clean`` key to the plugin data structure.
* The ``global_priority`` field to the plugin data structure.
* The ``many_active()`` condition function.
* The ``info`` key to the cleaning data structure.

Changed
-------

* Renamed the ``str`` key in the localised content data structure to ``text`` .
* The ``priority`` field of the plugin data structure now stores values between -127 and 127 inclusive.
* Regular expressions no longer accept ``\`` as a directory separator: ``/`` must now be used.
* The ``file()`` condition function now also accepts a regular expression.
* The ``active()`` condition function to also accept a regular expression.
* Renamed the dirty info data structure to the cleaning data structure.

Removed
-------

* The ``regex()`` condition function, as it has been obsoleted by the ``file()`` function's new regex support.

0.8 - 2015-07-22
================

Added
-----

* The ``name`` key to the location data structure.
* The ``many("regex")`` condition function.
* The documentation now defines the equality criteria for all of the metadata syntax's non-standard data structures.

Changed
-------

* Detection of regular expression plugin entries. Previously, a plugin entry was treated as having a regular expression filename if the filename ended with ``\.esp`` or ``\.esp`` . Now, a plugin entry is treated as having a regular expression filename if the filename contains one or more of ``:\*?|`` .

Removed
-------

* Removed the ``ver`` key in the location data structure.

Fixed
-----

* The documentation gave the values of the ``after`` , ``req`` , ``inc`` , ``tag`` , ``url`` and ``dirty`` keys as lists, when they have always been sets.

0.7 - 2015-05-20
================

Added
-----

* The message string substitution key, i.e. ``sub`` , in the message data structure.
* Support for YAML merge keys, i.e. ``<<`` .

Changed
-------

* Messages may now be formatted using most of GitHub Flavored Markdown, minus the GitHub-specific features (like @mentions, issue/repo linking and emoji).

0.6 - 2014-07-05
================

No changes.

0.5 - 2014-03-31
================

Initial release.
