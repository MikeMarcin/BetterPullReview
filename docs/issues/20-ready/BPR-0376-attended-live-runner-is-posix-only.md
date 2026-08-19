---
id: BPR-0376
title: The attended live runner is POSIX-only and its contract runs on Windows
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The attended live runner is POSIX-only and its contract runs on Windows

`rfc0016_device_live_runner_security` fails with 11 errors and 1 failure. The
dominant cause is not a defect in the runner's logic:

```
AttributeError: module 'os' has no attribute 'getuid'.
Did you mean: 'getpid'?
```

`scripts/run_rfc0016_device_capture_attended.py` calls `os.getuid()` at lines
1118, 1701, and 1808, and resolves the home directory through
`pwd.getpwuid(os.getuid()).pw_dir` at line 65. Every one is an ownership check
on an attended-capture artifact — the runner refusing to trust a file it does
not own. They are the right checks. They are written in an API that does not
exist on this platform.

A second cause is `WinError 1314` again, from a symbolic-link adversary the
account has no privilege to create, and a third is an unexamined
`AssertionError: 1 != 0`.

## The question this card carries

This is not a port-it-and-move-on defect, because Windows device capture is
explicitly **not** in scope yet. [[BPR-0311]] says so directly:

> Device capability stays unavailable on Windows until this card is terminal.

So there are two defensible dispositions and they are not equivalent:

1. **Make the runner refuse on Windows, explicitly.** Fail closed with a typed
   reason, the way this project treats every other unproven path. The contract
   then asserts the refusal. Small, honest, and consistent with [[BPR-0311]] —
   but it means the runner's ownership checks stay unwritten for Windows, so
   when [[BPR-0311]] does land, the work is still owed.
2. **Port the ownership checks now.** `os.getuid()` has no Windows meaning; the
   equivalent is the owner-private descriptor check the suite already has in
   `tests/sqlite_contract/platform_test_support.hpp` and the product has in
   `catalog_database.cpp`. This is real work and it pre-empts [[BPR-0311]]'s
   scope rather than following it.

Silently skipping the whole contract on Windows is a third option and the worst
one: it would leave a registered security contract that reports nothing on a
platform where the runner is nonetheless importable.

## Required final state

- One of the two dispositions above is chosen and recorded, with what it defers.
- `os.getuid()` and `pwd` are not reached on a platform that has neither,
  whether by refusing earlier or by having a real equivalent.
- The symbolic-link cases skip by name where the privilege is unavailable, as
  [[BPR-0375]] did for the capture contract and
  `epoch_catalog_storage_tests.cpp` does for the storage one.
- The remaining `AssertionError: 1 != 0` is diagnosed rather than absorbed.
- POSIX behaviour is unchanged. These checks are load-bearing there.

## Verification

```sh
py -3 tests/scripts/rfc0016_device_live_runner_tests.py
ctest --preset core-only --output-on-failure -R '^rfc0016_device_live_runner_security$'
```
