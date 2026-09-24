---
schema: "implement-rfc.card.v1"
id: "BPR-0866"
title: "The conditional-branch gate polices BPR_BUILD_TESTING regions as code no host compiles"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-sweep-fixes, diagnosing BPR-0854 item 3","found_at":"e84eaba2","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"The gate no longer reports findings in regions the build compiles, or the card records why it must keep doing so","outcome":"pass","revision":"c82db490","evidence":{"kind":"command","detail":"Testing and other build-switch regions are undecided and left to the compiler on every host (plant P4 silent on Linux, and the build caught it). Real runs report no finding: Linux OK:792 at c82db490 and OK:794 at 8e49aae7, macOS OK:792 at e648db24 (bpr-main-mac), Windows OK:792 at 6635aa03 (bpr-main-win).","path":"docs/issues/60-done/BPR-0866-the-conditional-branch-gate-polices-testing-only-regions-as-dead-code.md#closed-all-three-platform-runs-meet-the-acceptance-bpr-main-linux-2026-09-24"}},{"criterion":"The two rules from BPR-0854 item 3 still hold and their five tests in tests/scripts/conditional_branch_identifier_tests.py pass","outcome":"pass","revision":"c82db490","evidence":{"kind":"command","detail":"conditional_branch_identifier_contract: Ran 26 tests, OK on Linux (c82db490, 8e49aae7), macOS (e648db24) and Windows (6635aa03); the five tests of the init-capture and defined-call rules are among them, unchanged.","path":"docs/issues/60-done/BPR-0866-the-conditional-branch-gate-polices-testing-only-regions-as-dead-code.md#tests-16-to-26"}},{"criterion":"conditional_branch_identifiers passes, and a planted rename in a region the gate still polices is shown to be caught","outcome":"pass","revision":"c82db490","evidence":{"kind":"command","detail":"The row passes on all three platforms with the expected host sets (Linux Q_OS_LINUX,Q_OS_UNIX,__linux__,__unix__; macOS Q_OS_DARWIN,Q_OS_MACOS,Q_OS_UNIX,__APPLE__ with 8,243 policed lines; Windows Q_OS_WIN,_MSC_VER,_WIN32 with 12,394). Plants P1, P2 and P3 in policed branches were caught on Linux; P2 is missed by the old gate.","path":"docs/issues/60-done/BPR-0866-the-conditional-branch-gate-polices-testing-only-regions-as-dead-code.md#planted-renames"}},{"criterion":"The card states what a green gate asserts, including the collision-only remit","outcome":"pass","revision":"c82db490","evidence":{"kind":"observation","detail":"The card's section 'What a green gate asserts' lists what a green result covers and does not, including the folded-collision-only remit and the parameterless-function limit; the gate prints the same remit after its verdict line.","path":"docs/issues/60-done/BPR-0866-the-conditional-branch-gate-polices-testing-only-regions-as-dead-code.md#what-a-green-gate-asserts"}}]
---

# BPR-0866: The conditional-branch gate polices BPR_BUILD_TESTING regions as code no host compiles

## Defect

`scripts/verify_conditional_branches.py` exists to catch identifiers left
behind by a rename inside a preprocessor branch **the host does not compile** —
code a compiler will never check. It decides what the host compiles from
`host_macros()`, which returns platform macros only. `BPR_BUILD_TESTING` is not
among them, so every `#if defined(BPR_BUILD_TESTING)` region is treated as dead
code the gate must police.

Those regions are not dead. The `dev` preset compiles the very translation
units that hold them with `-DBPR_BUILD_TESTING=1`, so the compiler already
checks every identifier in them. The gate is doing redundant work on live code,
and doing it with a text scanner's precision.

That is where all eleven of [[BPR-0854]] item 3's findings came from. Every one
sat under `#if defined(BPR_BUILD_TESTING)` in a compiled translation unit. Two
pattern gaps turned that misclassification into concrete false positives —
lambda init-captures, and a call whose name folds onto a same-spelling local —
and both gaps are now fixed. But the misclassification behind them is
untouched: the next construct the scanner cannot parse will produce the next
batch of false positives in exactly these regions, and the next investigation
will start from scratch.

## Scope

Two candidate directions, from the station that found it:

