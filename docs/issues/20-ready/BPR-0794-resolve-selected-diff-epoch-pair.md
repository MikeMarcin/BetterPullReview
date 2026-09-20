---
schema: "implement-rfc.card.v1"
id: "BPR-0794"
title: "Resolve the exact epoch pair for a large pull request selected diff"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"94f6f8859993be4fea63c52855bab4f0144b185e","evidence":{"kind":"artifact","detail":"Retained metadata-only diagnosis-d12.json attributes protocol_acquire/transport/unavailable before revision preparation; the card retains its external path and digest. The prepared candidate's composed and epoch checks passed in build/r2-logs/dsp3-bundle-focused-ctest.log. This is retained frozen-candidate evidence, not a new private-target observation."}},{"criterion":"AC2","outcome":"pass","revision":"94f6f8859993be4fea63c52855bab4f0144b185e","evidence":{"kind":"artifact","detail":"Retained exact sparse/current/historical authority and stale-selection evidence is described on this card and BPR-0795. Inspected source-bound focused log: 13/13 passed, including epoch core/host, CLI transport, Git cache/sparse cache, SQLite contracts and Qt composition. No claim is transferred to later source revisions."}},{"criterion":"AC3","outcome":"pass","revision":"94f6f8859993be4fea63c52855bab4f0144b185e","evidence":{"kind":"artifact","detail":"Prepared-candidate focused log passes 13/13 and portable two-catalogue public course passes 1/1 in 219.66 s; both logs explicitly bind source 94f6f8859993be4fea63c52855bab4f0144b185e. Existing documentation/format evidence remains recorded on the cards. Resume verification reconfirmed executable digest, signature, tests-disabled Release cache and offline-page digest without launching the app."}},{"criterion":"AC4","outcome":"pending","revision":"94f6f8859993be4fea63c52855bab4f0144b185e","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c remains sequence 389 with BPR-0794-cli-selected-diff ready, protected and decision=null. The exact target's readable diff and source/anchor identity still require the recorded owner observation. No new target read occurred."}},{"criterion":"AC5","outcome":"pass","revision":"95a56ef965a0067cb3437d53ecf2d58a84162f32","evidence":{"kind":"observation","detail":"This resume performed only local repository/session/bundle checks. No app or gh was launched, no live PR write or account/device-session change occurred, and no credential or private content was read into the checkout. No BetterPullReview, gh, CMake, Ninja, CTest or attended-review host/watch process was present at the resume process check."}}]
---

# Resolve the exact epoch pair for a large pull request selected diff

The read-only tests-disabled GitHub CLI profile reached **Pull request ready**
for the owner-retained `EpicGames/UnrealEngine#1773` and presented 1,523 files
and 19 review threads. Two file selections each refused `selected_file_stale`;
the epoch control had no exact pair and no selected diff was readable. BPR-0643,
BPR-0792 and BPR-0793 are Done for their bounded outcomes and remain terminal.
This is the separate selected-diff follow-up authorized by mandate r14.
Mandate r15 chooses bounded on-demand selected diffs: acquire only exact current
selected-file objects needed for an immediate comparison and defer older-epoch
comparison objects until that epoch is selected. Lazy historical loading alone
cannot repair the current missing exact pair. The small-versus-large loading
policy from mandate r16 is tracked separately in BPR-0795; it does not replace
this card's readable selected-diff criterion.

## Acceptance

- Attribute the missing exact epoch pair and selected-file refusal through the
  public composition, retaining only fixed aggregate and failure metadata for
  the private target outside the checkout. Do not retain private response,
  path, object, thread, cursor, or diff content in repository artifacts or logs.
- Correct the responsible transition or host mapping without fabricating an
  epoch, relaxing exact authority/object checks, adding unbounded history, or
  silently treating incomplete returned commits as complete history. Focused
  positive and negative checks must cover the failure and stale selection.
- Focused core, host or Qt checks selected by the repair, a composed public-path
  check, documentation gate and one integration-focused check pass.
- A tests-disabled Mac application read through the already opted-in CLI
  profile shows the correct file list **and a readable selected diff** for the
  exact target, with source/anchor identity checked against the accepted epoch
  pair. A remaining explicit refusal is recorded honestly and leaves this
  acceptance open.
- No live pull request write, global `gh` account change, credential extraction
  or transfer, device-session change, or private content in the checkout occurs.

## Evidence

The previous fixed metadata-only product observation is recorded on BPR-0793
and BPR-0617. The append-only product diagnostic log attributes the absent
pair to `protocol_acquire/transport/unavailable` before epoch revision
preparation. A sandboxed, bounded `gh` Git probe reproduced nonzero full-clone
exits near the 32-MiB file ceiling. A blobless depth-one exact-base clone
fits the existing bounds and contains the accepted commit and root tree, but
not selected-file blobs. This is a possible first step, not a readable diff.
The fixed aggregate-only record is outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0794-evidence-2026-09-13/diagnosis-d12.json`
(SHA-256 `a68be3bd2c2b65ab3f749703107b4fbd04d53287d5e607467c64d62dca317311`).
No child stderr, response body, private object or credential was retained.
An adapter-only sparse clone cannot satisfy the current strict connected-pack
Git cache contract. A synthetic complete-pack/missing-blob/missing-parent
fixture confirmed that the incomplete cases refuse. The d13 candidate now
adds a separate durable sparse commit/tree residency mode, exact SHA-1
commit/root-tree checks and shallow lineage marked unknown when needed. The
strict complete-pack path remains unchanged. A closed `gh api` exact-blob GET
checks the Git object ID, size, base64 payload, viewer/configuration and
cancellation before handing bytes to the host. The host requests a missing
blob only for an accepted large review and an exact authorized current pair.
Synthetic tests prove a sparse current pair can be accepted, the small path
remains complete, and unavailable/stale selected-object reads refuse. The
tests-disabled Mac bundle builds. Four focused cross-layer tests, the cache
and storage contracts, documentation gate, and format check pass. Independent
critical review found no remaining current-pair authority or recovery blocker;
its older-epoch selection finding remains assigned to BPR-0795. A task-owned
app instance reached signed-out Settings, but the macOS computer-use control
could not activate its GitHub CLI host selector, so no new target read or
source/anchor observation is claimed. The readable selected-diff acceptance
remains open.

The renewed unlocked-Mac d16 attempt launched the signed tests-disabled d15
candidate after quitting a stale task-owned d13 instance that held the Default
profile lease. The d15 window and Accounts & hosts page rendered, with the
selector still showing GitHub.com Native HTTPS. Computer-use accessibility
clicks did not open or change the selector; pointer actions returned
`AXError.notImplemented`, and keyboard traversal skipped the selector.
The d15 app was quit and its process confirmed absent. No CLI profile was
selected, no target request was made, and source, anchors and diff remain
unobserved. This is an automation-path limitation, not evidence that the
candidate either passes or fails the exact selected-diff behavior.

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

The consumer session remains at sequence 389: the selected-diff question is
ready and protected with no decision or newer event. A read-only recheck
matched the executable and offline-page digests and passed deep strict
signature verification; the build cache remains tests-disabled Release.
The 13-test focused and one-test portable logs explicitly bind `94f6f885`.
Later source changes do not turn those results into evidence for current main.
The card is parked for the existing attended criterion. No application or
`gh` launch, target request, account/device-session change, or private-content
capture occurred. The separate protected BPR-0644 relaunch candidate remains
untouched. Resume from the same packet and record only the owner's exact
candidate verdict before considering this card or its milestone complete.
