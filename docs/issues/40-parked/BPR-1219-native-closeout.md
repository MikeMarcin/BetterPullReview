---
schema: "implement-rfc.card.v1"
id: "BPR-1219"
title: "Qualify structured diagnostics and reconcile RFC acceptance"
status: "Parked"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1218"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"41cce4bae80542a56fa842e8c98f00ffb9d9d57d","evidence":{"kind":"observation","detail":"Prior native logging/lifecycle/package evidence retained at its exact revisions; revised UI passes Mac public/Qt/native inspection, Linux 2/2 at 0fb249a2 and Windows changed public courses at 41cce4ba. Tour serializer Mac storage passes; optional Windows tour-storage target has a pre-existing missing platform include and is not a pass. Exact results and package limits: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC2","outcome":"pending","revision":"41cce4bae80542a56fa842e8c98f00ffb9d9d57d","evidence":{"kind":"observation","detail":"Original files accepted at cursor23, decision 413e836b-5898-4aeb-a48c-43b21595d54b at source7ab11993. Revised controls/export are protected at cursor31, source0fb249a2, SHA256 9c4ae94b217cfd20585c648dca0bd5e74ce53adad91fc40a8cb4966a459e5186; two actual new human verdicts required. No live sign-in."}},{"criterion":"AC3","outcome":"pending","revision":"41cce4bae80542a56fa842e8c98f00ffb9d9d57d","evidence":{"kind":"observation","detail":"Full criteria reconciled in BPR-0601; BPR-0603 and RFC/index remain open. Required new attended verdicts and accepted M5 main landing pending under mandate r2."}}]
parked_on: "human-review"
---

# Qualify structured diagnostics and reconcile RFC acceptance

Reconcile all RFC-BPR-0027 criteria and platform, dependency, packaging, native and attended obligations against exact candidates; preserve gaps until evidence exists.

## Acceptance

- AC1: Run focused integration and native logging/lifecycle/control courses on each claimed platform; bind dependency and package provenance.
- AC2: Prepare required attended review with exact candidates and pending decisions; no live sign-in without separate authority.
- AC3: Reconcile BPR-0603 and BPR-0601, RFC/index and all full criteria, land accepted scope on main.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.


## Native evidence and attended handoff

Final source `7ab119933439878d858100410083080ab4d1ea7b`.
[Verification](../../verification/RFC-BPR-0027.md) and
[durable checkpoint](../10-backlog/BPR-0601-close-rfc-0027.md#m5-checkpoint--human-pending-writer-released)
bind the actual native/course/package outcomes and preserve all limits.
Three attended criteria are prepared in session
`c1565558e5fd3fdc277e62622604873b`, cursor 19. No verdict supplied; M5 and the
full RFC remain unaccepted, so main integration remains pending. Source ownership
is released to the coordinator after the committed checkpoint; only the handed
off local review server remains active.


## Attended hand-back and UI revision

At cursor 23 the owner accepted `BPR-1219-files` (decision `413e836b`, source
`7ab11993`, binary SHA-256 `3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`).
Controls were rejected with “Settings should look more like the AllTheThings
settings selectors.” Export was rejected with “This seems much more complicated
than necessary, see the simplicity of the AllTheThings Help options”. The two
controls images and one Help-menu image were inspected locally. The reference
checkout was read only at `2ba1b33c9886a3e20865801581a26b5ba73f2700`.

Assignment `/root/rfc0027_m5_revision`, mandate r2, owns the bounded repair.
Paired capture buttons and direct Help export actions retain the existing
barriers and exact frozen selection. New controls/export acceptance remains
pending. The original files candidate and copied app are preserved. The shared
panel path caused `resume-development` to release its protection flag as well
as controls (the tool combines paths and candidate selectors); its verdict and
identity were unchanged. The old app remains reserved by this handoff and is
not overwritten; replacement candidates use a separate copy.


## Revised attended handoff

UI revision `e0a870b3`, merged main candidate `0fb249a2`, and separate MSVC
serializer comparison correction `41cce4ba` are committed. The
[latest checkpoint](../10-backlog/BPR-0601-close-rfc-0027.md#m5-revision-checkpoint--human-pending-writer-released)
records exact native results, the two revised protected criteria at cursor31,
original accepted files identity, copied apps, server ownership and next action.
Only the two replacement controls/export verdicts are requested. The original
files acceptance remains preserved. Full M5 acceptance and main landing remain
pending; required human review is not self-certified.