1. **Teach `host_macros()` the build's own definitions.** Most faithful to the
   gate's premise, and hardest: the script has no view of the build's compile
   flags, so it would need them passed in, or would have to infer them, and an
   inference that is wrong in the quiet direction reintroduces the blind spot
   the gate exists for.
2. **Scan `BPR_BUILD_TESTING` regions as live.** Simpler and matches reality
   for every build this project actually runs: the tests-enabled build compiles
   them, and a tests-disabled release build compiles the *other* side of those
   branches, which the gate would still police. The risk is narrow — a
   rename inside a testing-only region in a tree only ever built without
   testing would go unreported by this gate.

Pick one with evidence rather than by preference, and say on this card which
regions the gate then still polices, so the next reader knows what a green
`CONDITIONAL_BRANCH_IDENTIFIERS_OK` does and does not assert.

While there: record the gate's remit plainly somewhere a reader of its output
will find it. It reports a name whose **folded spelling collides with a
declared name**; a typo colliding with nothing is invisible to it. During
BPR-0854 item 3's acceptance, renaming a call to a freshly invented spelling
was correctly not reported, patched or unpatched. That is by design, and it is
easy to misread `OK:695` as "no misspellings exist".

## Acceptance

- The gate no longer reports findings in regions the build compiles, or the
  card records why it must keep doing so.
- The two rules from BPR-0854 item 3 still hold: an init-capture declares its
  name, and a call to a function the file defines is not a rename. Their five
  tests in `tests/scripts/conditional_branch_identifier_tests.py` still pass.
- `conditional_branch_identifiers` passes, and a planted rename in a region the
  gate still polices is shown to be caught.
- The card states what a green gate asserts, including the collision-only
  remit.

## The fix, Linux evidence (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, Python 3.13),
`build/dev`, claimed on `98e5b796`. Logs are under `build/r2-logs/bpr0866-*` on
that station. They are not committed.

### The folded finding: the Apple-only lambda

The coordinator folded one finding into this card:
`conditional_branch_identifiers` flagged the `__APPLE__`-only lambda in
`tests/end_to_end/setup_destination_public_path_tests.cpp:632-633`
(`[&session](const auto& aSession)`), one of nine findings recorded on
[[BPR-0863]] and [[BPR-0864]].

**It is a different defect from this card's.** That lambda sits in a platform
branch that Linux really does not compile, so the gate is right to read it.
The false positive came from a scanner gap: a captured outer local was
compared with the lambda's same-spelled parameter. [[BPR-0854]] fixed that gap
at `bac4c745` (`captured_in`), together with the digit-separator gap behind the
other eight findings. **Nothing is left for this card to fix.** Verified on
Linux at `98e5b796`:

- the gate prints `CONDITIONAL_BRANCH_IDENTIFIERS_OK:792`
  (`bpr0866-condbranch-head-98e5b796.log`);
- the gate as of `bac4c745^`, run on the same tree, still reports all nine,
  including `:632` and `:633` (`bpr0866-condbranch-unpatched-gate-98e5b796.log`).

So the row was already green on Linux at `98e5b796`. The "red on main" in the
assignment predates `bac4c745`.

### The choice: undecided, not option 1 or 2

The gate now decides only **platform macros**. Every other macro is
*undecided*: `BPR_BUILD_TESTING` and every other `BPR_*` switch (testing,
fault injection, fixtures, packaging), compiler identity, feature macros and
include guards. A
branch is policed only when it is false whatever the undecided macros are. So
a platform branch inside a testing region is still read. The evidence behind
the choice:

- **Some build on the host compiles each side of `BPR_BUILD_TESTING`.** In
  `build/dev/compile_commands.json`, 44 translation units carry
  `-DBPR_BUILD_TESTING` (the application, the composition and the agent
  adapter, plus test targets). `application_composition_contract`, a `dev`
  row, configures `BUILD_TESTING=OFF`, asserts that no
  compile command carries `BPR_BUILD_TESTING`, and builds
  `app_better_pull_review`. So the compiler reads both sides. The gate's
  premise is to read only what no compiler on the host reads, and that is
  neither side.
