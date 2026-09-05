---
schema: "implement-rfc.card.v1"
id: "BPR-0360"
title: "Owner-private catalog objects are unusable inside an AppContainer"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0362"]
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

## Correction — the descriptor is reachable after all

This card said `open_state_directory` never reaches the descriptor, so the fix
would have no observable symptom. [[BPR-0368]] disproved the premise: the
blocker was a doubled separator in `validate_ancestor_chain`, not the container,
and with it repaired the chain, the descriptor validation, and the filesystem
check all pass.

Measured uncontained, every check this card would change already passes today:
owner is the token user, the descriptor is protected, there is exactly one ACE,
its flags and mask are exact, and its SID is the token user.

So the work here is now observable and can be written with evidence rather than
ahead of it. What has **not** changed is that the descriptor is genuinely
unusable from inside an AppContainer — that measurement stands on its own, with
its negative control — which is why this card is still open and still matters
for MSIX packaging.

## Implemented and proven contained — 2026-08-22

[[BPR-0362]] changed how this card becomes observable, and the change is
recorded there: under option 3 the four SQLite storage tests run
*uncontained* on Windows, and this defect exists only inside a container. So
the evidence is a contained probe of the product's own code rather than those
four tests.

### The product

`private_security::load` now also asks the token `TokenIsAppContainer`, and
when the answer is yes copies the package SID out of `TokenAppContainerSid`
beside the user SID. `private_security_attributes::initialize` writes one more
`ACCESS_ALLOWED_ACE` for that SID, `FILE_ALL_ACCESS`, with the same
inheritance flags the user entry gets, and only when contained; the descriptor
stays `SE_DACL_PROTECTED`. `validate_private_security` now expects exactly one
entry outside a container and exactly two inside, walks every entry with the
same per-entry checks it always made, classifies each trustee as the user, the
container, or a refusal, and requires the user once and the container exactly
`contained ? 1 : 0` times — a set, not an order. Nothing else about the
descriptor moved: outside a container the bytes are what they were, which the
uncontained control below reads back as one entry.

### The evidence, through the product's own validator

A `BPR_SQLITE_TESTING` seam, `detail::testing::probe_private_descriptor`,
creates a directory and a file below a given parent with the product's
`private_security_attributes`, writes to the file through a plain
`CreateFileW` with the product's share mode and flags, runs
`validate_private_security` on both handles, and reads each object's DACL
size back with `GetSecurityInfo` — the measurement is independent of the
validator it is checking. `sqlite_appcontainer_private_descriptor`
(`tests/sqlite_contract/appcontainer_private_descriptor_tests.cpp`,
Windows-only) calls it from a scratch directory under the build tree and
asks the token for its own containment separately, so the expected shape and
the observed shape come from different sources.

It is registered through `bpr_add_network_isolated_test` **without**
`WINDOWS_UNCONTAINED` — the only `sqlite_contract` test that must run inside
the container, because the container is the subject.

Before the product change, with the seam in place, contained:

```
appcontainer-private-descriptor: token_contained=1 product_contained=1 created=1 usable=0
  directory_valid=0 file_valid=0 directory_ace_count=0 file_ace_count=0 last_error=5
FAILED: an owner-private catalog file is writable by the process that made it
```

The same unfixed binary uncontained: `usable=1`, `ace_count=1`, passes. So the
failure is the boundary's, exactly as this card measured on 2026-08-18.

After the change, contained: `Passed`, with
`usable=1 directory_valid=1 file_valid=1 directory_ace_count=2 file_ace_count=2 last_error=0`;
uncontained: passes with `ace_count=1`.

### Verification

| claim | command | result |
|---|---|---|
| the contained probe passes only with the fix | `ctest --preset core-only -R '^sqlite_appcontainer_private_descriptor$'`, before and after | fails `usable=0 last_error=5` before; passes `ace_count=2` after |
| the descriptor is unchanged outside a container | the same executable run directly | passes, `ace_count=1` |
| the helper-asserted one-ACE shape still holds where the four tests run | `ctest -R '^(sqlite_epoch_catalog_behavior_contract\|sqlite_review_mutation_recovery_storage\|sqlite_review_ledger_storage)$'` (uncontained, as registered) | 3/3 pass; `sqlite_epoch_catalog_storage_contract` fails at [[BPR-0425]]'s assertion as before |
| the new registration and seam satisfy the standing gates | `ctest -R '^(test_network_isolation_manifest\|live_mutation_isolation_contract\|architecture_boundaries)$'` | 3/3 pass |
| the seam cannot reach a tests-disabled Release build | `probe_private_descriptor` added to `RELEASE_BINARY_FORBIDDEN_BYTES` in `tests/end_to_end/composition_contract_tests.py` | source-pinned; that contract runs in the Qt graph, which [[BPR-0412]] owns, and is recorded there to be replayed |
| format and whitespace | `clang-format --dry-run --Werror`, `git diff --check` | clean |

Two observations left where they belong rather than folded in:

- The two older SQLite seam symbols, `interrupt_next_bootstrap_before_publication`
  and `classify_sqlite_result_code`, are **not** in
  `RELEASE_BINARY_FORBIDDEN_BYTES`; only [[BPR-0086]]'s admission seam was
  ever listed. That is a pre-existing gap in the Release absence contract, not
  this card's, and is noted on [[BPR-0358]] for the Qt-graph replay to take.
- `protect_inherited_private_file`'s two-inherited-entries path inside a
  container has source-level evidence only; the probe exercises the protected
  directory and file shapes.

This slice was implemented by a bounded worker lane under an exclusive file
scope. The integrator reviewed the complete diff and reproduced the after-fix
contained run, the uncontained control, the three uncontained SQLite tests,
the standing gates, the format check, and `git diff --check` directly; the
before-fix contained output is the worker lane's record and agrees with this
card's own 2026-08-18 measurement of the same denial.
