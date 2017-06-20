Simulation Settings
===================


Why simulation settings?
------------------------

Before you can start a simulation, you have to tell the simulation engine
(ngspice or gnucap) things like

- type of simulation (time domain, frequency domain, DC sweep, AC, Noise, ...)
- simulation dependant parameters (for time domain: start, stop, step, ...)
- ...


How to open the dialog
----------------------

1. Option: Click the gearwheel "Edit the simulation settings" in the toolbar.
Don't confuse the single gearwheel button with the three gearwheel button
("Run a simulation").
2. Option: Click "Edit"->"Simulation Settings..." in the menubar.


Analysis Parameters
-------------------

**Transient** (time domain)

- Use Initial Conditions:
	for some parts you can define the initial state of the part, for example
	the initial current of an inductor or the initial voltage of a capacitor
- Analyze All Currents And Potentials:
	The voltage markers will have no effect. As much as possible voltages and currents
	will be recorded and displayed. THIS FEATURE IS CURRENTLY WORKING ONLY WITH
	NGSPICE AS SIMULATION ENGINE.
- Start:
	Start time from which the data should be recorded.
- Stop:
	simulation end time
- Step:
	Maximum time step size. If set correctly, it can be a good hint for the simulation
	engine and the simulation will run fluently and without errors.


**DC Sweep**

- Source: the indipendent voltage source.
- Output: the voltage output variable.
- Start: the starting voltage (sweep from).
- Stop: the ending voltage (sweep to).
- Step: voltage increment.


**AC**

- Output: the voltage output variable.
- Type: the scale type (decade, linear or octave variation).
- Points: the number of points in the simulation (from start to stop).
- Start: the starting frequency.
- Stop: the final frequency.


**Fourier**

//TODO


**Noise**

- Source: the indipendent voltage source.
- Output: the voltage output variable.
- Type: the scale type (decade, linear or octave variation).
- Points: the number of points in the simulation (from start to stop).
- Start: the starting frequency.
- Stop: the final frequency.

At the moment, it works only with the ngspice engine.