- **Option 2 (read the switch as defined) moves the problem.** It would
  police the tests-off side, such as `main.cpp`'s four
  `__APPLE__ && !BPR_BUILD_TESTING` blocks on macOS, which the composition
  contract compiles. It would also leave every other switch read as
  undefined.
- **Option 1 (read the build's flags) answers for one build, not the host.**
  A header's branches depend on the including translation unit. And a gate
  bound to `dev`'s flags would police the tests-off side as option 2 does.
- **The same misclassification covered Qt's platform names.** HEAD knew only
  `_WIN32`, `__APPLE__`, `__linux__` and `__unix__`. So on Linux it policed
  the 2,577 lines under `Q_OS_UNIX` conditions that Linux compiles, and it read their
  `#else`, which is Windows code, as compiled.

### What changed

`scripts/verify_conditional_branches.py`:

- `PLATFORM_MACROS` lists the macros a host decides: `_WIN32`, `_MSC_VER`,
  `__APPLE__`, `__linux__`, `__unix__`, `Q_OS_WIN`, `Q_OS_MACOS`,
  `Q_OS_DARWIN`, `Q_OS_LINUX` and `Q_OS_UNIX`. `host_macros()` now includes
  Qt's names. It also changes two facts. Windows defines `_MSC_VER`, because
  every Windows build here uses MSVC. macOS no longer claims `__unix__`,
  which Apple's compiler does not define; the tree's own
  `defined(__unix__) || defined(__APPLE__)` idiom assumes the same. No branch
  in the tree tests `__unix__` alone, so this changes no result today.
- `evaluate` uses three values. `false && x` is false and `true || x` is
  true, whatever `x` is. Anything else involving an undecided value stays
  undecided.
- `inactive_lines` follows the chain. A branch is compiled when its own
  condition holds and no earlier branch's did. Before, an `#else` after a
  taken `#elif` counted as compiled, and so did an `#elif` after an `#if` the
  gate could not decide.
- `function_bodies` no longer reads a statement's parentheses as a parameter
  list (`for (; prefix != aPrefix.end(); …) {`). The enclosing function
  already covers that block with its real parameters.
- `unknown_platform_macros` refuses a platform-looking macro that the list
  does not name, such as `Q_OS_WINDOWS`, `__MACH__` or `_WIN64`, by name.
  Undecided is the quiet direction, so a new platform spelling fails loudly
  until it is added.
- The output says what it read. The verdict line is unchanged, and a
  `CONDITIONAL_BRANCH_IDENTIFIERS_SCOPE:host_defines=… policed_lines=…
  policed_files=…` line follows it, with one sentence of remit. The module
  docstring says the same.

**What moved, on the whole tree.** The HEAD gate and the candidate were run
side by side (`bpr0866-compare-models-final.log`, `bpr0866-attribute.log`).
The macOS and Windows columns are emulations: each gate's own
`inactive_lines` and `findings_for`, given that host's macro set. They are not
runs on those hosts.

| Host | Policed lines, HEAD → candidate | Files | Findings, candidate |
|---|---|---|---|
| Linux (run) | 30,505 → 12,045 | 111 → 87 | 0 |
| macOS (emulated) | 26,920 → 8,243 | 103 → 77 | 0 |
| Windows (emulated) | 30,103 → 12,392 | 107 → 78 | 0 |

On Linux, the 18,679 lines no longer policed are these:

- **Testing switches, compiled by `dev`:** 14,619 lines of
  `BPR_BUILD_TESTING` and 52 of `BPR_APPLICATION_TESTING ||
  BPR_BUILD_TESTING`. Also the application, SQLite and native-HTTPS testing
  switches, the fault-injection switches and the fixture-root and canary
  switches: 1,301 lines in all.
- **Qt names for Linux, compiled on Linux:** 2,585 lines of `Q_OS_UNIX`,
  `Q_OS_UNIX || Q_OS_WIN` and `Q_OS_LINUX`. Also 32 lines of the `Q_OS_LINUX`
  arms of macOS/Linux chains.
- **Compiled only by another local build or by another host:** 41 lines of
  `BPR_DISTRIBUTION_BUILD` (each platform's package preset), 35 lines of
  `BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING` (a target built only on
  macOS), 12 lines of `__clang__` (in `macos_keychain.cpp`, macOS only), one
  line of `BPR_AGENT_SUPERVISOR_EXECUTABLE_W` (Windows `dev`) and one line of
  `MSG_NOSIGNAL`, which Linux's headers define.

Newly policed on Linux: 219 lines. They are the `#else` branches after
Unix/Windows, macOS/Linux and `__APPLE__`/`__linux__` chains, and five lines
of `!defined(Q_OS_UNIX) && !defined(Q_OS_WIN)`. No build on Linux compiles any
of them.

### What the corrected chain rule found: 30 stale names

With the `#else` fix, the gate reported 30 names on all three hosts, all in
`src/adapters/git/src/installed_git_cache.cpp` (12) and
`installed_git_process.cpp` (18). Each is a fallback `#else` after a
`Q_OS_UNIX`/`Q_OS_WIN` chain, or after the macOS/Linux/Windows chain at
`installed_git_process.cpp:3042`. It still says `path`, `kind`, `bytes`,
`maximum_bytes`, `receiving`, `completed`, `source`, `destination` or `error`,
where the parameters became `aPath`, `aKind` and so on in `0ec1136a` ("Adopt
BetterPullReview C++ coding standard", 2026-08-02). **These are real renames
that stopped halfway, which is this gate's purpose.** They stayed hidden for
seven weeks because of the `#else` blind spot. The same 30 are there at
`e84eaba2`.

They are fixed in place: each reported identifier becomes its parameter, and
nothing else on the line changes. No supported build compiles these branches:
every platform defines `Q_OS_UNIX` or `Q_OS_WIN`, and the stale names built
cleanly on all three platforms, which proves no platform compiles them. On
Linux the preprocessed output of all eight translation units that compile the
two files is byte-identical before and after. That covers the adapter library
and the git-contract, anonymous-read and workspace-ownership test variants,
including the fault-injection ones. Neither file is under a round-trip pin.

A second false positive surfaced with them:
`src/app/src/github_application.cpp:5018`, `prefix` against `aPrefix`, in the
`within` lambda of a fallback that no platform compiles. It was the
statement-as-function gap above. It is fixed in the gate, so the pinned file
is unchanged.

### Tests: 16 to 26

`tests/scripts/conditional_branch_identifier_tests.py` gains ten tests:

- a testing region is left to the compiler on every host;
- a platform branch inside a testing region is still policed;
- a platform macro decides a condition it shares with a switch;
- an `#else` after a taken `#elif` is policed;
- a false `#elif` after an undecided `#if` is policed;
- Qt's platform names follow the host;
- a statement is not a function;
- a rename inside a statement block is still caught;
- an undecided platform spelling is refused;
- every host names its Qt platform and only macros the gate decides.

All 26 pass on the candidate. Run against the HEAD gate, with shims for only
the new names, 7 of the 10 fail (`bpr0866-tests-against-head-gate.log`). The
other three are guards that hold on both gates. The five tests of
[[BPR-0854]] item 3's two rules (init-captures, and calls to a function the
file defines) pass unchanged.

### Planted renames

Planted in a copy of the candidate tree (`/tmp`), except P4:

| Plant | Where | Candidate | HEAD gate |
|---|---|---|---|
| P1 | `installed_git_cache.cpp:405`, `#elif defined(Q_OS_WIN)`: `aPath` → `path` | caught | caught |
| P2 | `installed_git_cache.cpp:420`, the fallback `#else`: one fix undone | caught | missed |
| P3 | `agent_process_launcher.cpp:475`, `defined(BPR_BUILD_TESTING) && defined(Q_OS_WIN)`: `aEnabled` → `enabled` | caught | caught |
| P4 | `system_browser_opener.cpp:70`, a `BPR_BUILD_TESTING` region: `aLaunch` → `launch` | silent, by design | reported |
| P5 | a `#if defined(Q_OS_WINDOWS)` line | refused by name | not a concept |

Logs: `bpr0866-planted-candidate.log`, `bpr0866-planted-head-gate.log`,
`bpr0866-planted-testing-region.log`.

- **P4 is the quiet direction, and the compiler covers it.** It was planted in
  the real tree. The candidate gate printed `OK:792`. Building that one object
  in `build/dev` failed with `'launch' was not declared in this scope; did you
  mean 'aLaunch'?`. The file was then restored, and its SHA-256 matched
  (`252c02b2…`).
- **A plant outside the remit.** A half-done rename inside the parameterless
  `test_windows_assignment_failure_has_no_pre_assignment_side_effect`
  (`markerPath` against `marker_path`) was caught by neither gate. Shapes 1
  and 2 only read functions with an `aXxx` parameter. The remit below now
  says so.

**BPR-0854 item 3's tree.** At `e84eaba2`, the gate of that day reports the
eleven findings in `BPR_BUILD_TESTING` regions. The candidate reports none of
them. It reports the 30 fallback names above instead
(`bpr0866-candidate-on-e84eaba2.log`).

### Linux runs on the candidate

- `cmake --build --preset dev`: exit 0, no warnings
  (`bpr0866-build-candidate.log`).
- `ctest -R '^(conditional_branch_identifiers|conditional_branch_identifier_contract)$'`:
  both pass in `dev` (2.66 s and 0.08 s) and in `core-only`
  (`bpr0866-ctest-gate-rows.log`). The gate prints
  `CONDITIONAL_BRANCH_IDENTIFIERS_OK:792`, with
  `host_defines=Q_OS_LINUX,Q_OS_UNIX,__linux__,__unix__ policed_lines=12045
  policed_files=87`.
- `/usr/bin/clang-format` 22.1.8 leaves both C++ files unchanged.
- Only its own test suite imports the gate.

### What a green gate asserts

`CONDITIONAL_BRANCH_IDENTIFIERS_OK` on a host means that, in every branch no
build on that host compiles, no name matches a different declared name once
case and underscores are folded. That covers:

- branches a platform macro removes, including those inside testing regions;
- a parameter's bare form (shape 1) and any same-spelling name (shape 2),
  inside a function with an `aXxx` parameter;
- a name a chain declares in only some branches and uses after `#endif`
  (shape 3).

It does not assert:

- anything about a branch that only a build switch removes; the build that
  sets or clears the switch compiles it;
- anything about a file CMake builds only on another platform, outside its
  platform branches;
- anything about a misspelling that matches no declared name;
- anything about the parameterless functions, for shapes 1 and 2.

The gate prints this remit after its verdict.

### Acceptance state

- **The gate no longer reports findings in regions the build compiles:** pass
  on Linux. macOS and Windows are emulated only.
- **BPR-0854 item 3's two rules still hold, and their five tests pass:** pass.
- **`conditional_branch_identifiers` passes, and a planted rename in a
  policed region is caught:** Linux pass (P1, P2, P3). **macOS and Windows
  runs pending.**
- **The card states what a green gate asserts:** pass (above, and in the
  gate's output).

### Still needed: macOS and Windows runs

The host model changed for both, so each needs one real run. The candidate is
`c82db490` on `main`, which is the change and this record. At `c82db490` or
later, with `build/dev` rebuilt, the gate prints `OK:792` on Linux with
`policed_lines=12047` (`bpr0866-gate-c82db490.log`). Run:

```sh
ctest --test-dir build/dev -j1 --output-on-failure -V -R '^(conditional_branch_identifiers|conditional_branch_identifier_contract)$'
```

**macOS (Mac integrator).** Observe:

- both rows pass;
- `CONDITIONAL_BRANCH_IDENTIFIERS_SCOPE:host_defines=Q_OS_DARWIN,Q_OS_MACOS,Q_OS_UNIX,__APPLE__`,
  with `policed_lines` near the emulated 8,243 and `policed_files` near 77;
- `clang++ -dM -E -x c++ /dev/null | grep -E '__unix__|__APPLE__'` prints
  only `__APPLE__`. That confirms the `__unix__` correction.

**Windows (bpr-main-win).** The same command. Observe:

- both rows pass;
- `host_defines=Q_OS_WIN,_MSC_VER,_WIN32`, with `policed_lines` near the
  emulated 12,392 and `policed_files` near 78.

Both builds must also still compile `installed_git_cache.cpp` and
`installed_git_process.cpp`, which a normal `cmake --build --preset dev`
shows. If either host prints a finding, or refuses a platform spelling, the
line it prints names the file and the branch.

## The macOS run (bpr-main-mac, Mac batch 3, 2026-09-23)

Run on `e648db24`, which contains `c82db490`. `cmake --build --preset dev`
rebuilt 45 steps with no warning and no error, and `ninja -n` then reported
no work. The normal dev build still compiles both changed translation units,
in the adapter and in every test target that takes them directly:
`installed_git_cache.cpp` in `better_pull_review_adapter_git`, the
anonymous-public-read tests and the cache contract tests;
`installed_git_process.cpp` in the adapter, those two targets, the
agent-workspace ownership tests and the process contract tests
(`build/bpr0866-mac-build.log`).

The command ran as written (`build/bpr0866-mac-rows.log`), 2/2:

| Row | Result |
|---|---|
| `conditional_branch_identifiers` | Pass, 1.76 s, `CONDITIONAL_BRANCH_IDENTIFIERS_OK:792` |
| `conditional_branch_identifier_contract` | Pass, 0.13 s, `Ran 26 tests`, `OK` |

The scope line reads
`CONDITIONAL_BRANCH_IDENTIFIERS_SCOPE:host_defines=Q_OS_DARWIN,Q_OS_MACOS,Q_OS_UNIX,__APPLE__ policed_lines=8243 policed_files=77`,
which is the expected host set and 8,243 policed lines.

`clang++ -dM -E -x c++ /dev/null | grep -E '__unix__|__APPLE__'` prints only
`#define __APPLE__ 1`. The `clang++` on this Mac's `PATH` is Homebrew clang
23.1.1. The build's own compiler, `/usr/bin/c++` (Apple clang 21.0.0), and
`/usr/bin/clang++` print the same single line. So `__unix__` is not defined
on macOS, as the gate's host set assumes.

The macOS half holds, and the Linux station finishes the card.

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

## Closed: all three platform runs meet the acceptance (bpr-main-linux, 2026-09-24)

The macOS and Windows runs above were both asked for in "Still needed". Each
was checked against the observations listed there. Both runs used a head that
contains the candidate `c82db490`, and neither the gate nor its test suite has
changed since that commit.

| Observation asked for | macOS (`e648db24`) | Windows (`6635aa03`) |
| --- | --- | --- |
| Both rows pass | Yes: `OK:792`, and `Ran 26 tests`, `OK` | Yes: `OK:792`, and `Ran 26 tests`, `OK` |
| The host set | `Q_OS_DARWIN,Q_OS_MACOS,Q_OS_UNIX,__APPLE__`, as expected | `Q_OS_WIN,_MSC_VER,_WIN32`, as expected |
| `policed_lines` and `policed_files` near the emulation | 8,243 and 77, exactly as emulated | 12,394 and 78. The emulation gave 12,392. Linux's own count rose by the same two lines, from 12,045 to 12,047, between the emulation and `c82db490` |
| `__unix__` is not defined on macOS | Yes: every compiler on that Mac prints only `__APPLE__` | Not applicable |
| Both changed sources still compile | Yes, in the adapter and every test target that compiles them | Yes, under `/W4 /WX` in `windows-dev` and in `dev` |
| No finding and no refused platform spelling | None | None |

Linux was run again on `8e49aae7`, serially
(`build/logs/bpr-main-linux-20260924/bpr0866-rows-8e49aae7.log`, a station
record, not committed). Both rows pass. The gate prints
`CONDITIONAL_BRANCH_IDENTIFIERS_OK:794` and
`host_defines=Q_OS_LINUX,Q_OS_UNIX,__linux__,__unix__ policed_lines=12785 policed_files=91`.
The contract prints `Ran 26 tests`. The counts grew because the tree grew
after `c82db490`.

| Criterion | Result |
| --- | --- |
| The gate no longer reports findings in regions the build compiles | **Pass.** Testing regions are left to the compiler on every host (P4 above). The real macOS and Windows runs report no finding |
| BPR-0854 item 3's two rules hold, and their five tests pass | **Pass** on all three platforms, among the 26 |
| `conditional_branch_identifiers` passes, and a planted rename in a policed region is caught | **Pass.** The row passes on all three platforms. The plants P1, P2 and P3 were caught on Linux |
| The card states what a green gate asserts, including the collision-only remit | **Pass.** See "What a green gate asserts" above; the gate prints the same remit after its verdict |

Nothing is owed. The card finishes.
