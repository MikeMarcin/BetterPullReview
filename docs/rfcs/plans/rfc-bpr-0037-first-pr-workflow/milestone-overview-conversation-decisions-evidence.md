# Overview and conversation decisions evidence

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-002`; producer
  `/root/rfc0037_m2`; mandate revision `1`.
- Binding: `overview-conversation-decisions` for BPR-0918, BPR-0920,
  BPR-0923 and BPR-0926, recorded in
  `assignment-rfc0037-dispatch-002.json` in this directory.
- Reconciliation: the prior `source-update-review-entry` return remains
  accepted. Its three cards are Done with their original revision-bound
  evidence and final milestone commits present. The handoff helper reports
  `reconcile` solely because completion changed the live card fingerprints
  captured before those cards moved to Done; no contradicting source,
  acceptance record or active writer was found.
- Accepted design candidate:
  `fe6ac3633a61bf5fc4abab0057f6546d412c6446`. BPR-0928 and BPR-0929 are
  canonical linked backlog cards; BPR-0918, BPR-0920, BPR-0923 and BPR-0926
  are Done at `d2282aba`, `5cec3bbf`, `969b8a60` and `3b442317`.
- Owned paths: the four bound design cards, this evidence/assignment location,
  canonical ID reservation artifacts, necessary linked follow-up cards, and
  scoped source/tests/docs selected after investigation.
- Completed checks: live RFC queue inspected; prior assignment, return,
  acceptance JSON and evidence reconciled; the new assignment binds valid,
  resolved source obligations and the accepted prerequisite milestone; the
  four cards were promoted and claimed; ID reservation history/counter were
  committed. The first thin Qt pass passed the Overview, conversation panel,
  conversation cards, workbench, review editor and review-submission public
  path selectors. `qt_composed_github_scale_100` failed at an existing
  native-host/CLI-warning assertion. A read-only scout reproduced it three
  times and traced it to pre-RFC-0037 BPR-0915 shared-sign-in expectation drift;
  because the monolithic test aborts before later assertions it is not counted
  as passing integration evidence.
- Selected Overview decision: a retained expanded reader inside the central
  Overview surface, with a natural compact preview, one scroll owner, a fixed
  Return to Files control and source-anchor rather than raw-scroll retention.
  Synthetic 39-byte, 8,131-byte and 203,412-byte bodies were exercised at
  800/1400 widths, 13/20-point fonts and 1×/1.25×/2× scale. Full selection/copy,
  end visibility and keyboard return passed; raw scrollbar replay failed six
  of eight long-body width/font combinations. The correction's digest-bound
  source-anchor strategy then preserved directed selection and kept the exact
  source anchor visible in 864/864 Return-to-Files/reopen courses across all
  ordered scale pairs; a disabled-adjustment negative control failed as
  expected. Ordinary anchors restored within one wrapped line, while 126
  document-end cases used the explicit fully-visible nearest-position clamp.
  These results bound BPR-0928 without claiming the shipped runtime is fixed.
- Selected conversation decision: one in-window Compose review page retains
  the existing workflow instance, while Discuss becomes PR-wide by default
  with explicit optional file/Unread/Unresolved filters and distinct loading,
  accepted-empty, filtered-empty, refreshing, failed, unavailable and
  replacement states. A synthetic composed probe measured the 220-pixel inline
  editor against 429/453-pixel minimum hints and 23-pixel body/workshop fields.
  An inherited absent-file anchor hid the loaded page despite one retained
  whole-PR card; clearing file/line anchors revealed three roots. This bounds
  BPR-0929. The owner's two reported box identities remain unobserved, and the
  whole-review target did not clip in the probe.
- Plan reconciliation: a future-only proposal to add BPR-0928/BPR-0929 as new
  work atoms was checked and rejected because a complete authored plan must
  preserve its obligation atom set. The live plan is unchanged. The cards are
  instead linked through the existing current exit requiring runtime follow-ups,
  the next milestone's explicit follow-up exit and BPR-0927's dependency ledger.
- Active owners/commands: the bounded Overview correction and route-record audit
  are complete, its disposable directory is removed and all leaf commands are
  quiescent. The independent reviewer accepted corrected candidate
  `fe6ac3633a61bf5fc4abab0057f6546d412c6446` with no blocking or non-blocking
  finding; review commands are complete.
- Verification: the focused Overview, conversation panel/cards, workbench,
  review-editor and review-submission public-path selectors passed; both native
  synthetic probes passed their stated positive checks, and the Overview
  retention negative control failed as intended. `DOCUMENTATION_OK`, 64
  documentation self-tests, RFC-scoped board validation (921 cards) and
  `git diff --check` pass at the accepted boundary.
- Handoff reconciliation: the final return checker reports `reconcile` because
  finishing the four cards changed the live obligation fingerprints captured
  while they were In Progress. The original binding, acceptance JSON, Done
  cards, follow-up links and quiescent ownership agree; this is expected
  completion bookkeeping drift, not an engineering rejection.
- Pending obligations: publish the milestone return while leaving BPR-0928,
  BPR-0929, BPR-0924 and BPR-0925 runtime work and target-native observations
  open for `thread-reading-integration`.
- Next discriminating step: bind the next authored milestone and implement its
  thread reading plus exit-bound BPR-0928/BPR-0929 runtime obligations.

Historical source/update/entry evidence remains in
`milestone-source-update-review-entry-evidence.md`; this milestone does not
repeat or reopen that accepted engineering.
