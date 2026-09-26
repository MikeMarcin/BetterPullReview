---
schema: "implement-rfc.card.v1"
id: "BPR-0641"
title: "Enable normal inline publication for the Mac private alpha"
status: "Done"
rfcs: []
depends_on: ["BPR-0615"]
priority: "P1"
metadata: {"origin_rfc":"RFC-BPR-0008","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; AC1-AC3 pass; AC4 accepted by the owner 2026-09-25 (session 06ad5734, decision cbba6fac) with the file comparison failure carried by BPR-1199","followups":["BPR-1199","BPR-1200"]}
acceptance: [{"criterion":"AC1 ordinary production inline Publish selects the accepted authenticated PR with a short-lived one-use grant bound to session, generation, target, actor, proposal/body, selection and exact retained diff publication","outcome":"pass","revision":"50f364b8","evidence":{"kind":"observation","detail":"Production compose_review_mutation selects the observed accepted handoff unless attended mode is set; the private review_mutation_user_authority_issuer mints a user_one_shot grant bound to the complete publication request, session, generation, target and actor, expiring after five minutes. application_review_round_trip_composition_contract repinned the production mutation-branch digest and drove the fixture editor Publish through per-gesture grants."}},{"criterion":"AC2 stale-head, endpoint identity, wrong-target, cancellation and durable duplicate-prevention remain intact; automated builds cannot issue normal production authority; attended designated lane stays separately scoped","outcome":"pass","revision":"50f364b8","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -R focused 27-selector set passed 27/27 (427.95 s); includes application_review_mutation_application_host, application_review_mutation, live_review_mutation_provider, live_review_mutation_restart_recovery, sqlite_review_mutation_recovery_storage, live_mutation_isolation, live_mutation_isolation_contract, architecture_boundaries and the network isolation selectors. Host test asserts the issuer returns no grant under BPR_APPLICATION_TESTING."}},{"criterion":"AC3 focused host tests prove altered request/actor/target and replay refusal plus sequential distinct publications; real composition exercises the normal editor and unchanged source-line flow; local GHES contracts pass; live Enterprise verification skipped by owner instruction","outcome":"pass","revision":"50f364b8","evidence":{"kind":"command","detail":"test_user_grants_bind_exact_requests_and_allow_successive_gestures passed inside application_review_mutation_application_host; application_review_round_trip_composition_contract (54.91 s), application_github_public_path_ghes_3_18, application_public_path_ghes_cli_unavailable, application_device_authorization_ghes_refusal, application_epoch_persistence_ghes-3_18 and application_anonymous_public_preview_ghes_refusal passed. No live Enterprise course was run."}},{"criterion":"AC4 the tests-disabled Mac production app builds and launches with direct observations and limitations recorded; private-PR smoke waits for an owner-named target","outcome":"pass","revision":"3759b0d8caf5f1732c8bf807be427525f3f9a89a","evidence":{"kind":"observation","detail":"Owner verdict accept, attended session 06ad5734ed9f7c2fd6d0a62c8328fa0a (packet docs/verification/reviews/owner-review-20260925-fresh-v001), candidate BPR-0641-private-pr-smoke-v001, fingerprint 2ef08a50eb320f3197cc90193f1e6051a703e6c353a28afb8ee64a326f00a272 (equal to the prepared one), decision cbba6fac-e7dc-4b39-a7aa-1dbd0f7e1571, sequence 36, 2026-09-26T04:39:15Z, actor human. Candidate: cmake --preset app-release and --build on a git archive export of 3759b0d8 (389/389 steps, tests disabled), ad-hoc signed, codesign --verify --deep --strict passes, executable 5b3984204f5a63ebf3bcc350d1deca1a1fb0aab6bb985343f95a3e738e0fe1ed; the preparer launched it signed out with a scratch home and no network and did not sign in. The owner, on the normal profile, signed in through GitHub CLI and opened EpicGames/UnrealEngine#14366 read-only: Overview listed its 40 files. Limitation: no file showed a comparison (readiness row 'File comparisons unavailable. Review history could not be updated. The exact comparison is unavailable.'); the owner accepted with that failure noted as a separate defect, now BPR-1199. The signed-out attempt passed through an empty Overview before Home refused it in an easy-to-miss line; the owner asked for BPR-1200. Read-only by instruction; no publication is claimed.","path":"docs/issues/60-done/BPR-0641-enable-normal-inline-publication-for-private-alpha.md#owner-verdict-on-the-read-only-smoke--2026-09-25-attended-session-06ad5734"}}]
---

