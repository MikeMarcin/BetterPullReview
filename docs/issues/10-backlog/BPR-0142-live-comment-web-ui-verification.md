---
id: BPR-0142
title: Verify a published comment in the GitHub web UI
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0139, BPR-0221, BPR-0229, BPR-0235, BPR-0256, BPR-0318]
---

# Verify a published comment in the GitHub web UI

## Final-state safety correction

BPR-0221 repaired the false-green exit handling and added a useful structural
inventory, but it does not deny generic network calls, scripts, helper
subprocesses, or production-application launches. BPR-0235 must establish the
default-deny process boundary before this card may construct or use a live
provider. The designated repository decision below remains valid.

RFC-BPR-0008 requires that a BetterPullReview comment is **readable and
replyable in the GitHub web UI with no tool-specific marker**. A fixture cannot
establish this. The fixture proves what BetterPullReview sent; only GitHub can
show what GitHub made of it, and only a person looking at the page can confirm
it carries no marker and behaves like an ordinary comment.

## Exactly what is missing

1. **Attended execution of the prepared one-shot shipped-app lane.** BPR-0256
   prepares the digest-bound signed-capsule supervisor, private in-application
   bootstrap, and exact tests-disabled application registration for designated
   `MikeMarcin/BPR-Test#1`. It does not execute the lane or provide
   Linux/Windows evidence.
2. **The required device-login path and distributable identity.** BPR-0318
   transitively requires RFC-BPR-0016's capture, implementation, production
   composition, and public-behavior evidence. The owner rejected manual token
   minting and pasting during BPR-0256 v006.
3. **A human at the keyboard** to open the pull request, read the comment,
   reply to it, and confirm it is indistinguishable from one written in the web
   UI.

## What must not be done instead

Do not point this at an arbitrary repository. Do not create a repository and
assume it is designated. Do not weaken the coordinator's safe-target check to
let a test through — that check is the reason an accident is impossible, and a
test that disables it has removed the property it was meant to exercise.

## A guard that must be rebuilt when this card lands

Today "no automated test can mutate a non-designated live repository" holds for
a reason stronger than care: `review_mutation_port` has exactly one
implementation in the tree, the fake in the test, and no production code
constructs a coordinator. No code path can reach GitHub with a mutation.

This card introduces the first live implementation and **destroys that
argument**. A scan that keeps passing afterwards would be passing for a reason
that no longer exists, which is worse than no scan at all.

Before the live provider is used, replace the property with one that survives
it: the live implementation must be unreachable from any test target, and that
must be asserted rather than arranged.

## Required direct evidence

- The comment is read in the GitHub web UI and reproduced here in full,
  including that it carries no BetterPullReview marker, badge, or footer.
- A reply posted in the web UI is shown returning to the workbench.
- The account shown as author is the authenticated profile.
- The designated repository is named in documentation before the run, not after.

## Owner decision, 2026-08-08: the designated repository

`https://github.com/MikeMarcin/BPR-Test`. It is named in
`docs/architecture/review-round-trip.md` under "The live provider contract",
with GitHub database ID `1328277631` and node ID `R_kgDOTyvofw`. This discharges
repository designation and this card's last completion criterion: the
repository is documented before the run, not after.

On 2026-08-10 the product owner then authorized creation of the disposable
pull request and designated the result before any BetterPullReview live child
or review mutation ran. The only designated PR is `MikeMarcin/BPR-Test#1`,
database ID `4246446933`, node ID `PR_kwDOTyvof879G6NV`, head OID
`17990ba0c2fb50338141f35c0717f0f99639c7ba`, base-repository ID `1328277631`,
and open state. Creating the fixture PR is not the RFC's comment mutation and
does not itself satisfy any live-provider, web-UI, identity, or mixed-client
criterion.

BPR-0256 v006 was approved and started once. Its approval was consumed and
durably spent, but the owner cancelled at the current generic manual-token
dialog without entering a credential, selecting a line, or publishing. The
owner reaffirmed that a login/device-authorization flow was supposed to be
built. v006 is non-retryable and produces no evidence for this card.

A future BPR-0256 packet must bind the RFC-BPR-0016 production device-login
path after BPR-0318 establishes its distributable identity. The operator then
approves and materializes a new one-use artifact, authenticates through that
flow, runs the single live test, and inspects the resulting first comment in
GitHub's web UI. Its one-comment authority still does not include posting a
reply; reply-return evidence requires a separately authorized attended
checkpoint. Tokens, refresh material, device codes, and credentials remain
outside repository artifacts, logs, argv, environment, and fixtures.

## The structural guard was rebuilt first; the executable boundary is separate

[[BPR-0221]], now in `60-done/`, rebuilt the provider inventory and
binary-symbol guard before a live implementation existed. It does not satisfy
this card's whole-process requirement by itself. [[BPR-0235]] separately owns
the CTest launcher boundary and sits in `10-backlog/` until Windows is proved
target-native and the only live exception enforces the now-designated
pull-request identity before child creation. Its Linux criterion is no longer
outstanding: [[BPR-0156]] re-established the Ubuntu 26.04 x86_64 matrix and
[[BPR-0358]] records that half as discharged on 2026-08-17.

Building the structural half before the live provider existed was the point: it is the only moment
the guard could be shown to fail on a tree whose answer was already known, and
it was — a rogue provider compiled into `review_mutation_tests` was caught by
both the source and binary halves.

**Three things this card now inherits as obligations, not choices.**

