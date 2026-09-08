---
schema: "implement-rfc.card.v1"
id: "BPR-0708"
title: "Restore independent window drawer layouts"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0707", "BPR-0700"]
acceptance: [{"criterion":"Independent durable drawer layouts and exact lifecycle cleanup","outcome":"pass","revision":"0be5b756 plus explicit BPR-0708 finish paths; app SHA256 b4ef75e0ebb04cc3abaf88ecbed01d1b2a9f7e5e16a57eeee0168805a337a4a2","evidence":{"kind":"observation","detail":"Actual four-process layout course18.96s preserves independent splitter widths/drawer/source states, latest edit immediately before Quit, fresh profile defaults, explicit Close and orphan cleanup; future optional schema reports visible error without reset. Inspected2rasters retained. Workbench host0.41s and storage0.51s cover bounded latest/drain/failure/cancel/rollback; changed TU tidy and independentmedium review pass. Existing live70.36s/restart7.03s/management1.93s pass. Strictsource/bypass0.795s, tests-disabledRelease135.177s/docs/diff pass. PrivatePR/comparison/semanticlocation/nativegeometry remain later."}}]
---

# Restore independent window drawer layouts

## Scope

Bind each actual desktop window to the existing schema5 workbench window-layout
row under its stable ID and profile-private root. Restore drawer states/widths
and line-number visibility independently; shared behaviour and explicit profile
New Window Defaults keep their existing authority. New windows seed from defaults
without copying a sibling's layout. Keep the standalone compatibility fixtures
explicit and preserve their existing defaults behaviour.

All creation/load/save/cleanup filesystem and SQLite work stays on the effect
lane. Exact window lifetime guards reject late layout projection. Orderly Quit
must retain the latest accepted layout, including a change immediately followed
by Quit; explicit Close discards its row after pending writers drain. Reconcile
orphan rows against the authoritative retained/live ID set at profile startup so
an interrupted Close cannot exhaust the bounded32-row store. Reconciliation
never changes global allocator metadata or profile defaults and never removes a
live sibling's row. Optional layout failure keeps windows usable with an explicit
existing settings error rather than claiming restoration success.

This slice covers workbench drawer layout, not native window position/size,
private PR/comparison destinations or semantic reading location. Browser tabs
remain unscheduled. Closeout [[BPR-0595]], immutable RFC_BASE_SHA
`e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

Primary owns architecture, public restart course, CMake, board and commits.
Workers receive exclusive source ownership after the port/lifecycle seam is fixed.

## Acceptance

Actual desktop restart preserves different layouts for two stable window IDs;
a fresh window starts with profile defaults, and layout changes in one window do
not alter its sibling. Immediate Quit after a drawer change persists the final
accepted layout. Explicit Close removes only its layout row; restart reconciliation
removes an orphan without touching retained rows or profile defaults. At capacity,
closed/orphan rows cannot permanently block new windows. Busy/corrupt/future
optional layout storage yields visible failure without allocator reset or startup
failure. Relevant host/storage/direct public checks, changed-TU tidy, documentation
and production composition guards pass. Required native/full RFC acceptance stays
open.

## Evidence

BPR-0700 already implements bounded schema5 keyed layout storage and separates
shared preferences; BPR-0707 supplies durable lifecycle identities. Current actual
composition still uses defaults for its first window and session memory for later
windows. This card integrates the existing persistence through actual lifecycle.


Implementation is present: desktop layouts use effect-local keyed stores, startup
reconciliation captures the complete accepted live set, and Close erases only
after pending writers drain. Desktop shared defaults are also opened on effects;
standalone compatibility remains explicit. Orderly workbench shutdown closes
input and drains one active plus the latest layout write; forced shutdown still
cancels. Direct storage tests pass0.51s including transaction rollback, and
workbench host tests pass0.41s including immediate Quit/coalescing/request cap/
failure/forced escalation. Both workers' scoped tidy passes. Independent medium
review found no material counterexample in these ownership and lifetime paths.

Initial actual layout course passed seed/restored drawer widths and states,
immediate-Quit final line-number state, orphan reconciliation and explicit-Close
row cleanup without changing defaults. Its verify phase found a fresh window
not showing Home after a restored sibling opened a review; diagnosis remains
in progress. No BPR-0708 acceptance is claimed until that public path passes.


The fresh-window failure was a landing initialization bug: layout commands can
be enabled without a PR, so every admitted desktop window now explicitly chooses
Home. Only a review gesture/accepted PR clears that choice. The complete four-
process public layout course now passes18.96s, including restored widths and
states, immediate-Quit line-number state, orphan/Close cleanup, fresh defaults,
and a future optional schema that reports visible failure while preserving the
global allocator and unsupported database. Both restored-window PNGs are inspected
and retained under `build/dev/profile-window-layout-review/`. Production source
inventory/direct-bypass rejection passes0.795s. Existing lifecycle and tests-off
Release integration checks are running before acceptance.


Final app SHA256: `b4ef75e0ebb04cc3abaf88ecbed01d1b2a9f7e5e16a57eeee0168805a337a4a2`.
The tests-disabled Release graph/build/executable contract passes135.177s. Final
changed-TU tidy and documentation/diff checks pass. The course owns and removes
its exact private temporary root; passing rendered layouts remain separate.

Final existing lifecycle checks pass: live multi-window70.36s (pending sibling
read across Close), durable identity restart7.03s and management1.93s.
