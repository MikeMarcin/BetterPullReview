---
id: "RFC-BPR-0054"
title: "Workspace Source Capture and Immutable Revision Material"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0003", "RFC-BPR-0004"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0054: Workspace Source Capture and Immutable Revision Material

**Status (2026-10-04): Auto-Verified.** [BPR-1187](../issues/60-done/BPR-1187-close-rfc-0054.md)
judged all ten criteria whole against the cards' recorded evidence, with runs on
macOS arm64 and the Linux and Windows stations; the
[execution ledger](../verification/RFC-BPR-0053-0061.md#bpr-1187-rfc-bpr-0054-closed-2026-10-04)
holds each row and its limits. RC54-03's junction escape was observed refused on
native Windows by BPR-1284. RFC-BPR-0053, 0003 and 0004 are Auto-Verified. No
owner verdict was needed. On Windows the held root cannot be renamed, so root
replacement is prevented rather than detected; no window or command rebinds a
moved project (BPR-1279). Since closure, [BPR-1279](../issues/30-in-progress/BPR-1279-offer-an-explicit-rebind-for-a-moved-or-replaced-cycle-folder.md)
offers that rebind, confirmed against a material check, as `bpr cycle rebind`
and the window's Rebind Folder.

## Outcome and decisions

Provide source material directly from a chosen local checkout, linked worktree, folder or file. Preserve the exact bytes observed, including uncommitted changes; a commit ID is optional provenance, not a substitute for those bytes. The original revision R0 is a real captured source state, not a synthetic all-added diff.

Capture is read-only against the user's source and index. Root/descendant identities, no-follow traversal, bounded chunking and explicit exclusions prevent an innocent review operation from executing repository code or escaping its selected scope. Unsupported encodings, LFS/filter transformations, sparse objects, symlinks, gitlinks and cross-platform name collisions receive truthful handling rather than a lossy fallback.

A regular recursive copy can observe a stable handoff but cannot prove a simultaneous filesystem snapshot against arbitrary writers. Record observed_stable versus immutable_source accurately, retry detected instability finitely, and never show a half-captured revision as Latest. Store raw byte hashes separately from algorithm-tagged Git object identities.

Extend existing source/diff access through exact material providers and a separate cycle revision catalog. Retain scoped manifests, explicit deletions and coverage. Watchers only signal possible change. They do not advance revisions, move the current source view or dispatch recipients. Capture may be expensive, so use asynchronous bounded work with cancellation and stable previous content.

## Normative contracts

The detailed contracts [SNAPSHOTS.md](assets/bpr-review-cycles-v1/specs/SNAPSHOTS.md), [LIMITS.md](assets/bpr-review-cycles-v1/specs/LIMITS.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC54-01:** Open Git/linked-worktree/dirty checkout, plain folder and single-file scopes without network/authentication or a prerequisite commit.
- **RC54-02:** Capture changes neither source bytes, user index nor refs and runs no hooks, filters, textconv, lazy fetch or repository scripts.
- **RC54-03:** Reject root replacement, symlink/junction escape and unsafe descendant paths under native filesystem semantics.
- **RC54-04:** Detected instability retries finitely; capture-quality metadata never asserts global atomicity from ordinary copying or two matching passes.
- **RC54-05:** Scope exclusions, sparse/missing content, explicit deletions and new scope versions remain distinguishable in manifests/comparisons.
- **RC54-06:** CRLF/LF/BOM/no-final-newline, raw hashes, encoding offsets, file mode and path identities are preserved without lossy normalization.
- **RC54-07:** Binary, unsupported encoding, LFS/filter-managed, symlink/gitlink and platform-collision cases remain visible with accurate capability limits.
- **RC54-08:** Large enumeration/capture/diff access is paged/cancellable and measured without UI-thread hashing or unbounded core state.
- **RC54-09:** Retained material resolves the exact captured bytes after external edits, deletion or restart; corruption/missing pins are not replaced by live files.
- **RC54-10:** Explicit workspace rebind/import mapping prevents the same path/alias in another checkout from silently becoming the review subject.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
