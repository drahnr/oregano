# oregano - an electrical engineering tool

[![Join the chat at https://gitter.im/drahnr/oregano](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/drahnr/oregano?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Stories in Ready](https://badge.waffle.io/drahnr/oregano.svg?label=ready&title=Ready)](http://waffle.io/drahnr/oregano) 

### About
oregano is an application for schematic capture and simulation of electronic circuits. The actual simulation is performed by Berkeley Spice, GNUcap or the new generation ngspice.
oregano is licensed under the terms of the [GNU GPL-2.0](http://www.gnu.org/licenses/gpl-2.0.html) included in the
file COPYING.

### Status

Build matrix:

OS           | Build Status
------------ | ------------------
Fedora 22    | [![Build Status](http://drone.ahoi.io/api/badges/drahnr/oregano/status.svg)](http://drone.ahoi.io/drahnr/oregano)
Ubuntu 12.04 | [![Build Status](https://travis-ci.org/drahnr/oregano.png?branch=master)](https://travis-ci.org/drahnr/oregano)  

The overall status should still be considered `unstable` since the process of re-working core components has yet to be completed.

### Donations

Donations are welcome!  
[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=drahnr&url=https://github.com/drahnr/oregano&title=oregano&language=&tags=github&category=software)

### Support

The preferred way of supporting oregano is by sending patches and pull requests or filing bug reports.

----

### Quick install guide

#### Repositories

If you are not going to build it yourself from source (which in fact is quite straightforward) you can grab a either a ready to install packages or distribution specific recipes:

* Fedora - [stable and git](https://copr.fedoraproject.org/coprs/drahnr/oregano/) or via `dnf copr enable drahnr/oregano`
* ArchLinux - [stable](https://aur4.archlinux.org/packages/oregano/), [git](https://aur4.archlinux.org/packages/oregano/)

#### Requirements

You need `gtk+-3.0`, `glib-2.0`, `gio-2.0`, `gtksourceview-3.0`, `goocanvas-2.0`, `libxml2` and `intltool` in order to build oregano.
These are usually included in your favorite distributions repositories and can otherwise be found at the [gnome public ftp](ftp://ftp.gnome.org) server.
In order to simulate a schematic you need either `ngspice` or `gnucap`.

If you are running a recent `Fedora` or `Ubuntu`, you can simply use `su -c'./waf builddeps'` to do that automatically.

#### Building

To build the oregano application issue the following for a debug build

    ./waf configure debug -j

or the following for a release build

    ./waf configure release -j

For additional options like specifying the install directory, consult

    ./waf --help

Note that additional options can be passed to the `configure` stage, i.e. `waf configure --prefix="/usr" debug -j6` is commonly used.

**Attention!**  
If you install oregano to a different prefix than `/usr`, `/usr/local` keep in mind that the `GSettings` schema will be installed under `${PREFIX}/shared/glib-2.0/schemas/`, which will not be checked by default. So you need to export the schema location appropriately via `export XDG_DATA_DIRS=/usr/local/share:/usr/share:${HOME}/.local/share:${PREFIX}/share` before launching oregano, see [xdg basedir spec](http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html) for further details.  
Omitting the `--prefix=..` option results in `/usr/local` as prefix, which works just fine.

#### Installation

For installation to the default directories, it is usually required to run

    ./waf install

with root privileges.

After installation oregano can be started by running `oregano` from terminal.

#### Removal

    ./waf uninstall

**Attention!**  
On subsequent installs with different prefixes this will only remove the last install!

----

### Contributions

are very welcome! We provide `TODO`,`ARCHITECTURE.md`, `HACKING.md` and the files under `docs/*` as a starting point, an overview that should help you going.
If you want to discuss an issue or something you would like to implement, don't be shy, drop a message to [gitter.im](https://gitter.im/drahnr/oregano)

#### Packaging

This repo also tracks packaging information for fedora (which should also be used for RedHat and CentOS, `oregano.spec`), Ubuntu (and thus Debian unstable, see the `debian` subdir) and soon to come for Mac (`macports` only contains a draft right now). If you see the need for more platforms we'd be happy to include even more.

#### Bugs

For bug and issue tracking as well as feature requests, the github built in issue tracker plus [waffle.io](https://waffle.io/drahnr/oregano)

#### Translations

Translators are welcome to translate at transifex which will be synced into the git repository a day before a new release is created (see the release milestones for planned release dates)

**Attention!**  
Currently translations are out of sync and will stay so until the next stable release since many error and ui strings are in the process of being unified.
