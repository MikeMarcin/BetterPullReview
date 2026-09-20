---
schema: "implement-rfc.card.v1"
id: "BPR-0944"
title: "Investigate Whole PR comparison basis and preserve available source"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"source":"BPR-0943 exact selected endpoint diagnosis","decision_required":"product comparison contract","owner_deferral":"2026-09-20 RFC-0013 closeout amendment; investigate later, do not implement in the selected-diff closeout"}
---

# Investigate Whole PR comparison basis and preserve available source

The current contract retains and selects the exact latest base-branch tip and
head for Whole PR. GitHub changed-file metadata describes a merge-base/head
comparison. In the owner-observed diverged target, the exact selected base tip
had no entry at the modified path, while the merge base and head both had
different, verified nonempty regular blobs. Provider status was M with no
previous path. BPR-0943 makes the missing selected endpoint explicit; it must
not silently substitute a different commit or rewrite retained anchor identity.

This is a separate product/architecture decision. On 2026-09-20 the owner
explicitly deferred this anomaly from RFC-0013's selected-diff acceptance and
asked to stop investigating this target now. The newly supplied screenshot was
inspected directly outside the checkout: the current missing-earlier refusal
clears both panes and places its explanation above them. The owner wants the
available right-side file to remain useful and the missing-earlier explanation
to occupy the left pane. This is requested future behavior, not an implemented
fix or evidence that the comparison-basis anomaly is resolved.

No private paths, OIDs, bytes or screenshot pixels are retained here. Preserve
the existing exact anchors until a comparison-basis decision authorizes a change.

## Acceptance

- AC1: Decide and document whether Whole PR means exact base tip/head, merge
  base/head, or an explicit choice; align naming and changed-file metadata with
  that decision without misrepresenting arbitrary selected pairs.
- AC2: If merge-base semantics are adopted, specify bounded acquisition,
  authorization, anchor identity/durability, history/rewrite and unavailable
  behavior before implementation. Do not replace existing accepted anchors.
- AC3: Exercise diverged histories, path removal from a newer base, renames,
  exact historical/reversed pairs and provider-status mismatch through the
  production composition and rendered UI using safe generated material.
- AC4: When the earlier selected material is unavailable but the later file is
  verified and readable, preserve/render the available right-side file rather
  than clearing both panes. Keep the partial/unavailable comparison explicit;
  do not fabricate a two-sided diff, correspondence or publication anchor.
- AC5: Put the missing-earlier explanation in the left pane, with accessible
  text and appropriate dark/light/high-contrast presentation. Prove loading,
  cancellation, file/epoch changes and recovery cannot retain stale source or
  stale explanations through the production composition and safe native renders.

## Evidence

Scoped read-only diagnosis in the selected-diff d6/r21 round and the owner's
2026-09-20 closeout amendment. This card remains Backlog for later investigation
and implementation; neither website equivalence, merge-base support nor partial
pane rendering is claimed. The deferral closes the amended target obligation,
not the anomaly, and does not waive other RFC or canonical predecessor gates.
