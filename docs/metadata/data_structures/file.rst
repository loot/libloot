File
====

This structure can be used to hold file paths. It has two forms: a key-value string map and a scalar string.

Map Form
--------

.. describe:: name

  **Required.** An exact (ie. not regex) file path or name.

.. describe:: display

  A CommonMark string, to be displayed instead of the file path in any generated messages, eg. the name of the mod the file belongs to.

.. describe:: detail

  ``string`` or ``localised content list``

  if this file causes an error message to be displayed (e.g. because it's a
  missing requirement), this detail message content will be appended to that
  error message. If a string is provided, it will be interpreted as CommonMark.
  If a localised content list is provided, one of the structures must be for
  English. If undefined, defaults to an empty string.

.. describe:: condition

  A condition string that is evaluated to determine whether this file data should be used: if it evaluates to true, the data is used, otherwise it is ignored. See :doc:`../conditions` for details.

.. describe:: constraint

  A condition string that must also evaluate to true for the file's existence to be recognised. See :doc:`../conditions` for details.

  When a constraint is set, it's best to also set a ``display`` value that describes the constraint so that it is visible to users if a message is displayed for the file.

Scalar Form
-----------

The scalar form is simply the value of the map form's ``name`` key. Using the scalar form is equivalent to using the map form with undefined ``display``, ``detail``, ``condition`` and ``constraint`` keys.

Equality
--------

Two file data structures are equal if all their fields are equal. ``name`` field
equality is case-insensitive, the other fields use case-sensitive equality.

Examples
--------

Scalar form::

  '../obse_loader.exe'

Map form::

  name: '../obse_loader.exe'
  condition: 'version("../obse_loader.exe", "0.0.18.0", >=)'
  display: 'OBSE v18+'
