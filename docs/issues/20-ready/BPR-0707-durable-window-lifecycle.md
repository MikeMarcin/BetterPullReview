---
schema: "implement-rfc.card.v1"
id: "BPR-0707"
title: "Commit window lifecycle before runtime effects"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0706"]
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

BPR-0706 provides passing durable metadata/CAS/migration contracts. The current
management and desktop paths still perform lifecycle effects before persistence;
no BPR-0707 implementation or restart acceptance has run.
