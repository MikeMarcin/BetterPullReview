---
schema: "implement-rfc.card.v1"
id: "BPR-0704"
title: "Share revision-aware drafts and retain conflicting editor candidates"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690", "BPR-0697"]
acceptance: [{"criterion":"Shared draft scope/revision admission, preserved conflicts and shipped editor path","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Core/host/Qt narrow tests0.08/0.35/0.42s pass including scope aliasing,line0,selectreuse,same-textack and post-fanout retirement settlement. All6workerTUs/final4TUs and appTU tidy pass; both architecturechecks pass. Rebuilt public editor/publication13.12s passes stale revision/foreignscope zerohostrequests, retry/losteffect/deferredterminals. Dedicated isolation77.84s, final mutation-source guard0.700s and docgate pass. Two mediumreview blockers fixed and narrowrechecked. Compatibility context is processlocal; actual desktop multiwindow binding remains later."}}]
---

# Share revision-aware drafts and retain conflicting editor candidates

## Scope

Introduce pure revision-aware draft edit/discard/publish admission and a bounded
profile/review-scoped application owner for accepted draft allocation/state.
Window consumers retain local candidates separately from shared accepted drafts.
A stale edit or publish returns a typed conflict with current accepted state;
no mutation request may follow refused publication admission. Keep exact profile,
account, review and window lifecycle identity, bounded text/records/consumers and
consumer-aware shutdown. Existing mutation grants and terminal authority remain.

Qt presents accepted state and rejected candidate separately, preserving exact
local text across shared refresh and late completion. Explicit reload/reapply
resolves a conflict; presentation alone cannot silently replace the candidate.
Use this owner in the current application facade, with actual same-profile
multi-window construction supplied in later runtime integration. This card does
not add durable editable drafts or change publishing permissions.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Worker owns draft core/application/Qt and focused tests. Primary owns app wiring,
CMake, acceptance and bookkeeping; parallel epoch/ledger work is preserved.

## Acceptance

Two consumers editing revision N produce one accepted N+1 and one preserved
conflicting candidate. Stale discard/publish and retired/foreign consumers cannot
change accepted state or submit effects. Shared refresh, selection changes and
late terminals cannot erase rejected candidate bytes. Explicit conflict resolution
uses the latest accepted revision. Existing draft and exact publication contracts
remain passing, with a composed editor/publication path using the new owner.

## Evidence

Pure draft scope/revision admission and the bounded owner now drive the shipped
editor facade. Automatic exact line/reply selection reuses active drafts; retry
reads retain IDs/revisions. A monotonic owner scope prevents PR/account replacement
from aliasing a retained candidate. First-line zero is accepted. Qt keeps rejected
text through updates and uses explicit successful-command acknowledgement,
Reload and Reapply. The facade uses a core-admitted process-local compatibility
window; authoritative desktop window binding remains later runtime integration.

Medium review found same-text reapply acknowledgement and post-callback admission
failures. Both are corrected with focused tests; refused publication settles back
to drafting for surviving consumers. Core/host/Qt checks pass (0.08s/0.35s/0.42s),
all six worker TUs and final four-TU corrections pass tidy. Both architecture
checks and unchanged draft/submission core tests passed. App TU tidy passes.
The rebuilt public editor/publication course passes (13.12s), including stale
revision/foreign scope producing zero mutation-host requests, stale-head refusal,
lost effects, deferred terminal and retry. Mutation-source bypass checks pass
(0.738s); documentation gate passes.

The combined editor course exposed a duplicate exhaustive symbol scan with a
75-second limit; the independent live_mutation_isolation test already owns that
same scan with a 300-second limit. The duplicate invocation is removed while
source guards and the standalone scan remain. Eleven manifest binaries were
missing from dev (their earlier checks used core-only); those exact targets are
now built. The dedicated scan passes (77.84s); final mutation-source guard (0.700s) and
documentation gate pass.
The public fixture also now uses the existing owner-private directory helper.
