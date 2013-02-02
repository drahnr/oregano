# oregano - an electrical engineering tool

### About
oregano is an application for schematic capture and simulation of
electronic circuits. The actual simulation is performed by Berkeley
Spice, GNUcap or the new generation ngspice.

oregano is licensed under the terms of the GNU GPL-2.0 included in the
file COPYING.

### Requirements

You need `gtk+-3.0`, `gtksourceview-3.0`, `goocanvas-2.0` and `libxml2` in order to build oregano.
These are usually included in your favorite distributions repositories and can otherwise be found at the [gnome public ftp](ftp://ftp.gnome.org) server.

### Building

To build the oregano application issue the following for a debug build

    waf configure
    waf debug

or the following for a release build

    waf configure
    waf release

For additional options, consult

    waf --help

### Installation

For installation to the default directories, it is usually required to run

    waf install

with root privileges.

After installation oregano can be started by running `oregano.bin` from terminal.

### Web

The [official website is on github](https://github.com/drahnr/oregano).

### Bugs

For bug and issue tracking as well as feature requests, the github built in [issue tracker](https://github.com/drahnr/oregano/issue) is used.


