---
schema: "implement-rfc.card.v1"
id: "BPR-0130"
title: "Simplification and closeout preparation for RFC-BPR-0007"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0129"]
closeout_for: ["RFC-BPR-0007"]
---

# Simplification and closeout preparation for RFC-BPR-0007

Reconcile the verification ledger against what was actually built, remove any
duplication the slices introduced, and prepare closeout.

**This card makes no closure claim.** RFC-BPR-0007 cannot become
`Auto-Verified` until RFC-BPR-0005 is, which requires RFC-BPR-0004,
RFC-BPR-0003, and RFC-BPR-0002, and therefore real Windows x64 evidence in
BPR-0036 and BPR-0053.

## Required direct evidence

- Every claim-matrix row carries its own evidence, and no two rows share a
  marker or cite another row's result.
- A full replay: `dev`, `core-only`, format, tidy, documentation, the
  dependency and runtime-surface scans, the pack validator, a zero workflow
  count, and `git diff --check`.
- Any remaining gap is a card in `backlog/` naming exactly what is missing.

## Outcome

**This card makes no closure claim.** RFC-BPR-0007 stays `Proposed`.

### Claim matrix reconciled

Twelve rows, each citing its own card and its own evidence. The result column
was extracted and read on its own, because bulk-editing a matrix has twice in
this pack given one row another row's marker. No row shares evidence with
another; `BPR-0124` and `BPR-0125` each appear in three rows, and each citation
names different evidence within those cards.

Two gaps were found and fixed while reconciling. The core-dependency row still
read "verified for the receipt policy" when carry-over and ranking had since
landed under the same preset. And BPR-0128's public-path evidence had no row at
all, so the strongest end-to-end claim in the RFC — a receipt written by one
execution of the real binary and found by a later one — was recorded only in a
commit message. It has a row now.

### Simplification

Nothing was extracted. The one candidate is the `opaque` test helper, which
appears in ten test files — but seven of those predate this RFC, so it is a
pre-existing pattern rather than duplication these slices introduced, and
rewriting ten unrelated test files to hoist a four-line helper is churn, not
simplification.

The version-1 schema is deliberately written twice: once in the migration and
once in the test that builds a version-1 database. That is not duplication to
remove. If the test reused the production statements it would only be checking
that the code agrees with itself; writing them out pins the historical schema,
which is exactly what a migration test needs.

### Replay

`core-only` 42/42. `dev` 103/103. Format, tidy, documentation verifier, the
binary-dependency and runtime-surface scans, the pack validator, a zero
workflow count, and `git diff --check` all pass.

**The `dev` replay was not clean on the first attempt.**
`application_epoch_maintenance_public_path` failed once, passed on the next
full run, and passes 4 of 4 in isolation. That is recorded as BPR-0133 with its
captured evidence rather than smoothed over by reporting only the second run.
It is a different failure from BPR-0119: the verdict shows every reported
conjunct healthy, which localises it to the maintenance-evidence conjunction
that the verdict line does not yet cover.

### Remaining gaps, all carded

| Card | What is missing |
|---|---|
| BPR-0131 | The automated stage budget outlasts the harness timeout, so a genuine hang is killed from outside and reports nothing |
| BPR-0132 | The ledger creates `state/v1` itself rather than through the catalog's hardened bootstrap, leaving two directory policies for one directory |
| BPR-0133 | The maintenance contract fails intermittently under suite load |

Three further gaps are recorded on the cards that found them rather than as
separate cards, because each is a known half of work already scheduled: the
UI-intent path (BPR-0127, needs the window from a later slice), the composed
store's placeholder scope (BPR-0128, needs the same), and the Windows crash
recovery equivalent (BPR-0126, belongs to the Windows campaign).
