---
id: BPR-0360
title: Owner-private catalog objects are unusable inside an AppContainer
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0362]
---

# Owner-private catalog objects are unusable inside an AppContainer

[[BPR-0350]] found this defect in the test suite's DACL helper and fixed it
there. The product has the same defect in its own code, and [[BPR-0350]]'s
diagnosis named only `tests/sqlite_contract/platform_test_support.hpp`.

`src/adapters/sqlite/src/catalog_database.cpp` builds the identical shape.
`private_security_attributes::initialize` creates one `ACCESS_ALLOWED_ACE` for
the token user with `FILE_ALL_ACCESS`, marks the descriptor
`SE_DACL_PROTECTED`, and hands it to `CreateFileW` / `CreateDirectoryW`.
`validate_private_security` then requires `information.AceCount != 1U` to be
false — exactly one ACE, exactly the token user.

Under a lowbox token that object is unreachable by the process that made it. The
restricted access check additionally requires the DACL to grant the container's
own package SID or `ALL APPLICATION PACKAGES`, and a protected owner-only
descriptor names neither. The owner is the invoking user rather than the package
SID, so the implicit owner path cannot satisfy the restricted pass either.

## The descriptor defect is proven

Two directories under `build/core-only`, one given the product's exact
descriptor shape and one left with the container's inherited entries:

```
icacls probe_op\ownerprivate /inheritance:r /grant:r <user>:(OI)(CI)F
  -> JAEGAW\mikem:(OI)(CI)(F)                                   one ACE, protected
  -> probe_op\inherited: S-1-15-2-2501375856-…:(I)(OI)(CI)(M)    the container's grant
```

| case | inside the container | outside |
|---|---|---|
| protected owner-only DACL (the product's shape) | **DENIED** | writable |
| container-inherited DACL | writable | writable |

The outside column is what makes the inside column evidence: the descriptor is
not malformed and the user is not lacking rights. The boundary is the cause.

## It is latent, and this card was wrong about that

An earlier revision of this card claimed the four SQLite storage tests fail on
this defect. They do not, and the claim was withdrawn rather than left standing.

`open_state_directory` never reaches the descriptor. It first calls
`validate_ancestor_chain`, which opens every ancestor starting at `\\?\C:\`, and
inside the container the drive root itself is denied. Measured with a negative
control: `\\?\C:\`, `\\?\C:\Users`, and `\\?\C:\code` are all `win32=5` inside
and open fine outside. [[BPR-0362]] owns that, and it is strictly in front of
this card.

So this defect currently has **no observable symptom**. A fix would be a change
to a product security descriptor that no command in the tree can exercise, which
is precisely the kind of unverifiable claim this project refuses. An
implementation was written and reverted for that reason; it should be rewritten
after [[BPR-0362]] makes the path reachable, so the fix arrives with evidence
rather than ahead of it.

## This is not a test accommodation

It would be easy to read this as bending product security to satisfy a test
harness, and it should be resisted on that reading. It is not one.

An MSIX-packaged Windows application **runs inside an AppContainer**.
RFC-BPR-0014 owns Windows packaging. If BetterPullReview is ever packaged that
way, this defect stops the epoch catalog and the review ledger from opening at
all, for every real user, with `invalid_permissions`. The container is the
identity the process actually runs as, and an object that excludes the process's
own identity is a bug irrespective of who noticed it.

## Required final state

- When the process runs in an AppContainer, an owner-private catalog object also
  grants that container's package SID, read from `TokenAppContainerSid`, with
  the same inheritance the user entry gets.
- `validate_private_security` accepts exactly that pair and nothing wider. No
  second user, no group, no `ALL APPLICATION PACKAGES`, and no unprotected
  descriptor becomes acceptable. The entries are checked as a set rather than by
  the order `AddAccessAllowedAceEx` wrote them.
- Outside a container the descriptor is byte-for-byte what it is today: one ACE,
  protected, token user only. The POSIX path is untouched.
- A test fails without the fix, exercising the product's validator rather than
  the test helper's copy of the same rule.
- The four SQLite storage tests pass — which requires [[BPR-0362]] first, and is
  how this fix becomes observable at all.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^(sqlite_epoch_catalog_storage_contract|sqlite_epoch_catalog_behavior_contract|sqlite_review_mutation_recovery_storage|sqlite_review_ledger_storage)$'
```

Run the same selectors outside the launcher as the negative control, so a pass
inside is evidence about the boundary rather than about the machine.
