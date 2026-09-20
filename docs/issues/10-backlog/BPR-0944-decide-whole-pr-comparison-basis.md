---
schema: "implement-rfc.card.v1"
id: "BPR-0944"
title: "Decide Whole PR base-tip versus merge-base comparison semantics"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"source":"BPR-0943 exact selected endpoint diagnosis","decision_required":"product comparison contract"}
---

# Decide Whole PR base-tip versus merge-base comparison semantics

The current contract retains and selects the exact latest base-branch tip and
head for Whole PR. GitHub changed-file metadata describes a merge-base/head
comparison. In the owner-observed diverged target, the exact selected base tip
had no entry at the modified path, while the merge base and head both had
different, verified nonempty regular blobs. Provider status was M with no
previous path. BPR-0943 makes the missing selected endpoint explicit; it must
not silently substitute a different commit or rewrite retained anchor identity.

This is a separate product/architecture decision, not authorized by the current
repair. No private paths, OIDs, bytes or screenshot pixels are retained here.

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

## Evidence

Scoped read-only diagnosis in the selected-diff d6/r21 round. This card is a
pending decision; neither website equivalence nor merge-base support is claimed.
