# BPR-1452 bounded reconciliation checkpoint

Status: accepted branch disposition; BPR-1452 is Done as inapplicable.

Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh.
Assignment `/root/obsolete_hash`, local main; run `bpr-closeout-20261018`,
segment `obsolete-hash-1452`, mandate `owner-ready-backlog-20261018-r2`.

## Mandate and scope

Close ready/backlog work, leaving Insights/RFC-BPR-0086 to the separate owner.
The owner's amendment says older cards may be out of date or no longer relevant.
Only BPR-1452's conditional SHA-1 fallback is assigned here. Preserve all other
cards and RFC-BPR-0080's native/owner/performance/package obligations.

[Binding](binding.json) adopts the existing `hashing-replacement-v1` plan's
`adopt-sha1dc` outcome and both conditional exits without editing its source.
The previously accepted BPR-1446 evaluation satisfies its prerequisite;
BPR-1450's existing exact-candidate acceptance supplies the selected Accept
branch. This scope writes no BPR-1450 source or acceptance record.
[Selection](selection.json) preserves the coordinator recipe. No subagents.

## Candidate and result

Initial source/tracker revision:
`2b09f74f0287b1bd0a282b40cb31805a0973d293`.
Acceptance/finish commit: `45da1d9bdfac419a90119cf47922e20b0593a34a`.
This commit is the documentation candidate; product inputs are unchanged.
RFC_BASE_SHA remains `5620a9f15a391dde7750112294f7562f2f64a32f`.
Disposition: Q1 Accept, 2026-10-13, selected sha1dc; the first-party fallback
is inapplicable. Original BPR-1452 AC1–AC3 stay intact with no fabricated
fallback test or measurement. [Verification](verification.md) records the
decision, source composition and evidence reuse limits.

Changed paths are the BPR-1452 card (terminal move from Backlog to Done), this checkpoint,
its selection/binding, RFC-BPR-0080's progress paragraph and its verification
ledger's additive branch-disposition section. Product sources, authored plan,
imported packet, baseline and all Insights files remain unchanged.
The acceptance-driven finish directly reconciled the obsolete Backlog branch;
there was no product edit or implementation claim to promote to Ready.

Documentation verification passes before and after finish; scoped board
validation reports no errors; 14 focused source/decision assertions pass and
original AC1–AC3 remain intact. The milestone checkpoint helper reports
`action: stop`; semantic acceptance is the implementer's judgment of the
recorded conditional branch, not the helper's. Final evidence lives in
`/tmp/bpr-closeout-20261018/hash-relevance.json`, `hash-documentation-final.log`,
`hash-observation.json` and `hash-check.json`. The final runtime observation
binds the subsequent checkpoint commit without a self-referential commit ID.

## Ownership and continuation

Released at the milestone boundary. All task-owned source/board/doc-check
commands exited. No product build, GitHub call, desktop capture, review session,
protected candidate or background writer was started. No review server or
temporary build tree is handed off. No new human or independent decision is
needed for a branch already excluded by the recorded owner answer. No push.
Existing concurrent work and all protected candidates are preserved.

The final board query has no Mac-eligible Ready card. Suggested next bounded
backlog reassessment: BPR-0604, the optional schema-based capability-discovery
evaluation, whose dependency BPR-0559 is Done. Its card explicitly calls for
comparison and a recommendation before any separate implementation. Check its
current relevance under the older-card amendment; this milestone makes no
claim for its outcome. Insights/RFC-BPR-0086 remains excluded. The coordinator
owns the next dispatch; this implementer stops here.

RFC-BPR-0080 remains In Progress: full Windows BPR-1448 and Linux BPR-1449
station courses, conditional Q3 and BPR-1453 closeout remain open. The later
BPR-1617 native SHA-256 course does not discharge those original obligations.
