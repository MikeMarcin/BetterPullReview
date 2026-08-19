---
id: BPR-0373
title: The dependency contract asserts a Qt policy that was replaced
status: Ready
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
