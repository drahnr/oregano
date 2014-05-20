# oregano - an electrical engineering tool

### About
oregano is an application for schematic capture and simulation of electronic circuits. The actual simulation is performed by Berkeley Spice, GNUcap or the new generation ngspice.
oregano is licensed under the terms of the GNU GPL-2.0 included in the
file COPYING.

### Status

[![Build Status](https://travis-ci.org/drahnr/oregano.png?branch=master)](https://travis-ci.org/drahnr/oregano)

The overall status should be considered `unstable`, as the process of fixing core components has yet to be completed.

### Support

[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=drahnr&url=https://github.com/drahnr/oregano&title=oregano&language=&tags=github&category=software)

Any donations will directly increase sparetime we spend to write code, fix bugs, write developer documentation – working towards a 21st century Oregano Electrical Engineering Tool which is worthwhile using.

----

### Quick install guide

#### Repositories

If you are not going to build it yourself from source (which in fact is quite straightforward) you can grab a either a ready to install packages or distribution specific recipes:

* Fedora - https://copr.fedoraproject.org/coprs/drahnr/oregano/
* ArchLinux - https://aur.archlinux.org/packages/oregano/

#### Requirements

You need `gtk+-3.0`, `glib-2.0`, `gio-2.0`, `gtksourceview-3.0`, `goocanvas-2.0`, `libxml2` and `intltool` in order to build oregano.
These are usually included in your favorite distributions repositories and can otherwise be found at the [gnome public ftp](ftp://ftp.gnome.org) server.
In order to simulate a schematic you need either `ngspice` or `gnucap`.


#### Building

To build the oregano application issue the following for a debug build

    ./waf configure
    ./waf debug -j

or the following for a release build

    ./waf configure
    ./waf release -j

For additional options like specifying the install directory, consult

    ./waf --help

Note that additional options can be passed to the `configure` stage, i.e. `waf configure --prefix="/usr" debug -j6` is commonly used.


#### Installation

For installation to the default directories, it is usually required to run

    ./waf install

with root privileges.

After installation oregano can be started by running `oregano` from terminal.

#### Removal

    ./waf uninstall


----

### Web

The [official oregano website](https://srctwig.com/project/oregano)

### Mailinglist

Mailinglist is at `oregano@librelist.com`, just drop a mail to subscribe. To unsubscribe again send a mail to `oregano-unsubscribe@librelist.com`.

As every mailinglist, we also expect appropriate behavior, respect and a positive manner.

----

### Contributions

are very welcome! We provide `TODO`,`ARCHITECTURE.md`, `HACKING.md` and the files under `docs/*` as a starting point, an overview that should help you going.
If you want to discuss an issue or something you would like to implement, don't be shy, drop a mail to the mailinglist (see above).


#### Bugs

For bug and issue tracking as well as feature requests, the github built in issue tracker is used.

#### Translations

Translators are welcome to translate at transifex which will be synced into the git repository a day before a new release is created (see the release milestones for planned release dates)
