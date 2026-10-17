---
schema: "implement-rfc.card.v1"
id: "BPR-0944"
title: "Investigate Whole PR comparison basis and preserve available source"
status: "Backlog"
rfcs: []
depends_on: []
priority: P1
metadata: {"source":"BPR-0943 exact selected endpoint diagnosis","decision_required":"product comparison contract","owner_deferral":"2026-09-20 RFC-0013 closeout amendment; investigate later, do not implement in the selected-diff closeout","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency"}
---

# Investigate Whole PR comparison basis and preserve available source

## Comparison basis decided by [[BPR-1560]] — 2026-10-16

The owner reported wrong Whole PR diffs on a pull request with merges from its
base and a force push (owner chat, 2026-10-16). [[BPR-1560]] adopts merge-base
semantics: Whole PR compares from the merge base GitHub names for the current
head and the base branch tip, which is what GitHub's Files changed and the file
list compare from. That card owns AC1-AC3 (decision, acquisition, unavailable
behaviour, the diverged, rename and force-push courses). AC4 and AC5, the
partial pane and the missing-earlier explanation in the left pane, remain here.

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Whole PR comparison basis, readable partial pane and missing-earlier explanation; the anomaly is not fixed.
No owner decision, native observation or repair is supplied by this transfer.

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
