---
id: BPR-0386
title: The Qt provider policy outran its own tests
status: Done
rfc: RFC-BPR-0013
depends_on: []
completed: 2026-08-18
---

# The Qt provider policy outran its own tests

`dependency_modernization_contract` fails two subtests:

```
FAIL: test_qt_is_package_only_and_never_fetched
FAIL: test_dependency_discovery_is_centralized_and_package_based
AssertionError: 'caller-installed Qt' not found in "... The pinned Qt 6.10.3
provider produced '<cache>/bprqt/6.10.3/6.10.3/macos', but Qt6 with Core, Gui,
Widgets, and Network did not resolve from it."
```

Both encode a policy that no longer holds. `842fb46` (2026-08-02) wrote them
when Qt was package-only. `fbf0f18` ([[BPR-0337]], 2026-08-17) added
`BPR_QT_PROVIDER`, defaulting **ON** (`cmake/BprQtProvider.cmake:19-23`), which
acquires a pinned Qt — and the tests were not carried with it.

So `test_qt_is_package_only_and_never_fetched` configures without
`-DBPR_QT_PROVIDER=OFF`, takes the provider branch, and asserts against the
diagnostic only the `OFF` branch emits (`cmake/BprDependencies.cmake:135`).
Its name asserts a policy the build deliberately abandoned.

Found while fixing [[BPR-0385]], which repaired the other, unrelated failure in
the same area — a forwarding bug that starved the nested KSyntaxHighlighting
configure of `-DCMAKE_PREFIX_PATH`.

## This is the third of its kind

[[BPR-0382]] (format gate), [[BPR-0383]] (pinned digests), [[BPR-0362]]
(Windows generator) and now this one all have the same shape: a change landed
and the checked-in expectation that described the old behaviour stayed. Each was
found by a later slice running a wider gate, not by the slice that caused it.
Worth someone deciding whether that is acceptable rather than repairing a fourth
instance.

## Required final state

- The tests describe the policy the build actually implements: Qt is acquired
  through a pinned provider by default, and `BPR_QT_PROVIDER=OFF` requires a
  caller-installed Qt of at least the minimum version.
- `test_qt_is_package_only_and_never_fetched`'s name says what it now checks.
  A test whose name asserts the opposite of the policy is worse than no test.
- Both `BPR_QT_PROVIDER` branches are covered: the provider path succeeds from
  the pinned prefix, and the `OFF` path fails closed with its caller-installed
  diagnostic.
- `dependency_modernization_contract` passes.

## Not this card

Do not change the provider default to make the tests pass. BPR-0337 chose it
deliberately, and the pinned acquisition is what [[BPR-0385]] just proved the
nested configure depends on.

## Result — 2026-08-18

Both subtests now describe the build that exists.

`test_dependency_discovery_is_centralized_and_package_based` asserted that
`BprDependencies.cmake` contains the string "Qt is never fetched
automatically". It does not, and should not: [[BPR-0337]] made the pinned
provider the default acquisition path. It now asserts what centralization
actually looks like — that acquisition goes through `bpr_acquire_qt`, and that
turning the provider off demands a caller-installed Qt rather than silently
reaching for one.

`test_qt_is_package_only_and_never_fetched` is renamed
`test_qt_provider_off_requires_a_caller_installed_qt` and now passes
`-DBPR_QT_PROVIDER=OFF`, which is the branch whose diagnostic it was always
checking. Its second assertion moved from "never fetched automatically", a
phrase the tree no longer contains, to "re-enable the pinned provider", which
is what that branch really tells an operator to do.

The old name is worth a note. It asserted the opposite of the policy the build
implements, which is worse than having no test there: a reader trusting the
name would conclude Qt is never fetched, while a pinned Qt sits in the
dependency cache being fetched.

### The half that nothing covered

Both existing subtests exercised the `OFF` branch. With the provider deleted
outright they would have kept passing, and the provider is the half that ships.
`test_qt_provider_is_the_default_acquisition_path` covers it: the option really
defaults `ON`, and a default configure records
`Qt6|CMAKE_CONFIG|Qt6::{Core,Gui,Widgets,Network}` in the dependency audit.

Mutation-checked by flipping the default to `OFF`; that test fails and the
other two do not, which is the point of adding it.

`dependency_modernization_contract` passes.
