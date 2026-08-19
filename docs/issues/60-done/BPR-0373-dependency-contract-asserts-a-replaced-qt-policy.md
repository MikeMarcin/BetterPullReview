---
id: BPR-0373
title: The dependency contract asserts a Qt policy that was replaced
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The dependency contract asserts a Qt policy that was replaced

`dependency_modernization_contract` fails on Windows, and it is **not** a
Windows defect. It fails everywhere. It was found during the Windows program and
is recorded here so it is not mistaken for one.

`tests/scripts/dependency_modernization_tests.py:115` asserts:

```python
self.assertIn("Qt is never fetched automatically", provider)
```

against `cmake/BprDependencies.cmake`. That sentence is not in that file. It is
not anywhere under `cmake/`. `test_qt_is_package_only_and_never_fetched` asserts
`caller-installed Qt` and fails the same way.

`fbf0f18` — "build(deps): acquire Qt through a pinned provider (BPR-0337)" —
deliberately replaced the policy those assertions describe. Its own message says
so:

> The dependency boundary required the caller to install Qt. That suited a host
> someone had already set up by hand, but not the platform work in BPR-0036 and
> BPR-0156, where a new target machine must build from a clean checkout.

It added `cmake/BprQtProvider.cmake` with a pinned version, a shared cache under
`CPM_SOURCE_CACHE`, a lock around acquisition, an offline mode, and validation
of what was obtained. It changed **zero** lines of
`dependency_modernization_tests.py` — verified by `git show fbf0f18 --stat`.

So the contract still asserts "Qt is never fetched", the build now fetches Qt on
purpose, and the two have disagreed since that commit on every platform.

## The rest of this test's failures are not all the same thing

Thirteen cases fail under the launcher and six fail outside it, so the set has
at least two causes and only some are this one. The clearly-stale ones are the
two Qt-policy cases above. The others —
`test_predeclared_dependency_target_is_rejected`,
`test_provider_owned_package_redirect_is_rejected_unexecuted`,
`test_sqlite_*`, `test_repository_modules_ignore_caller_module_path`,
`test_standard_tool_discovery_ignores_config_preference`, and
`test_tests_disabled_core_graph_omits_test_and_qt_dependencies` — configure
CMake in a scratch tree and check the diagnostic or the return code. Whether
those are stale, Windows-specific, or container-specific is **not** established
here and must not be assumed from this card.

## Required final state

- The contract asserts the dependency policy the repository actually has. Qt is
  fetched, through the pinned provider, and the assertions describe that with at
  least the rigour the old ones had — a pinned version, the shared cache, the
  lock, the offline mode, and validation of what was acquired.
- Rewriting the assertions to match today's file is not sufficient on its own. A
  contract that is edited to agree with whatever the build currently does is not
  a contract; it needs to fail if the provider stops pinning, stops validating,
  or starts accepting an arbitrary Qt.
- The remaining failing cases in this file are separated by cause, and the ones
  that are Windows- or container-specific are moved to a card that says so.
- `dependency_modernization_contract` passes on this host and is not made to
  pass by deleting coverage.

## Verification

```sh
py -3 tests/scripts/dependency_modernization_tests.py \
  --build-dir build/core-only --cmake cmake \
  --cpm-source-cache build/.cpm-cache --sqlite-source <staged sqlite>
ctest --preset core-only --output-on-failure -R '^dependency_modernization_contract$'
```

Run it outside the launcher too. The difference between six and thirteen
failures is itself information about which cases belong to which cause.

## Measured on macOS — 2026-08-19

The card was authored against a tree state that no longer exists. The Qt-policy
half was fixed by the other line and arrived here as done [[BPR-0386]]; the
thirteen-versus-six split does not reproduce; and what is actually left is one
defect that fix introduced.

### The stale assertions are already gone

`assertIn("Qt is never fetched automatically", …)` is nowhere in the file, and
`test_qt_is_package_only_and_never_fetched` no longer exists. In their place:

- `test_dependency_discovery_is_centralized_and_package_based` asserts
  `"BPR_QT_PROVIDER=OFF requires caller-installed Qt"`, which **is** present at
  `cmake/BprDependencies.cmake:135`, and `"bpr_acquire_qt(bpr_qt_prefix)"`.
- `test_qt_provider_off_requires_a_caller_installed_qt` configures with
  `-DBPR_QT_PROVIDER=OFF` and asserts the diagnostic that branch actually emits
  rather than a blanket never-fetched claim.

So bullet one is satisfied, by a commit this line received rather than by work
done here.

### One case cannot execute at all

Both runs — direct and under the launcher — report **21 tests, 1 error, 20 ok**,
and it is the same case each time:

```
ERROR: test_qt_provider_is_the_default_acquisition_path
  File "tests/scripts/dependency_modernization_tests.py", line 394
    with tempfile.TemporaryDirectory() as temporary_directory:
NameError: name 'tempfile' is not defined
```

