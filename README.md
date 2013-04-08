# oregano - an electrical engineering tool

### About
oregano is an application for schematic capture and simulation of electronic circuits. The actual simulation is performed by Berkeley Spice, GNUcap or the new generation ngspice.
oregano is licensed under the terms of the GNU GPL-2.0 included in the
file COPYING.

----

### Quick install guide
#### Requirements

You need `gtk+-3.0`, `gtksourceview-3.0`, `goocanvas-2.0` and `libxml2` in order to build oregano.
These are usually included in your favorite distributions repositories and can otherwise be found at the [gnome public ftp](ftp://ftp.gnome.org) server.


#### Building

To build the oregano application issue the following for a debug build

    waf configure
    waf debug -j

or the following for a release build

    waf configure
    waf release -j

For additional options like specifying the install directory, consult

    waf --help

Note that additional options need to be passed to the `configure` stage.

#### Installation

For installation to the default directories, it is usually required to run

    waf install

with root privileges.

After installation oregano can be started by running `oregano` from terminal.

#### Removal

    waf uninstall


----

### Web

The [official oregano website](https://srctwig.com/oregano) - a clone of this README.md

### Mailinglist

The mailing list has yet to be set up, but this should happen within a few weeks.

As every mailinglist, we also expect appropriate behavior, respect and a positive manner.

----

### Contributions

are very welcome! We provide `TODO`,`ARCHITECTURE.md` and the files under `docs/*` as a starting point, an overview that should help you going.
If you want to discuss an issue or something you would like to implement, don't be shy, drop a mail to the mailinglist (see below).


#### Bugs

For bug and issue tracking as well as feature requests, the github built in issue tracker is used.

#### Translations

Translators are welcome to translate at transifex which will be synced into the git repository a day before a new release is created (see the release milestones for planned release dates)

----

#### Code

##### Coding style

As always, code should be kept modular and all methods should get a sane name (R.I.P. foo & bar)
New code should be written in mostly K&R fashion.
It's easiest to show within an example snippet how code should look like:

```
/**
 * \brief description - may be ommitted
 *
 * description from a toplevel viewpoint, what it does
 * @param long_name some magic key
 * @param offset whatsoeverargument
 * @param zoom the new zoom level
 */
int
some_function (gint16 long_name, gint16 offset, gint16 zoom)
{
    int ret;
    // comments for developers
    // like why something is done or has to be done that specific way
    // example:
    // required to handle zoomed shifts
    long_name -= offset/zoom;

    ret = function_call (&long_name, zoom);
    return ret;
}
```

Use <tabs> to indent, but whitspaces for syntetical linebreak indents.

```
{
|←tab→|{
|←tab→|←tab→|a_very_long_func_name_breaking_120_chars (first_argument,
|←tab→|←tab→|← - - - any sane number of spaces - - - →|second_argument,
|←tab→|←tab→|← - - - any sane number of spaces - - - →|third_argument);
|←tab→|}
}
```

Note: There is a lot of old code, that has mixed styling. It will be fixed over time.

#### Commit messages

Git commit messages should be one (1) line, describing briefly what you did. If it closes a bug append a , closes #bugnumber or , fixes #bugnumber. The #bugnumber refers to the github issue tracker number of a specific bug. Add no trailing . to the commit message.
If it is a commit fixing a corner case bug, add one (1) space line followed by an explaining paragraph. It should explain the cause as well as the fix used (please omit pony adventure stories or fairy tales, keep it brief).
