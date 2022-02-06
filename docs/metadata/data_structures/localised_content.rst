Localised Content
=================

The localised content data structure is a key-value string map.

.. describe:: text

  **Required.** The CommonMark message content string.

.. describe:: lang

  **Required.** The language that ``text`` is written in, given as a code of the form ``ll`` or ``ll_CC``, where ``ll`` is an ISO 639-1 language code and ``CC`` is an ISO 3166 country code. For example,

  ====================  =====
  Language              Code
  ====================  =====
  Bulgarian             bg
  Chinese (Simplified)  zh_CN
  Czech                 cs
  Danish                da
  English               en
  Finnish               fi
  French                fr
  German                de
  Italian               it
  Japanese              ja
  Korean                ko
  Polish                pl
  Portuguese            pt_PT
  Portuguese (Brazil)   pt_BR
  Russian               ru
  Spanish               es
  Swedish               sv
  Ukrainian             uk_UA
  ====================  =====

Equality
--------

Two localised content data structures are equal if all their fields are equal.
Field equality is case-sensitive.
