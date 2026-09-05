---
schema: "implement-rfc.card.v1"
id: "BPR-0385"
title: "A fresh configure leaks to a system Qt and fails KSyntaxHighlighting"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
completed: "2026-08-18"
---

# A fresh configure leaks to a system Qt and fails KSyntaxHighlighting

Any configure of a *fresh* build tree fails while building the verified
KSyntaxHighlighting dependency:

```
CMake Error at cmake/BprSyntaxHighlightingProvider.cmake:139 (message):
  Verified KSyntaxHighlighting configure failed (1).
```

The nested configure's own log names the cause, and it is not a missing
dependency:

```
Found package configuration file:
  /opt/homebrew/lib/cmake/Qt6Core/Qt6CoreConfig.cmake
but it set Qt6Core_FOUND to FALSE so package "Qt6Core" is considered to be
NOT FOUND.
```

It resolved **Homebrew's** Qt6 rather than the pinned Qt this repository
acquires, and that Homebrew install is partial enough to report itself not
found. The pinned Qt is present and healthy; nothing asked for it.

## Why it is not visible day to day

An already-configured tree has KSyntaxHighlighting built and cached, so `dev`
builds and the ordinary matrix runs. Only a *fresh* configure hits it, which is
why it surfaces exclusively through tests that configure their own tree:

- `dependency_modernization_contract` —
  `test_dependency_discovery_is_centralized_and_package_based` and
  `test_qt_is_package_only_and_never_fetched`;
- `application_composition_contract` —
  `test_tests_off_release_graph_omits_public_path_fixture`;
- and the tests-disabled capsule graph, which cannot be configured at all.
  That last one blocks [[BPR-0256]] from regenerating its capsule and operator
  packet.

Found while running the full `dev` matrix for [[BPR-0348]] and again while
checking whether [[BPR-0256]] was actionable after [[BPR-0262]] closed.

## What was checked, including one lead that is wrong

The obvious explanation is that the pinned prefix never reaches the nested
configure. Two candidate mechanisms were tested and one is disproven, so nobody
needs to spend the time again.

**Scope — disproven.** `cmake/BprDependencies.cmake:115` appends the pinned
prefix to `CMAKE_PREFIX_PATH` with a plain `list(APPEND)`, which would be
function-local. But `bpr_configure_dependencies` spans `:69-236` and calls
`bpr_acquire_syntax_highlighting()` at `:222`, inside that same scope. The
append is visible where it is needed.

**Acquisition — working.** A fresh capsule-graph configure resolved
`Qt6_DIR` to the pinned tree
(`build/.cpm-cache/bprqt/6.10.3/6.10.3/macos/lib/cmake/Qt6`). The provider did
its job; the pinned Qt is there and is what the parent selected.

So the leak happens between the parent's correct selection and the child's
resolution. Two things left to examine:

- `bpr_syntax_forward` (`BprSyntaxHighlightingProvider.cmake:114-128`) escapes a
  list with `string(REPLACE ";" "\\;" ...)` before passing `-D...`. Whether that
  survives `execute_process` into the child's cache is worth confirming
  directly rather than reasoning about.
- ECM calls `find_package(Qt6Core)`, which consults `Qt6Core_DIR` — a different
  variable from the forwarded `Qt6_DIR`. Forwarding `Qt6_DIR` does not
  constrain a component lookup; only `CMAKE_PREFIX_PATH` would.

**An unrelated observation worth keeping.** The checked-in `build/dev` tree has
`Qt6_DIR=/opt/homebrew/lib/cmake/Qt6` — it is configured against Homebrew's Qt,
not the pinned one, while a fresh tree selects the pinned one. Whatever the
outcome here, those two answers should not both be reachable.

## Required final state

- Configuring a fresh tree succeeds without any Qt outside the pinned
  acquisition being consulted.
- The nested KSyntaxHighlighting configure is pointed at the pinned prefix
  explicitly, and fails closed if that prefix is absent rather than falling back
  to whatever the host happens to have installed.
- `application_composition_contract` passes.
- `dependency_modernization_contract`'s KSyntaxHighlighting-related failure is
  gone. Its two remaining failures are a different defect and are not this
  card's — see the result below.
- The tests-disabled capsule graph configures.

## Not this card

Repairing the Homebrew Qt install would make the symptom disappear without
fixing anything: a build that silently uses a system Qt when it meant to use a
pinned one is the defect, and a healthy system Qt would only hide it.

## Result — 2026-08-18

A variable name collision, in `bpr_syntax_forward`
(`cmake/BprSyntaxHighlightingProvider.cmake:114-128`).

Every caller passes an accumulator named `forwarded`. The function's own
temporary was also named `forwarded`, so it shadowed the caller's list. By the
time `${${arguments_variable}}` was read it held one escaped *value* rather
than the arguments gathered so far, and each call therefore discarded
everything accumulated before it.

Only the last name in the list survived, and it survived twice — once as a bare
value with no `-D`, once correctly. Instrumenting a fresh configure showed
exactly that:

```
forwarded: /…/bprqt/6.10.3/6.10.3/macos/lib/cmake/Qt6      <- bare, no -D
forwarded: -DQt6_DIR=/…/bprqt/6.10.3/6.10.3/macos/lib/cmake/Qt6
```

`-DCMAKE_PREFIX_PATH` was never passed, so the nested configure had no idea
where the pinned Qt was and found Homebrew's — which reports itself not found,
which failed the build. The compiler, flags, build type, toolchain file and
sysroot were being dropped too; the Qt leak is simply the one that produced a
visible error.

After renaming the temporary, the same probe forwards all six:

```
-DCMAKE_MAKE_PROGRAM=…  -DCMAKE_C_COMPILER=…  -DCMAKE_CXX_COMPILER=…
-DCMAKE_BUILD_TYPE=Release  -DCMAKE_PREFIX_PATH=…/macos
-DQt6_DIR=…/macos/lib/cmake/Qt6
```

and a fresh tests-disabled configure exits 0 where it previously failed.

`application_composition_contract` now passes — 99.51 s, both subtests,
including the one that has been red throughout this session.

### The lead that was wrong, kept

The card first suspected scope: the pinned prefix is appended with a plain
`list(APPEND)` at `BprDependencies.cmake:115`, which would be function-local.
That is not it — `bpr_configure_dependencies` spans `:69-236` and calls the
syntax provider at `:222`, inside the same scope. Recorded so the next reader
does not spend the time.

### What is left, and why it is not this

`dependency_modernization_contract` still fails
`test_qt_is_package_only_and_never_fetched` and
`test_dependency_discovery_is_centralized_and_package_based`, for an unrelated
reason: they encode the policy that Qt is never fetched, dating from
`842fb46` (2026-08-02). `fbf0f18` ([[BPR-0337]], 2026-08-17) introduced
`BPR_QT_PROVIDER`, defaulting **ON**, which acquires a pinned Qt — and did not
carry its tests. The test configures without `-DBPR_QT_PROVIDER=OFF` and then
expects the diagnostic that only the `OFF` branch emits.

That is a policy-versus-test disagreement of the same kind as [[BPR-0362]], not
a leak, and [[BPR-0386]] owns it.
