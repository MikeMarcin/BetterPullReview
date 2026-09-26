# Combined main composition review — 2026-09-26

Assignment `/root/combined_main_integration`, run `rfc0052-20260925`, segment
`combined-main-integration-20260926`, mandate
`owner-20260926-unattended-worktrees`. This is the authorized main integration
round for BPR-0972 and RFC-BPR-0052 m2b, preserving BPR-0966 and BPR-1199.
It accepts no pending human/native criterion and interprets no new verdict.

## Candidate and landing

Main baseline `fe8a6937` already contains the eight BPR-0972 commits, BPR-0966's
accepted macOS workshop slot and BPR-1199's bounded sparse-Git repair. All 33
owned m2b commits landed in original order through `8f36d1cb`. The [complete
mapping](combined-main-integration-2026-09-26.json) records source and main SHAs,
subjects, changed paths and the six already-canonical adoptions excluded from
the original sequence. No product commit was skipped or aborted.

The only conflicts were the full/manifest source pins and adjacent review
comments in the round-trip composition contract. Both historical comments were
retained. Old main pins stayed deliberately stale until this review; no branch
pin was copied as evidence for combined main. Source auto-merges preserve both
branches. One newly joined declaration needed clang-format alignment only.

## Review of actual combined source

The production app manifest at baseline `fe8a6937` is byte-identical to the
accepted BPR-0972 candidate `149704df`; its full-source and manifest pins match.
The only production manifest changes through `8f36d1cb` are in
`github_application.cpp` and `review_submission_workflow.{cpp,hpp}`. The commits
are `9a22bbfe` (source `3161f459`), `3fa6d614` (`30748ed6`), `48543054`
(`44bd1266`) and `6fe51128` (`5f7bf9d1`). This implementer reviewed their complete
combined diffs, the shared Qt merge surfaces, core promotion and chat delivery.
This is an engineering self-review, not independent acceptance.

- The bar change changes the test-build capture order, not runtime admission.
- Whole-review invalidation gains specific explanatory text for closed,
  read-only and unavailable states. Its same selected-account/PR checks,
  retirement and provider construction remain; it grants no new authority.
- The defect-to-thread join is cached for one accepted immutable snapshot,
  invalidated with accepted read replacement and ledger retirement/failure.
  Ambiguous identities yield no thread controls. Menus retain an exact defect
  history; existing core commands retain their revision and identity checks.
- Notes use the existing ledger port and revision checks. Scope retirement
  clears accepted notes; stale scope, unavailable ledger and exhausted scope
  refuse gestures. File labels use accepted current/previous paths; missing
  historical path names remain BPR-1210 AC1, not an accepted fallback.
- Promotion opens the existing Comment target, then rechecks draft scope and
  anchor before a core revision-checked local edit. It copies only item text,
  keeps the source and does not call a mutation provider. Chat reply selection
  checks the current session and retained reply identity; delivery additionally
  checks the exact binding. Explicit Publish remains separate.
- The BPR-0972 FilesDelta path, indexed tab/restoration membership, retained
  queue item updates, same-head Overview history, semantic counters and exact
  sibling receipt refresh remain present. The failed exact-material reload
  stays out of missing-material restoration. Private-note changes still use
  the existing private-record notification with exact durable account/PR
  authority; their synchronous store work remains the BPR-1211 gap.
- Dif retains one widget, the accepted 64-pixel Compose workshop prime slot,
  the existing first-time Home slot and the new 40-pixel default corner.
  No sprite source, manifest, generated asset or shared Qt resource changed.

The production mutation branch remains
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
The final reviewed LF-normalized full-source digest is
`0e4c85336be0478ef95f35f06b62555e2d68b2fde0dfd34d809cb22dbf0cd108`;
the production manifest digest is
`7ef93d74ebe71c2bf8cb058188a2ddd922b7ccf7e77f211b73251e8faa494d68`.
The one formatting alignment is included in these values. Printing hashes is
not behavioral evidence; focused verification is recorded in the final handoff.

## Required pending boundaries

The frozen owner candidate remains source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
session `d7cf21b16656c6ec19c8787255aa4006`, cursor 36 at engineering handoff.
Main is a different candidate. The frozen app, shared Qt and m2b fixture tree
remain protected and untouched. No new verdict is read or attributed here.

BPR-1210 AC1 historical names, the twelve owner/native questions, BPR-1204's
terminal dependencies and Windows/Linux station evidence remain pending.
BPR-0972's prior 467/471 Debug matrix and diagnosed baseline failures stay
recorded. BPR-0967 owns optimized hardware budgets; BPR-1211 owns synchronous
ledger I/O; `host_load` remains `not_verified`. No full RFC, performance,
independent, native, live-provider, publication or release acceptance follows.
