---
schema: "implement-rfc.card.v1"
id: "BPR-0705"
title: "Compose multiple live windows over one profile runtime"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696", "BPR-0697", "BPR-0698", "BPR-0699", "BPR-0700", "BPR-0701", "BPR-0702", "BPR-0703", "BPR-0704"]
---

# Compose multiple live windows over one profile runtime

## Scope

Make the real Profiles > New Window gesture allocate a core window context and
compose a window session in the existing profile runtime. Ordinary Open Profile
activates a live window. One runtime owns profile roots, auth/vault/transports,
queue, appearance/behaviour preferences, logging, cache actors, shared drafts and
workspace recovery/cleanup. Each window owns its Qt surface, read/epoch/workbench/
diff session, selected PR/comparison, navigation, layout, mutation jobs and agent
invocation/chat/MCP session. Callbacks use exact window context and weak lifetime;
the first window is never the profile-wide projection sink or cleanup owner.

Startup admits the profile owner before requesting its first authoritative
management context; the desktop path must not create a second synthetic window
identity. Compatibility standalone fixture startup remains explicit. Closing one
window detaches its consumers and drains its work while siblings remain live;
profile/application quit drains windows before shared services. Keep one Settings
presentation per profile, with the invoking profile's accepted state.

MCP snapshots are private, derived from typed window/generation identity, and
cleanup removes only the exact owned session. Workspace recovery/parent cleanup
runs once per profile. Do not implement additional windows by starting two full
applications on the same root or disabling existing agent features to avoid
ownership work. Profile stores are reused through the established seams.

This slice proves live window composition. Durable window-session destinations,
restart restoration and cross-process owner routing remain subsequent bounded
work. Never bind process-local compatibility identities to fabricated durable
window rows. Preserve the existing Default layout/defaults behaviour while keeping
additional live window geometry independent. Per-window ledger views may use
independent handles over the existing ledger; live same-review ledger projection
fanout remains later work and is not claimed by this course.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture decisions, CMake, new public acceptance course and
commits. Runtime worker owns app composition; a separate worker owns profile
management intents/host and Qt menu. Preserve all parallel edits.

## Acceptance

The actual New Window action opens two visible windows with different admitted
contexts in one profile runtime/root owner. They open different fixture PRs and
retain independent comparison, navigation, layout and read state. Shared profile
preferences update both without replacing window geometry. Settings opens once
per profile. Closing A while B has read/epoch work pending retires only A;
B completes and auth/cache/workspace services remain usable. Ordinary Open Profile
activates B without duplication; another New Window receives a fresh context.
Quit drains all sessions and closes profile services once. Stale/foreign callbacks
cannot project into a replacement window. Existing one-window public paths and
focused management/shared-service tests remain passing. Native attendance and
full RFC restoration/cross-process acceptance remain open.

## Evidence

Read-only runtime scout identified the existing single-window map, missing menu
intent and first-window-owned callbacks/cleanup. Service foundations are complete;
no implementation or new two-window acceptance check has run for this card.
