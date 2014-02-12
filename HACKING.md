## HACKING

### Code

Readable code with a sane amount of good comments (always describe the `why`, but not the `how` is prefered.

#### Gettings started

Have a look at existing bugs, small bugs are usually easy to fix and get your started.
Have a dive through the `ARCHITECTURE.md` file which describes roughly how `Oregano` works from a top down point of view.


#### ProTips

0. Be nice.
1. Be explicit in your commit messages.
2. Add test cases to the testsuite where possible.
3. Keep your code clean and comment if necessary or non trivial.
4. Comments might be outdated, but should be fairly trustable by now.
5. Check Travis CI for known build failures.
6. Fix the source of a bug, do not code around it.
7. Do not change things based on personal preference or lack of understanding.
8. Always note all the changes made in your commit messages.


----

#### Code

##### Coding style

As always, code should be kept modular and all methods should get a sane name (R.I.P. foo & bar)
New code should be written in mostly K&R fashion.
It's easiest to show within an example snippet how code should look like:

```C
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

Use <kbd>tab</kbd>s to indent, but <kbd>space</kbd>s for syntetical linebreak indents.

```C
{
|←tab→|{
|←tab→|←tab→|a_very_long_func_name_breaking_120_chars (type first_argument,
|←tab→|←tab→|← - - - any sane number of spaces - - - →|type second_argument,
|←tab→|←tab→|← - - - any sane number of spaces - - - →|type third_argument);
|←tab→|}
}
```

Note: There is a lot of old code, that has mixed styling. It will be fixed over time.

### Tests

Verify that all existing tests do pass before calling for review/doing merge requests by calling `waf runtests` after successfully compiling (installing is _not_ required).
Add new tests whenever possible/sane!

### Commit messages
Git commit messages should be one (1) line, describing the changeset briefly. If it closes a bug append a `, closes #bugnumber` or `, fixes #bugnumber`, where `#bugnumber` refers to the github bugtracker bugnumber.
Always describe what effects your changes have or why that change was necessary if not obvious.


    Grid: Split into model and view objects, closes #77.

    The split was necessary due to ... and blah.

---


### Translations

* `potfiles.in` list of files to be skimmed for translateable data.
* `POTFILES.in` list of files to be translate to header files via `intltool-update` and the results will be added to `potfiles.in`.

These are regenerated automatically via the `waf` target `spawnpot` and should not be modified manually but only via `po/wscript`.
