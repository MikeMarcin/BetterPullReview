---
schema: "implement-rfc.card.v1"
id: "BPR-0705"
title: "Compose multiple live windows over one profile runtime"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696", "BPR-0697", "BPR-0698", "BPR-0699", "BPR-0700", "BPR-0701", "BPR-0702", "BPR-0703", "BPR-0704"]
acceptance: [{"criterion":"Live profile-owned multi-window composition and compatible public paths","outcome":"pass","revision":"HEAD plus explicit BPR-0705 finish paths; final app SHA25616ad6d6e2cac0200a172795922aadbc23ef8d27e145ae06d99ef8069e8c2d0df","evidence":{"kind":"observation","detail":"Rendered actual2window course70.93s proves independent PR/comparison/navigation/layout, sharedprefs/Settings/drafts, closeA duringheldBread, freshcontext, exactstalerefusal andoneownerQuit. SevenexistingUI/storagecoursespass; repairedqueue4.20s androundtrip13.49s pass. Independentmediumreviews narrow lifetime/auth/queue/scanner fixes resolved. Finaltidy, bypass0.761s, architecture8.17/80.83s, tests-disabledRelease151.50s, mutationisolation81.21s anddocumentationpass. Nativeattendance/restoration/crossprocess/same-reviewledgerfanout remain later."}}]
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
the live runtime is now implemented and the public course is being exercised.

Control/UI and read-presentation work is source-complete in the current working
candidate. Focused management, Qt management and Settings tests pass (1.87s);
window-read host tests pass (0.36s), including accepted PR/error/clear projection
and preservation of profile-owned session/grant presentation. Canonical tidy and
format checks pass for those changed translation units. Independent medium
review found no evidence-backed blocker in these bounded paths; it did not
inspect the still-changing application runtime. Settings mounting requires the
profile owner to unmount the prior session before mounting another and to unmount
all sessions before shell teardown. The app builds and the public course now loads independent PR17/18 snapshots in
two actual windows using shared authentication. The full public course passes (70.93s); existing one-window and tests-disabled
Release integration checks are running before acceptance.

Runtime integration repairs separate window request identity from profile
credential-session provenance without weakening account/generation/lease checks.
Focused provider (10.81s), native HTTPS recorded transport (30.82s), native Git
recorded transport (32.10s), window host (0.37s) and native contracts (0.38/0.36s)
pass. Canonical affected translation-unit tidy passes. Independent medium review
found no blocker in the provenance seam, including unchanged mutation authority.

Independent runtime review identified and rechecked two repaired blockers: staged
setup acceptance now checks weak window lifetime before any member access, and
successful exact-session retirement releases attachment authority pins so existing
windows can adopt an accepted replacement account/server. The replacement-account
host regression rejects late old-account completion and passes (0.42s). Runtime
tidy passes. Startup registry lifetime repair passed management startup five
consecutive times (4.75s total); standalone startup passed (1.43s). These focused
results do not yet establish the complete two-window acceptance course.

The actual New Window course now passes all live-composition assertions (70.93s):
two visible exact contexts and one profile owner; independent PR17/18 snapshots,
comparison keys, semantic navigation and geometry; shared appearance and a single
Settings shell; close A while B has a held read terminal, then complete B; ordinary
Open activation without duplication; fresh window C; shared same-anchor draft
edits between B/C; stale context refusal; and one startup/shutdown in the profile
log. No publish gesture is used. The integrator inspected the rendered
`build/dev/profile-multi-window-review/window-a.png` and `window-b.png`, showing
actual separate comparison positions at 1100×700 and 1400×900.

The course allows 60s for native epoch/diff work and 180s overall after stack
inspection showed finite serialized Git capability probes; remaining gesture
waits stay 15s. It selects the actual Read modality and requires visible settled
comparison state. Fixture-only concurrent scan failure was corrected with bounded
nonthrowing reads; independent narrow review confirmed incomplete scans cannot
report clean. Window invalidation no longer clears the profile queue; the same
review confirmed sign-out retains profile-owned queue retirement. Architecture
boundary checks pass (8.17s/80.83s), strict direct-mutation bypass rejection passes
(0.759s), and final app/public-course canonical tidy passes.

Integration on the runtime candidate passed existing diff UI (14.70s), review
ledger (40.16s), epoch seed/reopen (51.86s), appearance gestures (84.70s), exact
setup destination (7.37s), profile startup (1.87s) and management (1.80s). Two
contained failures were repaired: standalone compatibility startup now records
its admitted focused window for queue activation, and the round-trip source
inventory requires the new exact window mutation-transport accessor. The affected
queue (4.20s) and round-trip (13.49s) rechecks pass. Final strict bypass rejection
passes (0.761s). A tests-disabled compile exposed one fixture-only variable;
its reference now stays in the testing branch and canonical app tidy passes.
The tests-disabled Release graph/build/executable contract passes (151.50s),
and dedicated mutation isolation passes (81.21s). Documentation and diff checks
pass. Native attendance, durable restoration, cross-process ownership and
same-review ledger projection fanout remain subsequent RFC work.
