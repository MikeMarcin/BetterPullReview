---
schema: "implement-rfc.card.v1"
id: "BPR-0713"
title: "Share accepted private-ledger projections between review windows"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0712"]
metadata: {"dispatch":"UX-2026-09-08-D5","mandate_revision":1}
acceptance: [{"criterion":"Accepted-only observer and nonmutating selected receipt refresh","outcome":"pass","revision":"78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4","evidence":{"kind":"observation","detail":"Focused host tests pass: successful mutations notify; failed writes and reads do not; CAS and observer exception semantics preserved; selected material carry is read-only; rejected note candidates unchanged."}},{"criterion":"Bounded receipt and note/overview sibling fanout with independent views","outcome":"pass","revision":"78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4","evidence":{"kind":"observation","detail":"Final real two-file public multi-window course passes83.73s: receipt QAction and note overview fanout, distinct selection and scroll, different PR isolation, pending comparison refusal and unselected actionable-file restoration. Conversation/defect rendered fanout and two-account/native obligations remain open on BPR-0595."}},{"criterion":"Changed-surface integration and review corrections","outcome":"pass","revision":"78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4","evidence":{"kind":"observation","detail":"Final affected composition/inventory2/2 pass37.01s; prior architecture/host integration4/4 pass45.84s; critical stale receipt finding corrected and exercised, narrow actionability recheck no demonstrated blocker. Final format/tidy and tests-disabled Release inventory required30 forbidden7 pass; documentation and62 regression tests pass."}}]
---

# Share accepted private-ledger projections between review windows

## Scope and acceptance

Bounded AC-03/09 slice: accepted receipt and private-note changes reach sibling
windows of the exact profile/account/host/repository/PR authority. Delivery must
preserve independent comparison, file, semantic selection, layout and scroll.
No cross-profile/review delivery, stale delivery after replacement or Close,
publication on failed writes, or recursive material carry-over writes.
Keep rejected note candidates separate from accepted projection refresh.

## Inventory and owned paths

The profile runtime already bounds window observers and owns decision delivery.
Each window composes its own ledger store/host; receipt retention and conversation
caches currently have no sibling invalidation. Agent finding delivery can store
notes through the store port directly. Observe successful writes at that common
boundary and refresh affected projections through application ownership.

Own the narrow application ledger observer/refresh contracts, composition wiring,
focused host and real multi-window tests, and architecture/closeout records.
Verify exact authority, failure and lifecycle rejection, selected-material safety,
and a rendered multi-window gesture course. Preserve all native/attended and
two-account obligations; RFC-0028 Home/layout and unscheduled tabs are unchanged.

## Implementation and review invariants

The owning store decorator observes successful mutations only, including direct
agent note writes. It preserves typed CAS responses and cannot convert a
committed result into failure when an observer throws. Each store coalesces its
three domain invalidations into at most one pending decision callback. The
profile runtime supplies lifetime and account-generation admission; recipients
match complete durable review authority while retaining their independent
window session, provider snapshot and PR request generation. Notifications carry
no candidate prose or selected position. Receipt reload is read-only, including
core carry-over to a different selected material; rejected note candidates remain
separate and unchanged.

Independent critical review identified a stale-selection counterexample: starting
a new selected diff cleared Qt's receipt but left the host's old retained file.
A queued sibling update could re-enable that old receipt during the new request.
The correction clears host and observation receipt retention with selected-diff
invalidation and gates reload by live request generation, file and material.
Source recheck found no remaining demonstrated blocker. It also removed duplicate
overview publication from the callback. Existing overview reconstruction still
loads one receipt per snapshot file; incremental overview retrieval remains a
performance follow-up rather than a claim made by this slice.

## Scope limits

This slice accepts receipt and note/overview fanout. Conversation cache invalidation
has focused host evidence; independent rendered conversation and defect-card
fanout remains future AC-03 verification/work. The existing per-window
`DurableDefects` cache is not reloaded by this invalidation. Different-material
receipt carry is directly verified at the host boundary; two rendered historical
epochs and queued Close/account-replacement delivery are not claimed.
Private-note writing in the composed course uses the existing fixture host seam,
not a newly shipped note editor. No note-editing UX or full AC-04 is inferred.
Windows, package/native focus, accessibility, geometry, complete two-account
isolation and required attended verdicts remain on BPR-0595.

## Verification development notes

The earlier one-file course passed 83.61s, then 77.45s with distinct selections;
neither result is the final corrected two-file candidate. Switching to the
existing two-file restoration fixture exposed the old next-change assumption
(52.65s failure); the course now selects known source lines. Its pending-action
oracle originally observed synchronous label changes before disablement (71.30s
failure), so it now arms after the immediate cleared/disabled state and before
queued delivery. The following run reached and passed pending-action safety but
failed the return-to-first-file condition (85.89s): the old helper accepted any
resident comparison and a repeated current index could issue no selection.
The helper now emits explicit selection and requires exact `review.cpp` identity.
That exact check exposed the underlying remaining fanout gap (130.80s failure):
a previously read, unselected file stayed absent from the recipient's actionable
order after its sibling marked it unread. Refreshing only the currently retained
receipt could not detect that change. Receipt invalidations now also request the
existing selection-preserving workbench actionability observation, and the test
addresses both files by stable model identity instead of assuming row order.
These intermediate outcomes are not final acceptance evidence.

## Accepted candidate and evidence

Implementation candidate `78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4` accepts the
bounded receipt and note/overview slice, not all RFC-0024 AC-03. The final real
two-file `application_profile_multi_window_public_path` course passes 83.73s:
public receipt actions reach the sibling while distinct selections and scroll
remain stable; private notes reach both overviews; a different PR stays isolated;
pending selected material cannot revive the old receipt; and an unselected file
returns to the sibling's actionable order. The course also retains Close,
independent-read and shared-draft checks. Log: ignored
`build/dev/bpr0713-actionability-course.log`.

Focused application host tests cover accepted-only observation, typed CAS,
observer exceptions, nonmutating receipt carry, rejected candidates and cache
invalidation (core-only pass 0.38s; independent host pass 0.06s). The initial
four-check integration passes 45.84s, including architecture boundaries and host
tests. After the last actionability correction the affected ledger composition
and composed-host checks pass 2/2 in37.01s (`bpr0713-final-integration.log`).
Final seven-file format and changed-translation-unit tidy checks pass. The final
tests-disabled Release build and composed inventory pass with required=30,
forbidden=7 (`build/app-release/bpr0713-accepted-inventory.log`). Documentation
and its 62-test regression suite pass; board validation passes. Logs are ignored
build artifacts, not durable source. Critical review's counterexample is fixed
and exercised; its narrow actionability followup reports no demonstrated blocker.

The designated integrator judges these observations sufficient for this bounded
card. Named integrator and opposite-family review adapters were unavailable;
explicit native Astra-low integrator/workers and Astra-xhigh critical review were
used, with Sol-low scouting. Serving telemetry was not observed. No RFC/pack
closure, native/human waiver or external mutation is implied.
