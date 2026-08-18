---
id: BPR-0360
title: Owner-private catalog objects are unusable inside an AppContainer
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# Owner-private catalog objects are unusable inside an AppContainer

[[BPR-0350]] found this defect in the test suite's DACL helper and fixed it
there. The product has the same defect, in its own code, and [[BPR-0350]] did
not know that when it was written — its diagnosis named only
`tests/sqlite_contract/platform_test_support.hpp`.

`src/adapters/sqlite/src/catalog_database.cpp` builds the identical shape.
`private_security_attributes::initialize` (around line 1660) creates one
`ACCESS_ALLOWED_ACE` for the token user with `FILE_ALL_ACCESS`, marks the
descriptor `SE_DACL_PROTECTED`, and hands it to `CreateFileW` /
`CreateDirectoryW`. `validate_private_security` (around line 1725) then requires
`information.AceCount != 1U` to be false — exactly one ACE, exactly the token
user.

Under a lowbox token that object is unreachable by the process that made it. The
restricted access check additionally requires the DACL to grant the container's
own package SID or `ALL APPLICATION PACKAGES`, and a protected owner-only
descriptor names neither. The owner is the invoking user rather than the package
SID, so the implicit owner path cannot satisfy the restricted pass either.

## Measured directly, with a negative control

Two directories under `build/core-only`, one given the product's exact
descriptor shape and one left with the container's inherited entries:

```
icacls probe_op\ownerprivate /inheritance:r /grant:r <user>:(OI)(CI)F
  -> JAEGAW\mikem:(OI)(CI)(F)                                    one ACE, protected
  -> probe_op\inherited: S-1-15-2-2501375856-…:(I)(OI)(CI)(M)     the container's grant
```

| case | inside the container | outside |
|---|---|---|
| protected owner-only DACL (the product's shape) | **DENIED** | writable |
| container-inherited DACL | writable | writable |

The outside column is what makes the inside column evidence: the descriptor is
not malformed and the user is not lacking rights. The boundary is the cause.

## What this costs today

Four tests fail on it, and they fail *after* [[BPR-0350]]'s helper fix let them
reach the product at all:

| test | symptom |
|---|---|
| `sqlite_review_mutation_recovery_storage` | `FAILED: owner-private recovery store opens` |
| `sqlite_review_ledger_storage` | `FAILED: the bounded state directory opens` |
| `sqlite_epoch_catalog_storage_contract` | hangs; `Timeout` at 30s |
| `sqlite_epoch_catalog_behavior_contract` | hangs; `Timeout` at 180s |

The two timeouts are new. Before [[BPR-0350]] both failed immediately at
`temporary root becomes owner-private`; now they get past that and stop making
progress. The hang is not diagnosed and must not be assumed to be this same
cause — a direct run under the launcher produced no output at all for seven
minutes, which is longer than either CTest timeout and is its own question.

## This is not a test accommodation

It would be easy to read this as bending product security to satisfy a test
harness, and it should be resisted on that reading. It is not one.

An MSIX-packaged Windows application **runs inside an AppContainer**. RFC-BPR-0014
owns Windows packaging. If BetterPullReview is ever packaged that way, this
defect stops the epoch catalog and the review ledger from opening at all, for
every real user, with `invalid_permissions`. The container is the identity the
process actually runs as, and an object that excludes the process's own identity
is a bug irrespective of who noticed it.

## Required final state

- When the process runs in an AppContainer, an owner-private catalog object also
  grants that container's package SID, read from `TokenAppContainerSid`, with
  the same inheritance the user entry gets.
- `validate_private_security` accepts exactly that pair and nothing wider. No
  second user, no group, no `ALL APPLICATION PACKAGES`, and no unprotected
  descriptor becomes acceptable.
- Outside a container the descriptor is byte-for-byte what it is today: one ACE,
  protected, token user only. The POSIX path is untouched.
- The four tests above pass, or the two timeouts are shown to have a different
  cause and carry their own card.
- A test fails without the fix. `tests/sqlite_contract/platform_test_support.hpp`
  already models the accepted shape after [[BPR-0350]]; the product's validator
  needs its own coverage rather than borrowing the test helper's.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^(sqlite_epoch_catalog_storage_contract|sqlite_epoch_catalog_behavior_contract|sqlite_review_mutation_recovery_storage|sqlite_review_ledger_storage)$'
```

Run the same selectors outside the launcher as the negative control, so a pass
inside is evidence about the boundary rather than about the machine.