# Enable normal inline publication for the Mac private alpha

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this card's remaining private-PR smoke acceptance
out of RFC-BPR-0008's terminal gate. The card remains Parked for human review;
AC1–AC3 keep their recorded implementation evidence and AC4 remains pending.
The failed historical private-target read is not relabeled a success. No owner
verdict, new private-target observation or publication is inferred from later
loading repairs. Next: resume the read-only course below on an exact current
candidate when scheduled; retain its named targets and permission limits.
The [amended RFC](../../rfcs/RFC-BPR-0008-github-review-round-trip-interactions.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0008.md) make this deferral discoverable.

Owner-approved scope: [private-alpha route](../../development/private-alpha.md).
RFC_BASE_SHA: `ae4480bc`. Keep full RFC closeout on BPR-0242; live account and
mixed-client acceptance remain on BPR-0143/BPR-0144. This implementation slice
requires local evidence and a current Mac production build, not new remote posts.

## Owned paths

Application mutation authority/host; app composition; their focused host,
composition and GHES contract checks; affected architecture/isolation inventories
and product/operator documentation. Reuse the existing editor/provider/recovery.
The observed Mac Keychain blocker also requires local post-build bundle signing
in `src/app/CMakeLists.txt`, its preset description and build instructions.

## Acceptance

- Ordinary production inline Publish selects the accepted authenticated PR on
  GitHub.com or explicitly configured GHES, without a designated-target launcher.
  Each gesture supplies a short-lived one-use grant bound to session, generation,
  target, actor, proposal/body, selection and exact retained diff publication.
- Existing stale-head, endpoint identity, wrong-target, cancellation and durable
  duplicate-prevention paths remain intact. Automated test builds cannot issue
  normal production authority; the attended designated lane stays separately scoped.
- Focused host tests prove altered request/actor/target and replay refusal plus
  sequential distinct user publications. Real composition exercises the normal
  editor and unchanged source-line/publication flow. Local GHES contracts pass;
  live Enterprise verification is explicitly skipped by owner instruction.
- The tests-disabled Mac production app builds and launches. Record direct
  observations and limitations; private-PR smoke waits for an owner-named target.

## Evidence

Pending implementation and focused verification. Do not infer live publication
acceptance or RFC closure from this card.

## Implementation and verification — 2026-09-06

Codex implemented this slice and ran out of usage while editing the
private-alpha route document; this session resumed from the uncommitted
working tree, rebuilt both trees from the current source and verified it.

The execution authority gains a private `user_one_shot` kind. The
`review_mutation_user_authority_issuer` mints one grant per core-accepted
Publish gesture, bound to the complete publication request (proposal, semantic
selection and retained diff publication), session, generation, target and
acting identity. It expires after five minutes, is consumed once after host
endpoint validation, and refuses any altered request field or replay. The host
`publish` accepts a per-request grant and falls back to its construction grant,
which ordinary production composition leaves absent, so a missing grant still
fails closed. Production composition selects the observed accepted handoff
unless the attended designated launcher set attended mode; that mode keeps the
compiled designated target and one-shot bootstrap and cannot fall back to
ordinary issuance. The retained inline binding is reset on host retirement and
must match the accepted snapshot's account and pull request before issuance.
Under `BPR_APPLICATION_TESTING` the issuer returns no grant; the public fixture
uses request-bound testing grants instead of one reusable host grant. The editor
target line now names the host and the authenticated login rather than the
account identifier. The Mac release bundle is sealed post-build with
`codesign --force --deep` using `BPR_MACOS_CODESIGN_IDENTITY` (ad-hoc default;
this Mac's ignored cache selects the owner's existing Developer ID identity).

Verification at this revision:

