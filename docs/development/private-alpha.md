# Private alpha: shortest route to daily use

Owner direction, 2026-09-06: prioritize a usable personal work build for
GitHub.com and GitHub Enterprise; defer nonessential features and hardening.
Planning baseline: `8d6db643d1e599b1554123b44c5188905fa01065`, `main`.
This is a delivery plan, not runtime acceptance or a second issue-status ledger.

The owner confirmed this Mac as the first work machine.
Target one reviewer, one window, one PR at a time; open, closed and merged
pull requests are all readable, and publication stays on open ones. Use the existing
owner/repository/PR-number controls and explicit server selection. A terminal
launch and session-only Enterprise token are acceptable alpha compromises.
Enterprise Cloud on github.com uses the GitHub.com profile; a custom GHES
hostname uses the existing GHES 3.18 contract. Follow-up owner direction:
skip live Enterprise verification and implement support with reasonable
assumptions. Assume GHES 3.18-compatible REST/GraphQL endpoints, native HTTPS
with system certificate/proxy trust, and a user-supplied token permitted to
access the repository. Do not add GHES device authorization, automatic version
discovery, or certificate bypasses. Keep hostname configuration explicit.
Enterprise compatibility remains unverified against the owner's actual server;
neither its hostname nor access is required to deliver this alpha.

## What is already available

| Surface | Inspected state | Remaining alpha work |
|---|---|---|
| Production build | `app-release` exists; its cache selects Release and `BUILD_TESTING=OFF`; a Mac app bundle exists | Rebuild current source and launch it; an existing binary is not current-head runtime evidence |
| Authentication | GitHub.com device sign-in and remembered session; GHES explicit hostname and manual token | Confirm GitHub.com organization access; use local GHES profile/auth fixtures |
| Reading | PR metadata/conversations, selected-file diffs, retained epochs and private review ledger are composed | Read a representative GitHub.com private PR, navigate/refresh/reopen; cover GHES locally |
| Whole reviews | BPR-0615 composes ordinary Comment/Approve/Request Changes for the accepted target, with user-submit authority and recovery | Real attribution remains open in BPR-0144; do not reimplement it |
| Inline comments | Production `compose_review_mutation` still selects the compiled designated target and attended grant | Add ordinary user-initiated inline publication for the accepted PR |
| Mixed-client behavior | Existing comment publication and reply-return evidence; BPR-0143 retains remaining live checks | Reuse the prepared checks; do not restart the prior validation program |

These findings come from `src/app/src/github_application.cpp`, the selected
build configuration, BPR-0615, and the live RFC-0008 cards. Component/fixture
success does not establish that the owner's GHES deployment works.

## Execution order and stopping points

1. **Deliver a reading candidate first.** Build only the production app target
   below. Launch the ordinary app, select native HTTPS, and use the current PR
   locator. Check one representative private PR on GitHub.com: correct
   account/target, readable diff and
   comments, useful navigation, refresh, and private receipt/note restoration
   after an orderly restart. Fix only failures that stop that course. Hand over
   the build immediately for reading, with web publishing as the temporary
   workaround. Do not wait for inline publication or RFC closure.
2. **Make ordinary inline Publish usable.** Allocate one bounded RFC-0008 card
   for production user-publish authority and composition. Reuse the existing
   mutation host/provider, semantic anchors, recovery, and the whole-review
   user-submit pattern. Scope: `src/application/`, the narrow GitHub adapter
   seam if needed, `src/app/`, and the existing Qt editor only as needed.
   Bind each gesture to the exact host/account/repository/PR/head/anchor/body;
   preserve stale-head refusal, identity invalidation and duplicate prevention.
   Keep automated tests isolated from live services. Do not turn the designated
   test grant into a universal grant or build another attended launcher.
   Limit alpha comments to supported current-diff anchors; off-hunk publication
   and general historical comment mapping remain deferred.
3. **Verify the collaboration loop on GitHub.com; cover GHES locally.** Use
   the same candidate for
   an inline comment, web reply/refresh, and whole-review submission with
   authoritative account checks. Reuse BPR-0143/BPR-0144 and their existing
   evidence where applicable. For GHES, exercise profile selection, token
   authentication contracts, endpoint routing, diff reads and publication
   contracts with existing fixtures; add only coverage missing from the changed
   path. No live Enterprise course or credentials are required. Use a
   disposable non-self-authored PR for Approve. For alpha, thread resolution,
   comment editing/deletion and other unavailable actions may use the web UI;
   they must not claim success in the app. Required full-RFC checks stay open.
