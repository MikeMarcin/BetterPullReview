---
schema: "implement-rfc.card.v1"
id: "BPR-0241"
title: "Verify and close RFC-BPR-0007"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0115","BPR-0228","BPR-0234","BPR-0238"]
closeout_for: ["RFC-BPR-0007"]
---

# Verify and close RFC-BPR-0007

RFC base: `ea3a3b8223c00f9e0999bf0ab2882496abf65843`.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [x] | Read, accepted, excluded, and seen states survive restart. | Composed review-ledger host over SQLite with BPR-0228's two-process public gesture trace | Run `ctest --preset dev -R '^(application_review_ledger_host\|sqlite_review_ledger_storage)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | Read, accepted, excluded, and seen written through semantic intents return identically in a second real process; direct fixture-store access cannot satisfy the trace. | Read-accepted-excluded-and-seen-survive-restart row plus BPR-0228 public trace contract in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Read, accepted, excluded, and seen states survive restart.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Changed material invalidates only affected receipts. | Core mixed-material receipt carry-over plus persisted host projection | Run `ctest --preset dev -R '^(core_review_ledger\|application_review_ledger_host\|sqlite_review_ledger_storage)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | Advancing mixed material invalidates only receipts whose exact material identity changed and retains every unchanged receipt. | Receipt-invalidation-is-precise row in the claim matrix in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Changed material invalidates only affected receipts.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | New or edited comment versions reopen the correct conversation. | Core comment-version transitions and SQLite conversation receipt restart | Run `ctest --preset dev -R '^(core_review_ledger\|sqlite_review_ledger_storage)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | A new comment and an in-place edited version reopen only their conversation as unread/outdated while unrelated conversations retain state. | New-or-edited-comments-reopen-the-right-conversation row in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “New or edited comment versions reopen the correct conversation.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Database deletion loses only private state and recreates a clean ledger without changing GitHub. | Ledger database-loss isolation and BPR-0228 public composition trace | Run `ctest --preset dev -R '^(sqlite_review_ledger_storage\|application_review_ledger_host)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | Deleting the ledger, WAL, and SHM loses only private receipts, recreates a clean schema, and leaves the independent GitHub fixture/projection unchanged. | GitHub-remains-unchanged row in the claim matrix in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Database deletion loses only private state and recreates a clean ledger without changing GitHub.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Abrupt termination and WAL recovery preserve committed state and discard incomplete transactions. | Forked-child SIGKILL and SQLite WAL recovery contract | `ctest --preset dev -R '^(sqlite_review_ledger_storage)$' --output-on-failure` | After SIGKILL, committed records survive restart and the record inside the abandoned transaction is absent. | Crash-recovery-is-sound row in the claim matrix in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Abrupt termination and WAL recovery preserve committed state and discard incomplete transactions.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Migrations are forward-only, versioned, backed up where policy requires, and tested from every supported prior schema. | Versioned SQLite migration matrix from every supported prior schema | `ctest --preset dev -R '^(sqlite_review_ledger_storage)$' --output-on-failure` | Each older schema migrates forward with all records intact; a newer schema is refused without modification and no reverse migration exists. | Migrations-are-forward-only-and-complete row in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Migrations are forward-only, versioned, backed up where policy requires, and tested from every supported prior schema.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Strong identity prevents state reuse across host, account, repository, PR, epoch, and material boundaries. | Core scope identity plus cross-host/account/repository/PR/material SQLite isolation | `ctest --preset dev -R '^(core_review_ledger\|sqlite_review_ledger_storage)$' --output-on-failure` | Reused PR numbers, paths, and comment IDs under identities differing by one authority field never expose another scope's private state. | No-cross-identity-leak row in the claim matrix in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Strong identity prevents state reuse across host, account, repository, PR, epoch, and material boundaries.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | `Next actionable` is deterministic and explains its ranking. | Deterministic core queue ranking and BPR-0228 composed actionable projection | Run `ctest --preset dev -R '^(core_review_ledger\|application_review_ledger_host)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | Original, reversed, and tied inputs yield the same order; each item carries the exact deciding explanation and the public workbench follows it. | Next-actionable-is-deterministic row plus BPR-0228 composition contract in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “`Next actionable` is deterministic and explains its ranking.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Receipt/actionability transitions are directly replay-tested without SQLite/Qt, while host persistence tests and Qt projection tests prove the complete request/completion/delta path. | Receipt intent to persistence request, typed completion, delta, and Qt projection through BPR-0228 | Run `ctest --preset dev -R '^(core_review_ledger\|application_review_ledger_host\|qt_review_queue)$' --output-on-failure`, then execute BPR-0228's two-process public trace from receipt/actionability gesture through store completion and window projection, followed by restart. | The trace observes intent, store commit, accepted completion, and incremental Qt delta in order; no SQLite/Qt bypass can mark state directly. | Ledger-decision-and-effect-split row plus BPR-0228 public trace contract in [RFC-BPR-0007 verification ledger](../../verification/RFC-BPR-0007.md#claim-matrix); final replay record keyed by “Receipt/actionability transitions are directly replay-tested without SQLite/Qt, while host persistence tests and Qt projection tests prove the complete request/completion/delta path.” | automated | No live blocker; final-head focused replay remains required. |