- `cmake --build --preset dev` completed with no warnings (212 steps).
- Focused selectors passed 27/27 in 427.95 seconds
  (`ctest --test-dir build/dev --output-on-failure -R` over the named set):
  `application_review_mutation_application_host` (new
  `test_user_grants_bind_exact_requests_and_allow_successive_gestures`: altered
  body/draft/acknowledgement/path, wrong actor/target/session/generation,
  replay, a second distinct gesture, and a retired host),
  `application_review_mutation`, `live_review_mutation_provider`,
  `live_review_mutation_restart_recovery`,
  `sqlite_review_mutation_recovery_storage`, `core_review_round_trip`,
  `qt_review_draft_editor`, `qt_composed_github_scale_100`,
  `application_public_path`, `application_github_public_path_github_com`,
  `application_github_public_path_ghes_3_18`,
  `application_public_path_ghes_cli_unavailable`,
  `application_device_authorization_ghes_refusal`,
  `application_epoch_persistence_ghes-3_18`,
  `application_anonymous_public_preview_ghes_refusal`,
  `application_review_submission_composition_public_path`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (runs the fixture app
  with `--review-round-trip-composition-probe`: the normal editor's Publish
  gesture, stale-head refusal, lost-effect recovery, retry idempotence and
  server-authority projection now admitted by per-gesture request-bound
  testing grants; the full application, production manifest and production
  mutation-branch digests were repinned),
  `live_mutation_isolation`, `live_mutation_isolation_contract`,
  `test_network_isolation_structure_contract`, `test_network_isolation_manifest`,
  `test_network_isolation_application`, `architecture_boundaries`,
  `architecture_boundary_contract`, `documentation_structure` and
  `documentation_issue_graph_contract`.
- `VerifyCoreBoundary.cmake` and `VerifyArchitectureBoundaries.cmake` passed
  directly; `scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`; `clang-format --dry-run --Werror` on the five touched C++
  files and `git diff --check` exited 0.
- Strict `tidy.py --build-dir build/dev --warnings-as-errors` on the touched
  files reported four `readability-identifier-naming` errors, all in regions of
  `github_application.cpp` this slice did not change and all present at HEAD
  (`ordinal_text`, `ref_oid`, `indentation_width`, `escape_control_characters`).
  They are a follow-up, not this card's defect; the other four files were clean.
- The tests-disabled candidate: `cmake --preset app-release` then
  `cmake --build --preset app-release` rebuilt `app_better_pull_review` and the
  post-build step reported `replacing existing signature`. `codesign -dv`
  shows Developer ID Application authority and a sealed-resources bundle;
  `codesign --verify --deep --strict` reported valid on disk and satisfies its
  Designated Requirement. Launched from the terminal, the process stayed alive
  for 12 seconds, wrote nothing to stdout/stderr, rendered the workbench window
  with the `GitHub.com — Native HTTPS` profile, `Not authenticated`, empty
  owner/repository/PR controls, files/conversations drawers and `Review pull
  request`, and exited cleanly on SIGTERM. A second release-bundle process left
  running from the 01:55 build was not touched.

Limitations and remaining human steps: no live GitHub.com or GHES publication
occurred and none is claimed. The owner-named private PR read-only smoke check
was not performed; that target was not available to this session and the
attended sign-in course remains for the owner. Live Enterprise verification is
skipped by owner instruction; GHES coverage is the local 3.18 contract set.
Real attribution and mixed-client acceptance stay on BPR-0144 and BPR-0143;
this is not RFC-BPR-0008 closeout. The temporary build and test logs under the
session scratchpad were consumed and removed.

Implementation commit: `50f364b8`. The card and the private-alpha continuation
note are committed by finish.
## Owner-named read-only smoke — 2026-09-06

The owner named `EpicGames/UnrealEngine#1773` for a read-only check and
signed the 01:55 release instance in with the `MikeMarcin` account through the
device code flow; no comment, review or other write was authorized or made.
That instance links the same compiled application object (built 01:50 from the
unchanged source now at `50f364b8`); only the launcher was relinked and the
bundle re-signed afterwards.