4. **Freeze the usable candidate.** Record source revision, launch command,
   tested host and concise known limitations, explicitly stating that GHES
   support has local coverage but no live-server verification. Keep a known-working
   build before replacing it. Stop adding features once GitHub.com reading,
   ordinary inline comments, whole-review submission, refresh and restart pass,
   together with the focused local GHES contracts.
   Fix failures encountered in real work next.

The first candidate is a reading companion. The private review alpha adds
ordinary inline comments and whole reviews. Neither label claims full RFC-0008
closeout or production release readiness.

## Immediate build and launch

From the repository root on this Mac:

```sh
cmake --preset app-release
cmake --build --preset app-release --target app_better_pull_review
build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
```

For later GHES use, append `--ghes-host YOUR_GHES_HOSTNAME`, using the actual
hostname without a URL path. Enter tokens only in the app's credential
prompt. The tests-enabled `dev` app refuses ordinary live whole-review
composition, so it is unsuitable as the work alpha.

This route uses the local build tree and its installed dependencies. Mac release
builds now seal the finished bundle locally. Configure
`-DBPR_MACOS_CODESIGN_IDENTITY="YOUR EXISTING SIGNING IDENTITY"` once to retain
Keychain trust across rebuilds; this Mac's ignored build cache already selects
the owner's existing Developer ID identity. The default is ad-hoc signing.
The build contacts no timestamp/notarization service. This is not a relocatable
or notarized distribution package. If another machine is the first
target, produce and smoke-test that target's app/dependencies before delivery;
do not expand to the full release platform matrix.

## Verification budget

Reuse BPR-0615's unchanged whole-review evidence. For the inline slice, run
focused mutation/core/host checks and the real composition course; cover
wrong target/account, moved head and lost-result recovery. Run affected
architecture/test-isolation checks when authority changes. At delivery, do
one production build, one practical GitHub.com course and focused local GHES
checks, not repeated full matrices. Skip live Enterprise verification by owner
direction. Documentation changes use the normal documentation gate.

Existing authorization for specific prepared test actions remains scoped to
those actions. Implementation and planning authorize no new remote messages.
Before agent-run publication, bind a designated target and exact action, reuse
applicable prior permission, and honor the existing no-paid-CI/no-Actions
constraint. No pushes or workflow dispatches are part of this route.

## Work deliberately excluded

Owner direction, 2026-09-06 (later the same day): the workbench UI is
unusable as delivered, so the owner explicitly requested the deferred UX
redesign now. This is the post-pack frontier exception RFC-BPR-0018,
RFC-BPR-0024, RFC-BPR-0025 and RFC-BPR-0026 name. Work it in bounded slices
that make the alpha usable first: BPR-0645 (one palette per resolved variant),
BPR-0646 (chrome, epoch line, modalities, rails), then BPR-0647 to BPR-0650
(overview, aligned comments, Discuss, Home). The owner locked that direction
on 2026-09-06 as RFC-BPR-0028; the Settings pages, the repository/pull-request
explorer and the file tree are designed next. Do not start CLI authentication, AI/agent expansion, private-thread composition,
off-hunk comments, historical conversation joins, mascots, notification polish,
advanced diagnostics, schema discovery or broad refactoring for this alpha.
BPR-0622 agent-workspace recovery and BPR-0635 cache-pressure verification are
not prerequisites for this Mac reading/review course; their live status remains
unchanged. Avoid agent execution in the initial course.

Defer the full RFC-0013/0014 resilience, packaging, signing, notarization and
cross-platform qualification program. Keep existing TLS, credential isolation,
exact-target/head checks, persistence and mutation recovery intact. A concrete
data-loss, wrong-target, duplicate-publication or credential defect blocks the
affected alpha path; speculative hardening does not.

RFCs still close in canonical order. The owner's alpha priority permits this
bounded delivery work without completing the entire pack first; it does not
mark open criteria passed or activate the deferred redesign RFCs. On resume,
start with step 1, then allocate/claim only the next essential implementation
slice through implement-rfc. Keep acceptance on its owning live card.

## Implementation continuation — BPR-0641

BPR-0641 owns ordinary inline publication and the Mac candidate. The owner
named a private GitHub.com PR for a read-only smoke check; its content is not
retained in this repository. No comment/review submission to that work PR is
authorized. Authentication and live observation results belong on the card. The first
named target did not load and is retained as a stress test on BPR-0643; the
owner then named a small closed pull request for the read-only smoke check.
On 2026-09-25 the owner accepted that smoke on a tests-disabled build of
`3759b0d8`: signed in through GitHub CLI, the pull request opened read-only and
listed its files, and BPR-0641 is Done. No file showed a comparison there; that
is BPR-1199. Signed out, the link passed through an empty Overview before Home
refused it in an easy-to-miss line; that is BPR-1200.
