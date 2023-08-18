***********************
Metadata File Structure
***********************

The root of a metadata file is a key-value map. LOOT will recognise the following keys, none of which are required. Other keys may also be present, but are not processed by LOOT.

.. describe:: prelude

  The prelude can have any value, but if a masterlist prelude path is provided when loading metadata, the masterlist's ``prelude`` value will be replaced by the parsed content of the masterlist prelude file. The prelude exists so that metadata that is common across different masterlists can be shared without duplication.

  Note that prelude replacement is only supported when using YAML's block style and an unquoted ``prelude`` key that is not preceded by a mapping key indicator and that is immediately followed by a colon separator, i.e. ``prelude:``.

.. describe:: bash_tags

  string list

  A list of Bash Tags that are supported by the game. These Bash Tags are used to provide autocomplete suggestions in LOOT's metadata editor.

.. describe:: globals

  message list

  A list of message data structures for messages that are displayed independently of any plugin.

.. describe:: groups

  group set

  A set of group data structures that represent the groups that plugins can belong to.

.. describe:: plugins

  plugin list *and* plugin set

  The plugin data structures that hold all the plugin metadata within the file. It is a mixture of a list and a set because **no non-regex plugin value may be equal to any other non-regex plugin value** , but there may be any number of equal regex plugin values, and non-regex plugin values may be equal to regex plugin values. If multiple plugin values match a single plugin, their metadata is merged in the order the values are listed, and as defined in :ref:`plugin-merging`.

The message and plugin data structures are detailed in the next section.

Example
=======

.. code-block:: yaml

  prelude:
    - &thanksForUsing
      type: say
      content: 'Thanks for using LOOT!'

  bash_tags:
    - 'C.Climate'
    - 'Relev'

  globals:
    - *thanksForUsing

  groups:
    - name: 'Map Markers'
      after:
        - 'default'

  plugins:
    - name: 'Armamentarium.esm'
      tag:
        - Relev
    - name: 'ArmamentariumFran.esm'
      tag:
        - Relev
    - name: 'Beautiful People 2ch-Ed.esm'
      tag:
        - Eyes
        - Graphics
        - Hair
        - R.Relations
    - name: 'More Map Markers.esp'
      group: 'Map Markers'
