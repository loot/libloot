Message
=======

Messages are given as key-value maps.

.. describe:: type

  ``string``

  **Required.** The type string can be one of three keywords.

  .. describe:: say

    A generic message, useful for miscellaneous notes.

  .. describe:: warn

    A warning message, describing a non-critical issue with the user's mods (eg. dirty mods).

  .. describe:: error

    An error message, decribing a critical installation issue (eg. missing masters, corrupt plugins).

.. describe:: content

  ``string`` or ``localised content list``

  **Required.** Either simply a CommonMark string, or a list of localised content data structures. If the latter, one of the structures must be for English.

.. describe:: condition

  ``string``

  A condition string that is evaluated to determine whether the message should be displayed: if it evaluates to true, the message is displayed, otherwise it is not. See :doc:`../conditions` for details.

.. describe:: subs

  ``string list``

  A list of CommonMark strings to be substituted into the message content string. The content string must use numbered specifiers (``{0}``, ``{1}``, etc.), where the numbers correspond to the position of the substitution string in this list to use, to denote where these strings are to be substituted.

.. _languages:

Language Support
----------------

If a message's ``content`` value is a string, the message will use the string as
its content if displayed. Otherwise, the first localised content structure with
a language or locale code that matches LOOT's current language will be used as
the message's content if displayed. If there are no exact matches, LOOT will try
to find a close match.

If LOOT's current language uses a locale code, it will
display the first structure with the same language code, but not another locale
code with the same language code. For example, if LOOT's current language has
locale code ``pt_BR``, it will display the first structure with language code
``pt`` (but not locale code ``pt_PT``) if none exist with locale code ``pt_BR``.

If LOOT's current language has a language code, it will display the first
structure with a locale code that contains that language code. For example, if
LOOT's current language has language code ``pt``, it will display the first
structure with locale code ``pt_PT`` or ``pt_BR`` if none exist with language
code ``pt``.

If there are no exact or close matches, then the first structure in
English will be used.

Equality
--------

Two message data structures are equal if their `type`, `content` and `condition`
fields are equal, after any `subs` values have been substituted into `content`
strings. If the `content` field is a string, it is treated as a localised
content list containing a single English-language string. String equality is
case sensitive.

Examples
--------

.. code-block:: yaml

  type: say
  content:
    - lang: en
      text: 'An example link: <http://www.example.com>'
    - lang: zh_CN
      text: '一个例子链接: <http://www.example.com>'
  condition: 'file("foo.esp")'

would be displayed as

  * 一个例子链接: http://www.example.com

if the current language was Simplified Chinese and ``foo.esp`` was installed, while

.. code-block:: yaml

  type: say
  content: 'An alternative [example link](http://www.example.com), with no translations.'

would be displayed as

  * An alternative `example link <http://www.example.com>`_, with no translations.

In English,

.. code-block:: yaml

  type: say
  content: 'A newer version of {0} [is available]({1}).'
  subs:
    - 'this plugin'
    - 'http://www.example.com'

would be displayed as

  * A newer version of this plugin `is available <http://www.example.com>`_.