Observed on the running instance's window: `Host profile: GitHub.com — Native
HTTPS`, `Active host: github.com`, `@MikeMarcin`, owner and repository fields
populated, status `The pull request could not be loaded.` and `No pull request
is open in the workbench.` The diagnostics log records `github.viewer` and
`github.capability` succeeding for that session and nothing afterwards; pull
request load outcomes are not logged, so the failure class was not recoverable
offline (BPR-0642).

The target is atypical: opened in 2015 from a fork, 9311 commits of which
GitHub lists only the newest 250, 1523 files, 111 issue comments, 19 review
threads. Ruled out by read-only `gh` inspection against the read model at this
revision: collection bounds (files 1523 of 3000, commits 250 of 250), body and
title sizes, the largest patch (87 KB) and largest files page (0.9 MB), missing
patches (optional), null commit authors (167, optional), file statuses (all
accepted values), duplicate paths or commit OIDs (none), head presence in the
listed commits (present), and the exact review-threads GraphQL document (no
errors, 19 threads, nullable fields only). The cause remains undetermined.

Result: sign-in and identity observation passed; the named PR did not open.
AC4 stays pending. Next attended step: open the session panel's Details to
read the load advisory, and retry with a representative current work PR.

Update, 2026-09-06: the owner retained #1773 as a stress test
([BPR-0643](BPR-0643-load-the-retained-stress-test-pull-request.md))
and named `EpicGames/UnrealEngine#14366` (closed, 4 commits, 40 files, 9 review
comments) as the smoke target. The read model accepts open, closed and merged
pull request records; only queue search results are open-only. The smoke
remains an attended step on the running signed instance.

## Owner verdict on the read-only smoke — 2026-09-25 (attended session 06ad5734)

Packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md),
session `06ad5734ed9f7c2fd6d0a62c8328fa0a`, candidate
`BPR-0641-private-pr-smoke-v001`: `3759b0d8` built with `app-release` (tests
disabled, ad-hoc signed) from a `git archive` export that excludes the
uncommitted BPR-0972 work, executable `5b398420…e1ed`, fingerprint
`2ef08a50eb320f3197cc90193f1e6051a703e6c353a28afb8ee64a326f00a272`. The
preparer built it (389 of 389 steps), verified the signature and launched it
signed out with a scratch home and the network denied; it did not sign in or
open the pull request. The owner then ran it on the normal profile.

Decision `cbba6fac-e7dc-4b39-a7aa-1dbd0f7e1571`, sequence 36,
2026-09-26T04:39:15Z, `actor: human`, fingerprint equal to the prepared one:
**accept**, with this note, verbatim:

> The PR loads, although I hit a weird broken overview page after going to the URL without an account, getting kicked back to home, and then connecting with CLI. But loading the PR again worked. However I wasn't able to view a file (this may be already carded).
>
> Also the error message (4th screenshot) for getting kicked out feels too subtle and is easy to miss, I think we should file a follow-up card to improve that UX.

The four screenshots show a private repository and stay in the session's
external state; they are not committed. What they show, in the app's own words:

1. Signed out, after pasting the link: the workbench on Overview with two empty
   panes, "Select a file comparison" and the status line "No pull request is
   open in the workbench."
2. Signed in through GitHub CLI, the pull request open on Overview: its
   description, participants and the file list ("Files: 40 in this pull
   request, 40 not read"), under the readiness row "File comparisons
   unavailable. Review history could not be updated. The exact comparison is
   unavailable. Refresh to try again."
3. Read, with a file selected: the same readiness row, empty panes reading
   "Select a file comparison", nine review threads listed as "Code anchor
   unavailable", and the status "Actionable file 8 of 40. Review history could
   not be updated. The exact comparison is unavailable."
4. Home after the signed-out attempt: under the Open field, "This app's GitHub
   connection cannot read this pull request. Your browser sign-in may have
   different access.", beside the Sign in card.

**Interpretation (the integrator's, 2026-09-25).** The candidate's criterion
asked whether #14366 opens read-only and shows its files and a diff. It opened
read-only and listed its files; no diff rendered. The owner accepted with that
failure named and set it aside as a separate defect, so this records AC4 as
the owner's acceptance of the smoke, not as an observed diff. The file
comparison failure is not already carded: the matching wording was
[[BPR-0922]]'s, which is Done and fixed a recorded GHES/CLI course without
observing the owner's live cause. It is now [[BPR-1199]]. The signed-out path's
empty Overview and the easy-to-miss refusal on Home are [[BPR-1200]], the
follow-up the owner asked for. No comment, review or publication is claimed;
the candidate was read-only by instruction.

AC4 passes on that verdict, and with AC1–AC3 unchanged the card is Done. Live
Enterprise, real attribution ([[BPR-0144]]) and mixed-client acceptance
([[BPR-0143]]) are unchanged.
