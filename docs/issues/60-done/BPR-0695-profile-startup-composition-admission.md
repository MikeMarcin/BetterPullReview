---
schema: "implement-rfc.card.v1"
id: "BPR-0695"
title: "Admit one profile before constructing its private runtime"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0694", "BPR-0693"]
acceptance: [{"criterion":"Asynchronous exact-profile startup and private runtime composition","outcome":"pass","revision":"c5441c1e2a0cdcbca63a472e1c3074ccd8597a08 plus BPR-0695 finish paths","evidence":{"kind":"observation","detail":"Host0.33s; actualpublicprofilecourse1.47s proves no unknown/cancelledwindow, unsafe-root unchanged refusal, two-profile durable appearance, rename/reopen, independent close and optionalqueue isolation. Eight migrated E2E courses pass after designated fixture-root permissions updated; app/Qt1004.12s/architecture7.95s/sourcebypass0.691s pass. Isolated tests-disabledReleasebuild+executablecontract129.513s pass. Mediumreview scratchdestruction-orderfinding repaired; nootherfindings. Scopedtidy/format/docs pass except seven documented unchangedfixturewhole-TU diagnostics; changed-linefixturetidy passes. No multiwindow/UImanagement/processownerclaim."}}]
---

# Admit one profile before constructing its private runtime

## Scope

Asynchronous startup admission resolves a requested durable BPR profile before constructing its existing single-window runtime. Keep RegistryRoot separate from ProfileDataRoot; all private stores, caches, recovery, workspaces and diagnostics receive selected root, and vault/host/binding factories receive selected ID. Unknown or corrupt registry refuses without fallback. Same-profile multi-window sharing, activation routing and cross-process owner locks are separate later slices.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns CMake, shared composition, bookkeeping and acceptance. Workers own
explicitly assigned new authority/adapter/test files and preserve others.

## Acceptance

Focused admission host cancellation/stale/failure checks and fixture public course admit Default and profile2, reopen isolated durable settings, and verify no nonDefault private service receives registry base. Architecture and composition boundary checks.

## Evidence

Implemented queued startup host and async application/fixture factories.
The registry load and root resolution run on the effect lane before private
services/window construction. Exact profile identity, root and label reach all
private composition and credential/binding factories. Existing unsafe roots
refuse unchanged; optional queue failure stays local. Scratch lifetime outlives
all dependent stores after an independent medium review found the declaration
order regression; that repair rebuilt with the migrated courses.

Focused application startup tests pass (0.33s), including phase/context checks,
cancellation, stale completions and rejected effect submission. New actual
public startup course passes (1.47s): unknown/cancelled startup creates no
window; shared-permission root refuses unchanged; concurrent Default/Work have
isolated durable appearance; Work rename/reopen retains its root; closing Work
leaves Default usable; damaged Work queue preserves appearance and reports its
own failure. All eight migrated end-to-end courses pass: chat, findings, agent
invocation, queue, liveness, device binding restart, logging and exact setup.
Queue/invocation fixture drivers precreated0755roots and now explicitly make
those designated test roots private; production refusal was retained.

App/composition builds, Qt scale100 (4.12s), architecture boundaries (7.95s),
mutation-host bypass source contract (0.691s), and the isolated tests-disabled
Release graph/build/executable contract (129.513s) pass. Canonical tidy passes
for the new host/public course, app/main/Qt and eight migrated tests. Fixture
whole-TU tidy retains seven unchanged baseline findings; changed-line fixture
tidy passes without suppressing other changed surfaces. Format/diff and
documentation gates pass. No live credentials or provider mutations used.

No same-profile multiple-window, management menu, process-owner arbitration,
or native Windows/Linux application interaction acceptance is claimed.
Normal build outputs remain in use; fixture and contained Release build roots
cleaned themselves. Consumed /tmp logs retain only fixture diagnostics until
integration cleanup.