1. The live implementation must use the contracted name, factory, namespace,
   and file paths recorded in `docs/architecture/review-round-trip.md`. Any
   other name fails clause (A) of the gate. The code moves to fit the contract;
   the contract does not widen to fit the code.
2. The safe-target check goes in the composition root, immediately before the
   coordinator is constructed, comparing the requested repository against the
   compiled-in designated constant. Not in the coordinator, which cannot know
   what is documented, and not in the provider, which must not police itself.
3. **The runtime complement is this card's work.** Clause (B) deliberately
   excludes `app_better_pull_review`, because end-to-end tests execute it and
   after this card it will contain live code while running in fixture mode. A
   fixture-driven run must therefore emit `REVIEW_MUTATION_PROVIDER:fixture`,
   asserted in the end-to-end `PASS_REGULAR_EXPRESSION` chains. Symbol absence
   and runtime selection are different claims; the gate proves the first and
   only this marker proves the second.

Landing the provider also requires moving `live_review_mutation_provider` from
`NOT_COMPOSED` to `COMPOSED` in `scripts/verify_composed_hosts.py` in the same
commit — that entry exists so this cannot be forgotten.

## Current successor packet — 2026-09-04

BPR-0256 prepared the [v007 browser/device-login packet](../../verification/reviews/designated-live-review-publication-v007/operator-packet.md)
after BPR-0318, BPR-0541 and RFC-0017 implementation. A clean critical review
accepted candidate `ee159dba` and its exact stored capsule and packet; the
review is recorded on BPR-0256. It now awaits its own exact operator decision. v006 remains immutable, consumed and non-retryable. No
approval, live run, comment or GitHub UI observation exists for v007. The
one-comment course includes explicit Sign out cleanup; any web reply remains
a separate authority. This card's completion evidence is unchanged and open.

## v007 attended disposition — 2026-09-04

The owner approved v007 and its single run consumed the approval. Browser authorization completed, but the app refused GitHub data before establishing a session. No comment was published. Explicit Sign out and normal shutdown completed; the image detached. See the [frozen run result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md). BPR-0559/BPR-0560 now precede BPR-0256 regeneration; v007 cannot be retried.

## v008 prepared after sign-in repair — 2026-09-05

BPR-0559's repair is committed and BPR-0605 observed live sign-in success.
BPR-0256 prepared the [v008 packet](../../verification/reviews/designated-live-review-publication-v008/operator-packet.md)
with fresh application/source/supervisor/inventory/image and approval identities.
Clean exact critical review is recorded on BPR-0256. The packet now awaits
its own exact owner response. v006 and v007 stay consumed and
immutable. No v008 approval, live run, publication or GitHub web observation
exists; this card's outstanding acceptance remains unchanged.

## v008 consumed; agent UI-targeting failure — 2026-09-05

The [frozen v008 result](../../verification/reviews/designated-live-review-publication-v008/attended-result.md) records an aborted attempt: name-based UI selection launched/controlled an ordinary build alongside the approved mounted app. Device-flow interaction and an in-session device-panel screenshot occurred in that wrong instance; neither is compliant approved-build evidence. No PR was opened or comment submitted. Explicit Sign out and quit completed for both instances; CTest exited 0 in 892.24 seconds and the image detached. This is cleanup success, not publication or one-launch success. The approval is spent and cannot be replayed. Operating instructions now require full mounted-path selection, immediate PID/executable checks before and after binding, prompt device-flow initiation and filtered non-secret observations. No product source defect was established. BPR-0256 will prepare a fresh subject/packet using unchanged artifact evidence and this corrected procedure before any new approval request.

## v009 successor with direct owner authorization — 2026-09-05

The [v009 packet](../../verification/reviews/designated-live-review-publication-v009/operator-packet.md) reuses the unchanged v008 producer/image with a fresh canonical subject, corrected full-path/PID UI procedure and the same one-comment scope. The owner instructed, “I'm going to approve it so why even bother asking me, just make it and say I accepted please”. This direct instruction authorizes the bounded successor and overrides the local verbatim-response workflow; it is not represented as a pasted digest statement or a review of bytes not yet finalized. Finish exact critical review and static checks before materialization/launch. No prior spent approval is reused and no new run has started.

## v009 consumed; selected-file refusal — 2026-09-05

The [v009 result](../../verification/reviews/designated-live-review-publication-v009/attended-result.md) records the single approved mounted run: sign-in as MikeMarcin and all nine capability checks succeeded, and the correct designated PR opened. Selecting its sole file failed before source loading with a changed-file refusal and unavailable exact comparison history. No body was entered or Publish action taken. Explicit Sign out, quit and detach completed; CTest exit 0 in 321.64 seconds proves cleanup only. Device-field emissions are disclosed in the result; clean capture compliance is not claimed. The new approval is spent. Next: diagnose and repair the file/history path with fixtures before preparing another candidate. All live publication acceptance rows remain incomplete.

Current publication checkpoint: [v010](../../verification/reviews/designated-live-review-publication-v010/operator-packet.md),
a fresh rebuilt one-use candidate after the proven BPR-0608 history fix.
Standing owner authorization supplies continuation without another question;
remembered sign-in is preserved on quit. Exact prelaunch review remains pending;
no publication result or human acceptance is claimed. Earlier packets stay frozen.

Current publication checkpoint: [v011](../../verification/reviews/designated-live-review-publication-v011/operator-packet.md)
after BPR-0609 exposed the existing Publish/Discard actions as buttons. V010 was
reviewed but never materialized/launched. Its generated artifacts were removed.
Standing owner instructions remain the authority; no new user question is needed.
Current next slice: BPR-0256 exact v011 command check and one-comment execution.
