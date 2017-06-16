Progress Bar
============


Why progress bar?
-----------------

If a simulation runs over a long period of time or
has a high resolution in time, the simulation time
increases. Then a progress information is integral,
because the user has to estimate if the simulation
will run too long and if he should simplify the
simulation parameters or the circuit.

How to start progress bar
-------------------------

Just press the "Run a simulation" button in the main
toolbar. The progress bar works guaranteed in the time
simulation (transient) and simulation engine ngspice.

How to interpret the progress info
----------------------------------

The simulation consists of mainly two parts:

1. solve simulation with simulation engine
2. transform data of simulation engine to displayable
format

The progress panel consists of two progress bars. One
progress bar displays the progress information of the
simulation engine, the other progress bar displays the
progress information of data transformation. Empirically
the transformation time is shorter than the simulation
time.

Cancel button
-------------

The cancel button cancels the simulation. All simulation
data will be lost. It is guaranteed to work in transient
analysis with ngspice.