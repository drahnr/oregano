# oregano - An electrical engineering tool

### About
oregano is an application for schematic capture and simulation of
electronic circuits. The actual simulation is performed by Berkeley
Spice, or GNUcap or the new generation ngspice.

oregano is licensed under the terms of the GNU GPL included in the
file COPYING.

### Requirements

You need gtk+-3.0 >= 3.0.1, gtksourceview-3.0, goocanvas-2.0 and libxml2 in order to build  oregano. 
They are usually included in your favorite distributions repositories and can otherwise be found at ftp.gnome.org or mirrors.

### Building

To build the oregano application issue the following for a debug build

    waf configure
    waf debug

or the following for a release build

    waf configure
    waf release

For additional options, consult

    waf configure --help

### Web

Currently there are multiple forks at github, development will be streamlined as soon as possible in order to speed up development

https://github.com/drahnr/oregano
