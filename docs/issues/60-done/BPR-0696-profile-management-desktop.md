---
schema: "implement-rfc.card.v1"
id: "BPR-0696"
title: "Expose profile creation, rename and open in the desktop"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0695"]
acceptance: [{"criterion":"Durable profile management and independent desktop lifetime","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Host 0.36s and Qt 0.62s contracts pass; actual management course 1.43s includes create/rename/activate/private prefs/independent close/Quit and reviewer-found last-management close regression. Main executable shutdown13.44s passes. Agent inspected rendered management PNG with visible unclipped controls. Changed TU tidy/format and bypass0.718s pass; isolated tests-disabled Release130.716s and documentation gate pass. Same-profile windows, process arbitration, native/screen-reader attendance remain later scope."}}]
---

# Expose profile creation, rename and open in the desktop

## Scope

Add an explicit desktop composition owner for different-profile application runtimes,
with one in-process runtime/startup per profile ID. A profile menu opens the
management surface, creates/renames through the durable registry host, opens
another profile without replacing existing windows, and raises an already-open
profile. New profiles begin empty with standard defaults. Accepted labels update
chrome and Settings; rejected edits retain their text. No Qt-owned persistence.

Desktop owns runtimes and startup handles independently of its initial window;
closing the initial window must not strand another profile or shut its services
down. Per-profile close drains that profile; desktop Quit drains all. Public
production and fixture desktop paths share the same management composition.
Same-profile New Window, cross-process owner arbitration and restoration are
later slices and are not advertised as completed here.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, shared contracts, bookkeeping and acceptance.
Workers receive exclusive source/test ownership and preserve others.

## Acceptance

Actual profile-menu/management course creates Work from tuned Default, opens
both concurrently, renames Work, reopens by ID without duplicate runtime, and
closes Default while Work remains usable. Unknown/failed startup preserves other
windows; rename/create refusal preserves user text. Focused Qt stable identity,
keyboard and accessibility checks; one desktop public-path fixture and affected
main startup/production graph check. No live credentials or provider mutations.

## Evidence

Implemented a decision-lane management host backed by the durable registry,
a native Qt profile menu/modeless editor, and an independent desktop owner used
by both main and fixtures. It reserves pending/live/closing runtime identities,
keeps the registry and scratch lease after the initial window closes, and drains
all runtimes on Quit. New profiles retain process/build facts but clear borrowed
profile settings and authority. Same-profile windows and process arbitration
remain outside this slice.

Focused host and Qt contracts pass (0.36 s / 0.62 s), covering durable-only edits,
refused candidate retention, stable row/action identity, keyboard/accessibility
names, exact callbacks, failure isolation and owner reservation. The actual
management course passes (1.43 s): tuned Default, standard Work, concurrent open,
rename, activation without duplication, independent close, create after initial
close, private preference persistence/reopen and Quit. An independent medium
review found an invisible-desktop lifecycle bug after all profiles and Management
closed. The fix and regression pass in that same course; the actual executable
shutdown course also passes (13.44 s).

Agent inspected `build/dev/profile-review/profile-management.png`: labels and
controls are visible without clipping. This is a rendered fixture observation,
not native platform or screen-reader attendance. Changed application/Qt/host
TUs and the public course pass scoped tidy/format; source inventory and direct
mutation-bypass rejection contract pass (0.718 s). No live credentials or remote
provider mutations were used. The network-isolated tests-disabled Release graph/build/executable contract
also passes (130.716 s); fixture-only construction remains absent. Documentation
gate passes. Task-created contained production root was automatically removed;
the management raster is retained for later attended preparation.
