---
id: BPR-0363
title: A fresh configure leaks to a system Qt and fails KSyntaxHighlighting
status: Ready
rfc: RFC-BPR-0013
depends_on: []
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

## Where to look first

`cmake/BprSyntaxHighlightingProvider.cmake:320-340` forwards `CMAKE_PREFIX_PATH`
and `Qt6_DIR` to the nested configure, but `bpr_syntax_forward` (`:114-128`)
drops any name that is not defined *at that moment* and any whose value is
empty. So the question is whether those two are set, in this scope, by the time
the syntax provider runs — `cmake/BprDependencies.cmake:113` acquires Qt and
`:126` is where the syntax provider is reached.

Stated as the place to start rather than as the answer: the forwarding code is
present and correct-looking, so the fault is more likely ordering or scope than
a missing forward.

## Required final state

- Configuring a fresh tree succeeds without any Qt outside the pinned
  acquisition being consulted.
- The nested KSyntaxHighlighting configure is pointed at the pinned prefix
  explicitly, and fails closed if that prefix is absent rather than falling back
  to whatever the host happens to have installed.
- `dependency_modernization_contract` and
  `application_composition_contract` pass.
- The tests-disabled capsule graph configures.

## Not this card

Repairing the Homebrew Qt install would make the symptom disappear without
fixing anything: a build that silently uses a system Qt when it meant to use a
pinned one is the defect, and a healthy system Qt would only hide it.
