
## Windows at `6635aa03` (bpr-main-win, 2026-09-23)

`6635aa03` contains `c82db490`. The run used the documented Windows developer
preset, `windows-dev` (Visual Studio 18 2026, Debug; see
`docs/development/build-and-test.md`), which was reconfigured at this head.
The same two rows were also run on the station's usual `dev` tree (Ninja,
MSVC 19.51 Debug), and the results were identical. The logs are station
records under `build/logs/bpr-0866-1026-main-win/` and are not committed.

| Row (`ctest --preset windows-dev -j1 -V`) | Result |
|---|---|
| `conditional_branch_identifiers` | Pass, 3.22 s, `CONDITIONAL_BRANCH_IDENTIFIERS_OK:792` |
| `conditional_branch_identifier_contract` | Pass, 0.34 s, `Ran 26 tests`, `OK` |

The scope line reads
`CONDITIONAL_BRANCH_IDENTIFIERS_SCOPE:host_defines=Q_OS_WIN,_MSC_VER,_WIN32 policed_lines=12394 policed_files=78`.
That is the expected Windows host set, with 12,394 policed lines, two more
than the 12,392 expected at `c82db490`. The contained `windows-dev` runs also
print `Failed to find real location of C:\Program Files\Python314\python.exe`
from the launcher, and pass anyway. The `dev` runs do not print it.

**Both changed sources still compile under MSVC.** In `windows-dev`, after
`installed_git_cache.cpp` and `installed_git_process.cpp` were touched,
`cmake --build --preset windows-dev --target better_pull_review_adapter_git`
recompiled both: `Build succeeded`, 0 warnings, 0 errors, with `/W4 /WX`. In
`dev`, the rebuild that brought in `c82db490` compiled both into the adapter
and into the git contract suites, with no diagnostic.

This note does not move the card; it is Linux's.
