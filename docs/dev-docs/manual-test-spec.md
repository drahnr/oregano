Manual Test Specification
=========================


About this file
---------------

This file describes how to test parts of the oregano system that
can not be tested automatically if an official release is planned.

Progress bar
------------

The progress bar should move continuously (nearly linear) under the
following conditions:

- ngspice as simulation engine AND
- transient analysis AND (
- ngspice active OR
- transient analysis parser active )

The ngspice progress bar should not be combined with the transient
analysis progress bar because the speed of the two processes differ
and depend highly on the executing hardware.

Cancel button
-------------

The cancel button, that cancels the simulation calculation, should
work under the following conditions:

- ngspice as simulation engine AND
- transient analysis AND (
- ngspice calculates OR
- ngspice prints results to stdout OR
- ngspice finished printing and only parser is working )

This means that the cancel button should work at any time if the
simulation engine is ngspice. These 3 states are identifiable.
The first state (ngspice calculates) begins at 0% of the ngspice
progress bar and ends at 100% of the ngspice progress bar. The
second state begins at 0% of the transient analysis progress bar
and ends if the progress bar speeds up a little. The third state
begins when the second state ends and ends at 100% of the transient
analysis progress bar.

Expected behavior:

- All (additional) threads should terminate normally.
- ngspice should terminate immediately without insubordination.
- The whole canceling process should not take longer than 100ms.
- There should not be printed any errors or warnings in stdout
of oregano.
- There should be printed an information message in the user log.
- No memory leaks.

The following cases are unspecified:

- ngspice prints many errors + user cancels => show abortion log
window?