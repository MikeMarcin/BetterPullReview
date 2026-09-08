---
schema: "implement-rfc.card.v1"
id: "BPR-0707"
title: "Commit window lifecycle before runtime effects"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0706"]
acceptance: [{"criterion":"Durable lifecycle before window effects and actual process restart","outcome":"pass","revision":"e9b1b217 plus explicit BPR-0707 finish paths","evidence":{"kind":"observation","detail":"Manual-executor lifecycle/CAS failures/reentrant/retirement/capacity tests0.46s pass. Actual four-process restart6.78s restores MRU identities/fresh generations and independent profile sets, distinguishes Close/Quit and global highwater; real SQLite locks keep both windows usable and explicit Reload never retries destruction. Live multiwindow70.43s/management1.47s pass. Focused read-host restored-ID/stale/legacy/churn tests0.31s and all changed TU tidy pass. Independentmedium findings resolved; strictsource inventory and bypass0.703s/docs/diff pass. Inspected restored-home.png retained. Private destinations/layout/crossprocess/native criteria remain outside this slice."}}]
---

# Commit window lifecycle before runtime effects

## Scope

Integrate schema2 window metadata through asynchronous profile management and
actual desktop gestures. The core catalog decides allocation, fresh generations,
MRU, Close and Quit. The host saves one bounded CAS candidate before emitting
window-start or explicit-close effects; orderly Quit saves the retained set
before shutting down runtimes. Treat accepted live windows as retained records
in the durable snapshot while preserving live state in the runtime catalog.
Registry create/rename and reload must preserve exact live identities without
duplicating retained IDs. Emit every admitted restored context in core order,
not only launch.Activated. Native activation determines live MRU; orderly Quit
persists it. No window effect may run on failed/uncertain persistence admission.

Change the desktop's close event and user Quit to wait for typed host commands.
A save failure keeps windows usable and reports an actionable error; retry/reload
must not regress allocator identity. Retire exact outstanding jobs before freeing
reservations. Stale/duplicate completions, reentrant projections and Quit during
an outstanding save cannot start a wrong window or strand a reservation.
Explicit forced lifetime cleanup remains distinct from successful orderly Quit;
it never claims that failed persistence succeeded. Initial startup failure must
still drain without hanging when no registry has been accepted.

This slice restores window identities/count/order after process restart. Private
review/comparison destinations and per-window layout restoration are the next
slice: restored windows currently enter Home with an explicit unavailable-content
notice, never implying their discarded review was restored. New windows enter
Home without that notice. Browser tabs remain unscheduled. Do not synthesize a
Default PR/Resume destination, weaken account authority, or reset corrupt metadata.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, public restart course, board and commits.
Host worker owns profile registry lifecycle save API, management host/projection
and direct tests; desktop worker owns profile_desktop.cpp/.hpp. Coordinate the
exact command/API contract before parallel edits.

## Acceptance

Manual-executor tests prove no start/close/quit effects before durable acceptance,
failed saves preserve usable exact windows, global IDs never repeat, restored
contexts receive fresh generations, all restored windows are emitted in MRU
order, explicit Close removes only its record and Quit retains all survivors.
Cover duplicate/stale completions, reentrant close/Quit, busy/cancelled/failed
saves, failed startup and shutdown quiescence. Existing profile edits preserve
window records and stale contexts remain refused.

The actual desktop public course creates two Home windows, quits and reopens
both stable IDs with newer generations, explicitly closes one, quits/reopens only
the survivor, and creates a new ID above high-water. Check independent profile
retained sets, activation without duplication, visible unavailable-content notice,
and unchanged management controls. Use a designated private temporary root with
no provider/vault mutations. Focused management, storage, current live-window
course and documentation pass. Private content restoration and native attendance
remain open on the RFC closeout.

## Evidence

Implemented core-candidate lifecycle saves before typed start/close/quit effects,
explicit reload recovery, deferred focus and exact failed-start retirement replay.
Manual-executor registry/management tests pass (0.46s), including failed or
uncertain persistence, stale/reentrant completions and physical closing capacity.
The actual four-process restart course passes (6.78s final): two retained Default
windows restore in MRU order with fresh generations; Work stays independent;
Close omits only its window; Quit retains survivors; fresh IDs exceed global
high-water. Real SQLite write locks prove failed Close and Quit preserve both
windows and explicit Reload never retries the action automatically.

The public course found descending restored IDs were rejected by the read-host
attachment high-water rule. A fixed32 startup-only restored-ID history now
admits MRU order while refusing retired IDs and changed-generation reattachment;
the first fresh attachment seals restoration. Focused host tests pass (0.31s final),
including descending32/capacity/churn; independent medium review has no remaining
material finding. Earlier close-widget and pending-start retirement findings
were fixed and narrowly rechecked.

Retained and inspected rendered artifact:
`build/dev/profile-window-restart-review/restored-home.png`. Its unavailable-
content notice is visible; private destination/layout restoration remains next.
Strict production source inventory and direct-mutation bypass rejection pass
(0.703s). Unchanged schema2 storage/migration evidence is reused from BPR-0706.

The existing live multi-window course passes (70.43s), including pending sibling
read completion and accepted-close visibility; profile management passes (1.47s).
Restored stable ID1 is also marked explicit to prevent the older standalone read
slot from reviving it after retirement. That compatibility-only guard receives
a focused host regression; the public course uses explicit window readers.

Final changed-TU tidy, documentation and diff checks pass. The four-process
runner removes its exact private temporary root, including failure cases.
Unchanged relevant BPR-0705 architecture and tests-disabled isolation evidence
is reused; this slice introduces no fixture seam in production.
