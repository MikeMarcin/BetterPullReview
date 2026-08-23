---
id: BPR-0433
title: The launcher lets a contained child raise a modal loader dialog
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# The launcher lets a contained child raise a modal loader dialog

During the first Windows Qt-graph run ([[BPR-0412]]) the owner's desktop
filled with dialogs:

> better_pull_review_qt_comparison_scroll_controller_tests.exe — System Error
> The code execution cannot proceed because Qt6Guid.dll was not found.

One per Qt-linked test, dozens in a row, each waiting for a person to click
OK. The missing library is [[BPR-0430]]'s; that a missing library became a
*dialog* is this card's. A Windows process inherits its parent's error mode,
and with the default mode the loader reports a hard error by putting a box on
the desktop. CTest does not change that, and neither did the launcher, so an
unattended run stalled on the very kind of attended interaction the boundary
exists to rule out — and would have done so for any crash box as well.

## Required final state

- `windows_network_sandbox.cpp` sets `SEM_FAILCRITICALERRORS`,
  `SEM_NOGPFAULTERRORBOX`, and `SEM_NOOPENFILEERRORBOX` before
  `CreateProcessW`, so every contained child inherits them: a loader failure
  exits at once with `STATUS_DLL_NOT_FOUND`, and a crash exits rather than
  prompts.
- The inherited mode is measured from inside the container, not inferred
  from the source: a probe reads `GetErrorMode()` under the launcher and
  reports the three bits set, where an uncontained probe reports the host's
  default.
- No test's verdict changes: a failure that was a dialog is the same failure
  as an exit code, only visible to CTest.

## Verification

```sh
cmake --build --preset core-only --target tool_better_pull_review_network_sandbox
build/core-only/better_pull_review_network_sandbox.exe python build/core-only/probe/errmode.py
```

## Result

Measured with a one-line probe that prints `GetErrorMode()`:

```
outside the launcher        error_mode=0x3
inside the relinked launcher error_mode=0x8003
```

`0x8003` is `SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
SEM_NOOPENFILEERRORBOX`, inherited by the contained child; the host's own
default carries only the first two. The relinked `core-only` launcher was
itself refused by Smart App Control for its first minutes ([[BPR-0424]]),
so the measurement above is from the `windows-dev` build of the same
source, which the policy allowed at once; both are the one
`windows_network_sandbox.cpp`.
