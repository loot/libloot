*****************
C++ API Reference
*****************

.. contents::
  :local:

String Encoding
===============

* All output strings are encoded in UTF-8.
* Input strings are expected to be encoded in UTF-8.

Errors
======

All errors encountered are thrown as exceptions that inherit from
``std::exception``.

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

.. doxygenclass:: loot::PluginNotLoadedError
   :members:

.. doxygenclass:: loot::UndefinedGroupError
   :members:
