---
schema: "implement-rfc.card.v1"
id: "BPR-0663"
title: "Public preview refuses the epoch smart acquire"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
parked_on: "human-review"
acceptance: [{"criterion":"The anonymous binding admits the lease with the accepted revision's generation and still refuses an authenticated authority and a GHES profile","outcome":"pass","revision":"7506c87b","evidence":{"kind":"observation","detail":"At this head: native_git_smart_transport_contract and native_git_smart_transport_fixture pass, including the seam pin (a github.com anonymous wire built with the revision's generation is admitted; one built with the old constant is refused as invalid_request before any request) and the unchanged GHES and authenticated refusals. rfc0017_public_preview_decision_contract and the six application_anonymous_public_preview_* courses pass, as do application_composition_contract and the round-trip contract with both application digests repinned."}},{"criterion":"A public pull request opened through Public preview in the signed bundle shows its updates and an exact comparison, with smart_acquire succeeding in diagnostics.log","outcome":"pending","revision":"7506c87b","evidence":{"kind":"freeform","detail":"Owed, and not reachable from this session. The live-candidate course that drives a real public pull request is compiled only when BUILD_TESTING is on, so the signed bundle (tests disabled) has no automated path to it; the observation needs a URL typed into the locator of the running bundle, and this session must not inject input. The bundle is rebuilt at this head (executable sha256 dba2d952133ee18b28e458fb282fe8d9ad3930c68a76d1fd5c3f601d86fc88cf). Queued for the attended session with BPR-0646 and BPR-0662; microsoft/terminal#20607 is the pull request the defect was found on."}}]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Public preview refuses the epoch smart acquire

Opening `microsoft/terminal#20607` through Public preview in the signed
`app-release` bundle at the pack head (bc551240, 2026-09-07) loaded the pull
request (title, author, branches, 7 review comments in the conversations
drawer) and then failed the review history: `logs/diagnostics.log` records
`op=epoch outcome=failed epoch_stage=smart_acquire failure_domain=transport
failure=invalid_request` followed by `epoch_stage=revision_prepare
failure=unavailable`, the epoch line reads "No updates loaded." and the
status line says the workbench, the review history and the exact comparison
could not be updated. The only smart acquires that ever succeeded on this
Mac were signed in (2026-09-05).

Probable cause. `anonymous_git_smart_transport_factory::acquire` in
`src/app/src/github_application.cpp` builds the anonymous wire with a
hard-coded `github_session_request_context` (request 917, identity
generation 918). `native_https_transport`'s anonymous smart binding admits a
lease only when `aContext.Revision.Source.IdentityGeneration` equals that
context's identity generation (`credential_matches` for
`anonymous_smart_request`), and the accepted revision carries the host's
real generation, so `make_native_git_smart_transport` refuses with
`invalid_request` before any request is made. The composed public-path
courses run over fixture transports and did not catch it.

## Scope

- Make the anonymous factory's context carry the generation the lease will
  be judged by (or make the binding's admission independent of a generation
  the anonymous surface never established), whichever the RFC-BPR-0017
  contract intends; keep the refusal of an authenticated authority.
- A composed check that a public-preview pull request reaches
  `revision_prepare` over the production anonymous factory.
- Owned paths: `src/app/src/github_application.cpp` (the anonymous factory),
  `src/adapters/github/src/native_https_transport.cpp` if the admission is
  the defect, their tests and the application digests.

## Acceptance

- A public pull request opened through Public preview in the signed bundle
  shows its updates on the epoch line and an exact comparison, and
  `diagnostics.log` records `smart_acquire` succeeding.
- The anonymous binding still refuses an authenticated authority and a
  GHES profile.

## Notes

- Fix, 2026-09-07. `anonymous_git_smart_transport_factory::acquire` in
  `github_application.cpp` builds the anonymous wire with the accepted
  revision's identity generation (`aContext.Revision.Source.IdentityGeneration`)
  instead of the constant 918; the binding's admission compares exactly that
  generation, so the lease it refused for every public-preview pull request
  is admitted. The request id stays the factory's own correlation, which
  nothing judges. `native_git_smart_transport_contract` gained the seam
  pin: a github.com anonymous loopback wire built with the revision's
  generation admits `make_native_git_smart_transport`, and a wire built with
  a drifted generation (the old constant) is refused as `invalid_request`
  before any request; the GHES and authenticated refusals are unchanged.
- The fixture courses (`application_anonymous_public_preview_*`) run over
  fixture and loopback read transports and stop before the epoch's smart
  acquire, so they neither caught the defect nor prove the fix; the proof
  in the signed bundle (updates on the epoch line, a comparison,
  `smart_acquire` succeeding in `diagnostics.log`) is owed and waits for
  the Mac's screen, locked since the smoke that found the defect. Both
  application digests were repinned; `application_composition_contract`,
  the round-trip contract, the six public-preview courses,
  `check_boundaries`, strict tidy, `clang-format --Werror` and
  `verify_documentation.py` pass.

## Attended verdict, 2026-09-07: the fix is confirmed, the criterion is not met

The owner's run at 17:47 reached the wire, and the diagnostics log records
`epoch_stage=smart_acquire outcome=succeeded` with no `invalid_request`: the
generation this card corrected is right, and the refusal it was written for is
gone. The next stage then failed,
`epoch_stage=protocol_acquire failure_domain=protocol failure=oid_mismatch`,
so the epoch line still read "No updates loaded." and no comparison resolved.

The card's acceptance names all three, so it stays open behind
[BPR-0667](../60-done/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md),
which owns the new failure. This card's own change is not in question.

## Observation from the v003 session, 2026-09-10 (no verdict)

The owner recorded no decision on `BPR-0663-public-preview-epoch`; the
candidate stays undecided and protected. The three screenshots on the
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
rejection (decision `65f3f881`, bundle `9334b13a…` at da92d5a9) show the
Updates row reading "No updates loaded." beside "pair ready" and the status
line "Review history is ready" with microsoft/terminal#20607 open. That
review was restored at launch, not opened through Home as the packet asked,
and the diagnostics log for the launch (05:19:48Z to 05:34:45Z) records only
`catalog_open`, `catalog_recovery` and `catalog_prepare` succeeding, no
`smart_acquire` or `protocol_acquire` stage, no request and no failure;
every comparison was answered from the cache. So this run never exercised
the acquire this card fixed, and neither confirms nor refutes its pending
criterion. Whoever next drives this candidate should decide whether a
restored review is meant to re-acquire its updates (in which case the empty
line over a "ready" history is a defect of its own) or only a fresh open
does, and ask the owner to open the pull request through Home.
