************
Introduction
************

The metadata syntax is what LOOT's masterlists and userlists are written in. If you know YAML, good news: the syntax is essentially just YAML 1.2. If you don't know YAML, then its `Wikipedia page <https://en.wikipedia.org/wiki/YAML>`_ is a good introduction. All you really need to know is:

* How lists and associative arrays (key-value maps) are written.
* That whitespace is important, and that only normal spaces (ie. no non-breaking spaces or tabs) count as such.
* That data entries that are siblings must be indented by the same amount, and child data nodes must be indented further than their parents (see the example later in this document if you don't understand).
* That YAML files must be written in a Unicode encoding.
* That each key in a key-value map must only appear once per map object.

An important point that is more specific to how LOOT uses YAML:

* Strings are case-sensitive, apart from file paths, regular expressions and checksums.
* File paths are evaluated relative to the game's Data folder.
* File paths cannot reference a path outside of the game's folder structure, ie. they cannot contain the substring ``../../``.

In this document, where a value's type is given as ``X list`` this is equivalent to a YAML sequence of values which are of the data type ``X``. Where a value's type is given as ``X set``, this is equivalent to a YAML sequence of **unique** values which are of the data type ``X``. Uniqueness is determined using the equality criteria for that data type. All the non-standard data types that LOOT's metadata syntax uses have their equality criteria defined later in this document.

Some strings are interpreted as `CommonMark`_: where this is the case, the strings are interpreted according to version ``0.29`` of the specification.

.. _CommonMark: https://spec.commonmark.org/0.29/
