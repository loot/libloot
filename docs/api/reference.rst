*************
API Reference
*************

.. contents::

Constants
=========

.. doxygenvariable:: loot::LIBLOOT_VERSION_MAJOR

.. doxygenvariable:: loot::LIBLOOT_VERSION_MINOR

.. doxygenvariable:: loot::LIBLOOT_VERSION_PATCH

Enumerations
============

.. doxygenenum:: loot::EdgeType

.. doxygenenum:: loot::GameType

.. doxygenenum:: loot::LogLevel

.. doxygenenum:: loot::MessageType

Functions
=========

.. doxygenfunction:: loot::SetLoggingCallback

.. doxygenfunction:: loot::SetLogLevel

.. doxygenfunction:: loot::IsCompatible

.. doxygenfunction:: loot::CreateGameHandle

.. doxygenfunction:: loot::GetLiblootVersion

.. doxygenfunction:: loot::GetLiblootRevision

.. doxygenfunction:: loot::SelectMessageContent

Interfaces
==========

.. doxygenclass:: loot::DatabaseInterface
   :members:

.. doxygenclass:: loot::GameInterface
   :members:

.. doxygenclass:: loot::PluginInterface
   :members:

Classes
=======

.. doxygenclass:: loot::ConditionalMetadata
   :members:

.. doxygenclass:: loot::Filename
   :members:

.. doxygenclass:: loot::File
   :members:

.. doxygenclass:: loot::Group
   :members:

.. doxygenclass:: loot::Location
   :members:

.. doxygenclass:: loot::MessageContent
   :members:

.. doxygenclass:: loot::Message
   :members:

.. doxygenclass:: loot::PluginCleaningData
   :members:

.. doxygenclass:: loot::PluginMetadata
   :members:

.. doxygenclass:: loot::Tag
   :members:

.. doxygenclass:: loot::Vertex
   :members:

Exceptions
==========

.. doxygenclass:: loot::CyclicInteractionError
   :members:

.. doxygenclass:: loot::ConditionSyntaxError
   :members:

.. doxygenclass:: loot::FileAccessError
   :members:

.. doxygenclass:: loot::UndefinedGroupError
   :members:

Error Categories
================

LOOT uses error category objects to identify errors with codes that originate in
lower-level libraries.

.. doxygenfunction:: loot::esplugin_category

.. doxygenfunction:: loot::libloadorder_category
