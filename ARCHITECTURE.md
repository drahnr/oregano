## ARCHITECTURE

This document is targeted at oregano developers, giving a brief overview how all pieces connect together.

### Overview

The central elements are the `schematic`, `sheet`, `schemtic-view`, `node-store`, `engine`.

The `schematic-view` displays a schematic. The `schematic-view` is one oregano window including all its UI elements.
The most important child element is the `sheet`, a canvas drawing all `*-item`s.

The `schematic` is an abstract object saving a set of strings for simulation, `node-store`, authorship and a bunch of others.

The `node-store` saves all `node`s, `wire`s, `part`s (they are all derived from `item-data`). This is the *store* part of one `schematic-view`.

For actual simulation an `engine` object is used to transform the data contained in `schematic` and `node-store` to a string being piped to the actual backend engine.

The `sheet` handles mouse interaction, triggers the creation of new `sheet-item`s (like `wire-item`,`textbox-item`, etc.). These hold a pointer of type `item-data` to the actual object data, dependant of the subclass this is a `wire`, `node` or `part` (derived classes from `item-data`). The `*-item` objects exclusive task is to handle input events, creating context menus for that item, handling events when clicking these items, handling selection as well as moving of objects. This is the *view* part of one `schematic-view`.

Note that all these objects are local to the `schematic-view`. This is required to keep it reentrant and thus run multiple windows within one process.

### Issues

The above layout is quite complex and currently there are some issues which require new `item-data` derived classes be added _first_ to the schematic and _afterwards_ being set to the proper position.
Also there is a total lack of documentation within the sourcecode which is currently being addressed.
