LogView
=======


Introduction
------------

The LogView consists of 2 columns "Source" and "Message".
"Source" is the program or part of Oregano, that emits the "Message".
Don't confuse LogView with Log/Oregano Log Window that can be shown
by "View"->"Log" in the menubar.

Sometimes the messages are confusing. This file will help you to
interpret the messages.


Show/hide the LogView
----------------

You can show/hide the LogView by clicking the red exclamation mark button
in the toolbar.


Source = ngspice Error
----------------------

If the message begins with "Reference value", the log entry is not an
error but a progress information. From time to time ngspice spits out
the simulation time that is called "Reference value". ngspice prints
it to stderr, because ngspice is often called in console like
```
$ ngspice -b "/tmp/netlist.tmp" > "/tmp/netlist.lst"
```
and the progess information ("Reference value" - printed to stderr)
is still visible for the user in console.

