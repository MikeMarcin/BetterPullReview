---
id: BPR-0370
title: The ledger tests' make_private is a silent no-op on Windows
status: Ready
rfc: RFC-BPR-0007
depends_on: []
---

# The ledger tests' make_private is a silent no-op on Windows

`sqlite_review_ledger_storage` and `sqlite_review_mutation_recovery_storage`
fail at their first step:

```
FAILED: the bounded state directory opens
FAILED: owner-private recovery store opens
```

The product is right to refuse. The test never made the directory private.

`scratch_directory::make_private()` in
`tests/sqlite_contract/review_ledger_storage_tests.cpp:145` does:

```cpp
std::filesystem::permissions(
    Path,
    std::filesystem::perms::owner_all,
    std::filesystem::perm_options::replace,
    ignored);
```

On Windows the MSVC standard library implements `permissions` by toggling
`FILE_ATTRIBUTE_READONLY`. It sets no DACL at all. The call succeeds, the error
code is zero, and nothing the product checks has changed — so
`open_state_directory` correctly rejects a root that is not owner-private.

The `std::error_code` is discarded into a variable named `ignored`, so even a
real failure would be silent. Here there is no failure to see: the call reports
success and does nothing.

## Measured

```
before: is_owner_private = false
std::filesystem::permissions error = 0
after : is_owner_private = false
make_owner_private -> true, is_owner_private = true
```

The last line is the contrast that makes the rest evidence. The suite already
has a Win32 helper that does the job —
`tests/sqlite_contract/platform_test_support.hpp`'s `make_owner_private`, which
[[BPR-0350]] taught about AppContainer package SIDs — and the epoch-catalog
tests use it. The review-ledger tests do not.

## Why this was invisible until now

POSIX `permissions` really does set mode `0700`, so on macOS and Linux
`make_private()` is correct and these tests pass. And on Windows the tests never
reached this assertion before, because [[BPR-0368]] and [[BPR-0369]] were
failing earlier in the same path.

## Required final state

- The review-ledger and review-mutation-recovery scratch directories are
  genuinely owner-private on Windows, through the same
  `platform_test_support.hpp` helper the epoch-catalog tests use, rather than a
  second implementation.
- POSIX behaviour is unchanged: `0700` there, exactly as now.
- The `std::error_code` is not discarded into `ignored` where a failure would
  matter — a helper that cannot make the directory private should fail the test,
  not proceed.
- `sqlite_review_ledger_storage` and `sqlite_review_mutation_recovery_storage`
  get past `the bounded state directory opens` and
  `owner-private recovery store opens`. What they then do is not predicted here.

## Not in scope

The product side. `open_state_directory` is behaving correctly; the defect is
entirely in the test's setup.

## Verification

```sh
ctest --preset core-only --output-on-failure \
  -R '^(sqlite_review_ledger_storage|sqlite_review_mutation_recovery_storage)$'
```

Run the binaries directly as well as through the launcher, so a container effect
is not mistaken for a fix.