The file imports `sys, argparse, json, os, re, shutil, subprocess, unittest,
Path` and never `tempfile`. Every other test in it uses
`contained_temporary_directory()`; this one lone case — added by the same commit
that repaired the Qt strings — reached for the standard-library form and the
import was never added.

Classification: **a real defect, not staleness and not host-specific.** It is a
pure Python `NameError`, identical with and without the launcher, and it would
reproduce on any platform. The coverage the fix added for "the default
acquisition path" has therefore never run.

`contained_temporary_directory()` is the right substitute rather than adding the
missing import: it yields `str` precisely so it drops into a
`tempfile.TemporaryDirectory()` call site, and the Windows lane runs this suite
inside an AppContainer, where `tempfile`'s owner-only descriptor is unreadable —
which is [[BPR-0350]]'s whole subject. The standard-library form is not merely
inconsistent here; it is the form that cannot work there.

### The rest of the file passes, measured

Every case the card warned against assuming stale was run and passes:
`test_predeclared_dependency_target_is_rejected`,
`test_provider_owned_package_redirect_is_rejected_unexecuted`, all seven
`test_sqlite_*`, `test_repository_modules_ignore_caller_module_path`,
`test_standard_tool_discovery_ignores_config_preference`, and
`test_tests_disabled_core_graph_omits_test_and_qt_dependencies`.

The launcher is `bpr_add_network_isolated_test`
(`cmake/BprTestIsolation.cmake:320-456`), which on macOS wraps the command in
`sandbox-exec` with `cmake/sandbox/macos-network-deny.sb` — `(allow default)`
plus denied network — and scrubs `PYTHON*` and the dynamic-loader variables. On
this host it changes nothing: the same one error, the same twenty passes. So no
case in this file is container-specific here, and there is nothing to move to a
Windows card. Whether the card's original thirteen included Windows-specific
causes cannot be recovered from this host; the split predates the fix.

### What is left for this card

- Repair the `NameError` so the default-acquisition-path coverage executes.
- Give the assertions the rigour the card asks for. `BPR-0386` bound the `OFF`
  diagnostic and the `bpr_acquire_qt` call; nothing yet binds the four other
  mechanisms the provider actually implements, so the contract would not notice
  if they were dropped:

  | Mechanism | Where | The fact an assertion must bind |
  |---|---|---|
  | Version pin | `cmake/BprQtProvider.cmake:16` | `set(BPR_QT_VERSION "6.10.3")` — a literal, not a range or an env default |
  | Shared cache | `:208` | the version-namespaced `${CPM_SOURCE_CACHE}/bprqt/${BPR_QT_VERSION}` template |
  | Lock | `:212-223` | `file(LOCK … GUARD FUNCTION TIMEOUT 1800)` and its fail-closed `FATAL_ERROR` |
  | Offline mode | `:227-233` | the `BPR_DEPENDENCY_OFFLINE` gate sitting strictly *before* `bpr_download_qt` |
  | Validation | `:61-89` | the six required package configs, and the version match against `Qt6ConfigVersionImpl.cmake` rather than the forwarding file |

  That is what "not sufficient to rewrite the assertions to match today's file"
  means concretely: each row has to fail if the provider stops doing it.

## Result — 2026-08-19

`test_qt_provider_is_the_default_acquisition_path` now uses
`contained_temporary_directory()`, so the coverage that commit added executes for
the first time.

`test_qt_provider_keeps_every_mechanism_it_replaced_the_old_policy_with` binds all
five mechanisms, each paired in the source with the failure it exists to catch:
the literal three-component pin, the version-namespaced cache path, the
`GUARD FUNCTION` lock with its timeout and its fail-closed result check, the
offline gate's position **strictly before** `bpr_download_qt` (asserted by index
comparison, not by presence), the six required package configs, and the version
match read from `Qt6ConfigVersionImpl.cmake` rather than the forwarding file. It
also requires the validator to be what the acquisition path actually consults, so
it cannot become a function nothing calls.

### Mutation-checked, not asserted

Each was driven to fail by breaking the provider, then restored:

| Provider mutation | Verdict |
|---|---|
| `BPR_QT_VERSION` becomes `$ENV{BPR_QT}` | fails |
| lock becomes `GUARD PROCESS` | fails |
| the `BPR_DEPENDENCY_OFFLINE` gate renamed away | fails |
| validation reads the forwarding `Qt6ConfigVersion.cmake` | fails |
| provider restored | passes |

`git diff --stat cmake/BprQtProvider.cmake` is empty afterwards.

### Evidence

| Check | Result |
|---|---|
| `dependency_modernization_tests.py` direct, before | 21 tests, 1 error |
| same, after | **22 tests, OK** |
| `ctest --preset core-only -R '^dependency_modernization_contract$'` | passes |

Nothing was made to pass by deleting coverage: the case count went up by one and
the previously-erroring case now runs.

### Left to another card

The sibling instance of the same migration mistake — a
`contained_temporary_directory(dir=...)` call in
`tests/scripts/network_isolation_counterexamples.py` — belongs to [[BPR-0394]],
together with the audit of every other call site and a gate for the class.
