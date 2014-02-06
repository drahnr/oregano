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



### Translations

* `potfiles.in` list of files to be skimmed for translateable data.
* `POTFILES.in` list of files to be translate to header files via `intltool-update` and the results will be added to `potfiles.in`.

These are regenerated automatically via the `waf` target `spawnpot` and should not be modified manually but only via `po/wscript`.
