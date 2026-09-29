---
schema: "implement-rfc.card.v1"
id: "BPR-1214"
title: "Restore conversation content after Discard and Back"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"Implementer native inspection during owner hand-back32 follow-up, 2026-09-26","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1207","BPR-1208"],"reservation":"docs/issues/_reservations/BPR-1214--BPR-1214--2caaece7bc57.json","mandate_revision":"owner-20260926-unattended-worktrees","scope":"Separate return-path projection defect; not new owner acceptance for the repaired editor or accepted thread controls"}
---

# Restore conversation content after Discard and Back

## Observed route

On macOS, source `c777a33d7467e83cfd0d2217329953e2044f1d41`, executable
SHA-256 `8589abf9dca824127e1531aa26699c9ee9d0456626fdca090c04eacf217b50ba`:

1. Seed the synthetic github-com defect course directly at its fixed runtime
   directory, then launch that same directory with epoch phase reuse and
   attended-launch. Connect the fixture account, open the synthetic PR and
   start Review.
2. Select a right/current-head source line, open Comment, and type local text.
   The editor is editable and Publish is available. Do not publish.
3. Click Discard, then Back. The comparison returns, but the Comments drawer
   is blank below its thread-availability text: cards, filters, map and detail
   are absent. Switching the source-line selection does not restore it.
4. Click Next defect. The drawer content returns and the comparison reveals
   the selected thread's exact line 60. No data loss or provider mutation was
   observed; persistence and the underlying cause have not been diagnosed.

The agent inspected the actual native screenshot and accessibility state. Its
private observation record is
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r3-rb0nbkl2/evidence/native-final-observations.json`.
That runtime's `review-seed-command.json` gives the exact seed argv and
`launch-review.py` gives the reuse argv. The proof app exited normally. The
older owner's r2 app and fixture were untouched.

## Scope and acceptance

This is a separate return/projection transition. BPR-1207's entry to an editable
Comment page and BPR-1208's accepted thread-command placement retain their
recorded results. Neither card claims this return path fixed. Activate this
card separately; do not infer a cause or widen authority from the observation.

- AC1 Reproduce Discard then Back on the public composed fixture and locate
  the state/projection transition that hides the existing conversations.
- AC2 Returning from an inline comment restores the Comments content permitted
  by the current conversation snapshot without requiring another selection.
  Preserve thread selection, current authority and exact code-reveal policy;
  a draft discard must not publish or erase the conversations.
- AC3 A focused public-path regression covers the reproduced sequence and
  recovery, with a negative control; inspect the actual native result. Keep
  unavailable platform evidence explicit and reuse unchanged command/placement
  checks rather than introducing a new owner approval checkpoint.
