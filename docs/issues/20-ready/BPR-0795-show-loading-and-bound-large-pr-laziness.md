---
schema: "implement-rfc.card.v1"
id: "BPR-0795"
title: "Show PR loading promptly and reserve lazy objects for large reviews"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"artifact","detail":"Dedicated qt_composed_loading_scale_100 passes held-provider paint, cancellation and Home-refusal assertions. Actual tests-disabled synthetic UI visibly shows PR loading and selected comparison loading before Diff ready."}},{"criterion":"AC2","outcome":"pass","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"artifact","detail":"Current focused core_epoch_policy and application_epoch_host pass bounded small/large shape classification, missing/conflicting facts, exact authority and stale selection checks."}},{"criterion":"AC3","outcome":"pass","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"artifact","detail":"Current focused epoch/transport/cache/storage checks and portable two-catalogue course pass exact current and historical acquisition and small-complete/large-lazy boundaries; integration took 228.69 s."}},{"criterion":"AC4","outcome":"pass","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"artifact","detail":"15 focused passes plus dedicated Qt loading pass, portable integration, documentation gate, 64 documentation tests and formatting pass. Wider Qt layout and Release stage-0 self-test failures remain explicit in v002 session-identity.json. No live write, explicit account mutation, token extraction or private-content capture. Ambient inspector auto-relaunch caveat is recorded."}},{"criterion":"milestone-exit-5-attended-loading","outcome":"pending","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"artifact","detail":"New r17 loading candidate is ready/protected with no decision at session sequence 404. The actual synthetic Release course is not the owner's prompt-loading observation for the retained large target. No new human criterion is added to the four automated card bullets."}}]
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

On the unlocked Mac, the signed d15 tests-disabled app opened and rendered
Accounts & hosts after the stale task-owned d13 profile holder was quit.
Computer-use could not select GitHub.com GitHub CLI from the native host combo:
accessibility clicks left Native HTTPS selected, pointer actions returned
`AXError.notImplemented`, and keyboard traversal skipped it. The d15 process
was quit and confirmed absent. No target Open or new loading/diff observation
occurred, so the native product exit and both cards remain open; the prior
focused and integration checks still apply to unchanged code.

## Attended preparation at main `94f6f885` (dsp-003)

A tests-disabled bundle was built from `94f6f885`, signed with the stable
Developer ID identity, and passed `codesign --verify --deep --strict`
(`BUILD_TESTING=OFF`). It is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app`,
with executable SHA-256
`96fa16fea8983347303c6ce18095f147871b7d45ef51537d48097ac6004e1bfa`. A
`--self-test` run printed `AUTOMATED_PROFILE_PRIVATE_OK` and did not touch
the Default profile.

The BPR-0794/0795 focused set was rerun at that source and passed 13 of 13
(`build/r2-logs/dsp3-bundle-focused-ctest.log`).
`application_epoch_portable_occurrence_two_catalogues` also passed
(`dsp3-bundle-portable-ctest.log`).

[the rfc-bpr-0013-attended-v001 packet](../../verification/reviews/rfc-bpr-0013-attended-v001/operator-packet.md) holds three ready, protected questions in the checkout's attended-review
session: `BPR-0794-cli-selected-diff`, `BPR-0795-loading-visible` and
`BPR-0644-device-relaunch-8h`. The human part is open. The owner must quit
their own running BetterPullReview before launching the bundle.

## Resume checkpoint — 2026-09-19

The consumer session remains at sequence 389: the loading question is ready
and protected with no decision or newer event. The frozen executable,
signature, tests-disabled Release cache and offline-page digest were rechecked
without launching the app. The source-bound 13-test focused and portable
two-catalogue logs still pass at `94f6f885`; they are retained evidence for
that build, not a fresh verification of later main changes. The card is
parked for the existing person-observable part of milestone exit 5; its four
automated bullets do not acquire an additional human criterion. BPR-0794's
exact-target selected diff remains independently required. No application,
`gh`, target request or account/device-session change was started, and the
separate protected BPR-0644 relaunch candidate remains untouched.

## Owner rejection and current-source correction — mandate r17

The owner could not reach a readable review through dsp3; that failure also
prevents judging this loading question. Attended observations 390–392 and
releases 393–395 supersede the sequence-389 preparation above. The stale bundle
omitted the later connected-CLI routing and shared sign-in changes described on
BPR-0794. Both cards are resumed for a coherent current-source candidate, with
fresh composed GitHub.com CLI/native refusal-retention coverage. Fixture success
does not answer the owner's large-target loading or readable-diff criteria.
The replacement presentation must be the actual offline attended HTML, not only
the earlier Markdown operator packet.

## Prepared current candidate — `7b591faa`

The [current offline review page](../../verification/reviews/rfc-bpr-0013-attended-v002/offline-package/index.html)
binds revision `rfc0013-r17-7b591faa-ad50fe5d` at sequence 404. The same new
tests-disabled bundle as BPR-0794 visibly showed PR loading, selected-file loading
and Diff ready in an actual synthetic CLI product run. The owner has not judged
the large private target's loading; this item stays independent of readable-diff
acceptance and the separate eight-hour device-session follow-up.

Current focused core/host, exact/sparse cache, transport, storage, setup and epoch
Qt checks pass. The dedicated `qt_composed_loading_scale_100` passes the held-read
paint, cancellation and Home-refusal assertions. The portable two-catalogue course
passes in 228.69 seconds. The older broad Qt suite still has an unrelated wrapped
conversation-row layout failure, and the Release self-test retains its stage-0
CLI inventory failure; neither is represented as passing. See
[the evidence identity](../../verification/reviews/rfc-bpr-0013-attended-v002/session-identity.json)
for the complete bounded results, verified executable digest, launch caveat and
cleanup. No fresh private-target latency or historical-content observation is
inferred from the synthetic course.
