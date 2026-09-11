---
schema: "implement-rfc.card.v1"
id: "BPR-0663"
title: "Public preview refuses the epoch smart acquire"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"The anonymous binding admits the lease with the accepted revision's generation and still refuses an authenticated authority and a GHES profile","outcome":"pass","revision":"7506c87b","evidence":{"kind":"observation","detail":"At this head: native_git_smart_transport_contract and native_git_smart_transport_fixture pass, including the seam pin (a github.com anonymous wire built with the revision's generation is admitted; one built with the old constant is refused as invalid_request before any request) and the unchanged GHES and authenticated refusals. rfc0017_public_preview_decision_contract and the six application_anonymous_public_preview_* courses pass, as do application_composition_contract and the round-trip contract with both application digests repinned."}},{"criterion":"A public pull request opened through Public preview in the signed bundle shows its updates and an exact comparison, with smart_acquire succeeding in diagnostics.log","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0663-public-preview-epoch, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint 014078d29e8eb3a82a29948c633080cf5135ca03f46a12d925f15122fb1b75b6, decision 469c6114-cc71-4368-b4e8-d1ba01009e18, actor human, 2026-09-11T23:43:25Z: accept. The line carried the catalogue's updates on the cache-hit launch and the owner accepted without a note; the v006 rejection on this criterion was the empty line, fixed by BPR-0725. The smart_acquire log half rests on the 2026-09-07 live acquire recorded on BPR-0667 (session observation at sequence 280); the v007 launch was a cache hit by design of the packet."}}]
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
[BPR-0667](BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md),
which owns the new failure. This card's own change is not in question.

## Observation from the v003 session, 2026-09-10 (no verdict)

The owner recorded no decision on `BPR-0663-public-preview-epoch`; the
candidate stays undecided and protected. The three screenshots on the
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
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

## Attended verdict, 2026-09-11 (v006): rejected

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0663-public-preview-epoch`, decision `da24251c`, 18:32:18Z: **reject**. Verbatim:
"Shows No updates loaded".

The question the v003 observation left is now answered. The diagnostics log for the three
launches during the session (18:15:40Z, 18:24:03Z, 18:31:16Z) records only `catalog_open`,
`catalog_recovery` and `catalog_prepare`, no acquire and no request; the review was restored
from the cache. That is not a failure of the acquire this card fixed (last seen succeeding
live at 2026-09-07T20:56:43Z). It is that a cache hit never publishes the catalogue's updates
to the line: the line gains rows only from an accepted append or a requested history page,
and a revision prepared with no mutation goes straight to the comparison
(`src/core/src/epoch_transition.cpp:1302-1309`). A fresh open of a cached pull request
through Home would show the same empty line. Carded as
[BPR-0725](../40-parked/BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md).
This card's own change is not in question; it stays parked on human-review for the line, asked
again on a bundle carrying BPR-0725. The comparison half of the same run was accepted on
[BPR-0667](BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md).

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0663-public-preview-epoch` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: the epoch line shows the updates on a bundle carrying BPR-0725; the acquire only runs again when GitHub holds a newer head than the local catalogue.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0663-public-preview-epoch`, decision `469c6114`, 23:43:25Z: **accept**.
The public-preview review history is accepted on the bundle carrying [BPR-0725](../40-parked/BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md). Done.