## Focused replay of the automated rows — 2026-09-02

Run against `build/dev` at `6dd7240a` (`cmake --build --preset dev` reported
`ninja: no work to do`, so the binaries are that tree's). Under the light
closeout contract this is the check per criterion, its result, and its
commit; a later commit that does not touch these surfaces does not reopen
them.

`ctest --preset dev --no-tests=error -R '^(core_review_ledger\|application_review_ledger_host\|sqlite_review_ledger_storage\|qt_review_queue\|application_review_ledger_composition_contract\|composed_application_hosts)$' --output-on-failure`
— **6/6 passed**: `core_review_ledger` 0.28 s, `application_review_ledger_host`
0.09 s, `sqlite_review_ledger_storage` 0.50 s, `qt_review_queue` 0.22 s,
`application_review_ledger_composition_contract` 41.86 s (this is
[[BPR-0228]]'s two-process public trace: receipt gesture through store
completion and window projection, then restart in a second process),
`composed_application_hosts` 1.62 s ([[BPR-0248]]'s linkage audit).

Every row above names one or more of those selectors and no attended
interaction; the per-criterion record is in the Closeout section of
`docs/verification/RFC-BPR-0007.md`.

## Final closeout record — 2026-09-02

- **Automated rows**: the replay above, 6/6 at `6dd7240a`. The same three
  ledger selectors also passed on the WSL2 station at `8948d61a`
  ([[BPR-0472]]), with the composition contract's `unavailable` course at
  2.07 s against the 2 m 31 s hang that card removed.
- **Blockers**: the ledger's recorded blocker — RFC-BPR-0005 and, through it,
  RFC-BPR-0002/0003/0004 with real Windows evidence in [[BPR-0036]] and
  [[BPR-0053]] — is discharged; all four are `Auto-Verified` and both cards
  are done. [[BPR-0130]] prepared the closeout and made no claim, as it said.
- **Half-verified rows**: the two claim-matrix rows that read "core half
  verified" since BPR-0121/0122 now name their persisted halves —
  [[BPR-0125]]'s host carry-over test and BPR-0228's newer-head reopen for
  invalidation, and the persisted, migrating conversation receipts
  ([[BPR-0124]], [[BPR-0238]]) for reopening — all of which ran in the replay.
- **Documentation gate**: `python3 scripts/verify_documentation.py --root .`
  → `DOCUMENTATION_OK`; `git diff --check` clean.
- **Independent review**: not selected. The surfaces changed since the slices'
  own reviews are BPR-0472's one-function repair in the SQLite adapter, which
  its own regression test and the composition contract cover on both
  standard libraries; no wire, authority, concurrency, or destructive
  migration surface moved. Recorded as an omitted optional lane, not a gap.

RFC-BPR-0007 is `Auto-Verified`.
