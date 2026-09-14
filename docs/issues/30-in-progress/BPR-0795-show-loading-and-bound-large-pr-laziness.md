---
schema: "implement-rfc.card.v1"
id: "BPR-0795"
title: "Show PR loading promptly and reserve lazy objects for large reviews"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Show PR loading promptly and reserve lazy objects for large reviews

Mandate r16 adds the owner's experience requirement to the current selected-diff
milestone: opening any pull request should transition promptly to a visible
loading state. Small pull requests should continue loading their complete Git
comparison asynchronously in the background. Only pull requests classified as
large from bounded accepted snapshot facts may defer selected-file or older-epoch
objects until the user requests them. This companion work preserves BPR-0794's
exact-target selected-diff acceptance.

## Acceptance

- A public composition test opens a pull request with the provider effect held,
  observes the loading state after a paint turn, and proves no provider or Git
  effect blocks that first visible transition. Diagnose and correct any measured
  decision-turn obstruction rather than masking it with a spinner alone.
- A deterministic policy uses validated bounded snapshot shape and documented
  limits to classify small versus large. Missing, inconsistent, stale or
  superseded shape never silently grants lazy object loading.
- Small reviews keep complete Git acquisition on an effect worker and reach exact
  diff readiness without a selected-file-triggered object read. Large reviews
  defer older epoch objects until that epoch is selected and acquire only the
  exact current selected file objects needed for immediate comparison. Both
  routes preserve authority, cancellation, budgets, and explicit unavailable
  states.
- Focused positive, negative and stale-switch checks, a composed loading and
  small/large branch check, documentation gate, and an integration-focused check
  pass. No live pull request write, account change, token extraction or private
  content retention occurs.

## Evidence

The composed window/action/read-host test held the provider effect and found
that the new-target projection switched away from the Home loading label before
Qt could paint it. Keeping that surface visible makes the loading state paint
before the held provider completes. Accepted file and commit counts now travel
through the validated handoff. The existing 100-record REST page bound classifies
over-100-file or over-100-reported-commit reviews as large; absent or conflicting
facts use complete loading. This threshold bounds scheduling policy, not a
measured latency guarantee. Host tests cover the boundary and stale identity.

The current candidate routes large CLI reviews through a bounded sparse
commit/tree import and exact selected-blob read, while a 100-file review still
uses complete background acquisition. A synthetic host test establishes the
current sparse pair and checks that small reviews do not request a lazy blob.
The production Mac target and a tests-disabled Release bundle build. Four
focused cross-layer tests, cache and storage contracts, documentation gate,
and format check pass. Independent critical review confirms that the current
selected-file path is guarded by exact authority and durable sparse residency.

The d15 candidate activates an unimported force-push occurrence by its typed
occurrence ID and accepted catalog revision. Its large-review effect acquires
only that exact older head and tree, records the derived comparison base without
claiming an original historical base, and links the occurrence only after the
durable import. The selected historical file now reads its exact missing blob
under the selected-side sparse marker, retained ref and commit checks; a
complete opposite endpoint does not veto that read. Host regressions cover
stale catalog gestures, switch-before-effect, a second selection after an
older append commits, a repeated same-occurrence gesture, and cancellation
after the append's point of no return. The last cases reconcile the core and
published catalog before resuming the latest selection. Core and Qt tests
cover typed intent and compact pending state; the state bound is 2,112 bytes.

The synthetic selected historical diff, small complete background route,
current large selected-file route, and visible loading composition pass focused
checks. The first full portable course exposed a small-review error: Qt offered
lazy import for an unimported occurrence whose complete background path had
reported a rewritten base unavailable. The host now projects permission for
that action only on the large sparse route and refuses a typed import on the
small route. Qt checks show disabled rows preserve status and handles, while
large rows emit the exact typed gesture. The final five-test focused set and
the isolated portable two-catalogue public course pass, including rewritten
base reuse, retained events and the unfetchable-first gap. Documentation,
documentation self-tests, format, and the tests-disabled Release Mac build
pass. The exact-target product read remains open: the final uniquely identified
app bundle was prepared and verified, but computer-use reported the Mac locked
and could not unlock it. No new live selected-source, anchor or prompt-latency
observation is claimed. BPR-0795 remains In Progress until that native exit
and the other milestone obligations are met.
