Localised Content
=================

The localised content data structure is a key-value string map.

.. describe:: text

  **Required.** The actual message content string.

.. describe:: lang

  **Required.** The language that ``text`` is written in, given as a code of the form ``ll`` or ``ll_CC``, where ``ll`` is an ISO 639-1 language code and ``CC`` is an ISO 3166 country code. For example,

  ====================  =====
  Language              Code
  ====================  =====
  Brazilian Portuguese  pt_BR
  Chinese               zh_CN
  Danish                da
  English               en
  Finnish               fi
  French                fr
  German                de
  Korean                ko
  Polish                pl
  Russian               ru
  Spanish               es
  Swedish               sv
  ====================  =====

Equality
--------

Two localised content data structures are equal if all their fields are equal.
Field equality is case-sensitive.
