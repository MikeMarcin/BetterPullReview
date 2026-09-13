---
schema: "implement-rfc.card.v1"
id: "BPR-0643"
title: "Load the retained stress-test pull request"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0642"]
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"The diagnostics log names the failure class for this target.","outcome":"pass","revision":"5d5c8b661d16a95cf11c7fe860524629571ed481","evidence":{"kind":"observation","detail":"Rebuilt tests-disabled Mac candidate at 5d5c8b661d16a95cf11c7fe860524629571ed481, signed in as MikeMarcin on GitHub.com Native HTTPS, attempted EpicGames/UnrealEngine#1773 read-only. Exact two-line appended log at /Users/jaeger/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-2-diagnostics.log (SHA-256 a7fe87f2880f68e97d578c53997c56d9b0a93175c0654643a36d7f3d2aa50b9f) records github.pull_request started then failed, failure=permission, page_operation=GetRepository, request=1/window_session=12; the first historical failed log is also preserved on the card."}},{"criterion":"Either the target opens read-only in the Mac candidate with correct account, file list and a readable selected diff, or the refusal is presented with its reason and recorded here as a known limit.","outcome":"pass","revision":"5d5c8b661d16a95cf11c7fe860524629571ed481","evidence":{"kind":"observation","detail":"The rebuilt candidate session panel displayed: The pull request could not be loaded. This GitHub sign-in does not have permission to read this pull request. The card records permission at GetRepository as the device-grant known limit; no file page or diff was reached. Read-only CLI GET under the same account succeeds but its credential cannot be adopted by this native HTTPS profile. qt_composed_github_scale_100 and coding-format-check passed after the wording correction."}},{"criterion":"No live write occurs.","outcome":"pass","revision":"5d5c8b661d16a95cf11c7fe860524629571ed481","evidence":{"kind":"observation","detail":"The owner completed only device authorization; task target interactions were gh api -X GET and two application Open pull request read actions. No Publish, review, comment, approval, label, push or other PR mutation was invoked. Private PR content and attempt logs were not retained in the repository."}}]
---

# Load the retained stress-test pull request

## Current frontier — bpr0643-r1 / d1 / mandate r2

The bound `retained-stress-pr-read` milestone now has a direct read-only
outcome for the owner's exact `EpicGames/UnrealEngine#1773` target. BPR-0642
is Done; the owner completed product sign-in through wizard
`bpr0643-signin`, and the Mac candidate observed `MikeMarcin` on GitHub.com
Native HTTPS. The target failed at `GetRepository` with the typed `permission`
class. The first candidate's session panel displayed a permission refusal,
but its wording attributed the denial to the account even though a separate
credential for that account can read the repository. The narrow Qt copy change
at `5d5c8b66` now attributes it to the current sign-in. After rebuilding, the
same candidate resumed the owner's device grant and a second read-only load
showed the corrected refusal text. Both exact attempt logs are retained
outside the repository. The target's file list and diff were not reached.
No live write, credential exposure or private PR content retention occurred.

The authenticated `gh` CLI session reports the owner's `MikeMarcin` account
and read-only GET confirms the target is open with 1,523 changed files and
9,311 total commits. Before manual sign-in, the product could not adopt CLI
credentials and its sole persisted native device binding was tombstoned. The
network-isolated dev courses
`application_github_window_read_host`, `github_paged_read`,
`qt_composed_github_scale_100`, `application_github_public_path_github_com`
and `application_logging` pass 5/5 at the current source. The pager's fixture
accepts 3,000 files and 250 returned commits, but this cannot establish the
target's runtime outcome. A task-launched dev UI process was stopped after its
computer-use attachment timed out; it produced no new product diagnostic.
The fresh tests-disabled Release candidate builds at
`build/bpr0643-release/src/app/BetterPullReview.app` from this source. Its
ad-hoc local signature passes `codesign --verify --deep --strict`; direct Mac
launch rendered the workbench, GitHub.com Native HTTPS profile and Accounts &
hosts page, which initially said `No account` and `Signed out`. It accepted
the owner's device authorization without further signing; this dispatch did
not use signing credentials.

Mandate r2's manual sign-in wizard completed at revision 2. The fresh
candidate's actual Accounts & hosts page shows `MikeMarcin`, active
`github.com` on GitHub.com Native HTTPS, and `GitHub account connected.`
Immediately before the target attempt, the product's metadata-only diagnostics
log was inode 309064343 at byte 290786. No CLI credential was transferred.

## Read-only target result and known limit — 2026-09-13

The signed-in tests-disabled Mac candidate attempted
`EpicGames/UnrealEngine#1773` once through the Accounts & hosts Open pull
request control. Its new `github.pull_request` diagnostic records `started`
then `failed`, with `failure=permission` and final
`page_operation=GetRepository` for request 1/window session 12. No file page
or selected diff was reached. The same session panel displayed:
`The pull request could not be loaded. The authenticated account does not
have permission to read this pull request.` The exact two-line attempt log
is retained only outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-1-diagnostics.log`
(SHA-256 `f3b35aa9220d238f082295fe65937b53aa2308fa216dd1239570ff08f3badea9`).

At `5d5c8b66`, the rebuilt candidate resumed the same `MikeMarcin` sign-in.
A second read-only Open produced the same `permission`/`GetRepository` terminal
and the session panel now displayed:
`The pull request could not be loaded. This GitHub sign-in does not have
permission to read this pull request.` Its exact two-line appended log is at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-2-diagnostics.log`
(SHA-256 `a7fe87f2880f68e97d578c53997c56d9b0a93175c0654643a36d7f3d2aa50b9f`).
The changed Qt wording passes `qt_composed_github_scale_100` and the source
tree passes `coding-format-check`. The prior five focused diagnostic, pager,
Qt and public-path courses remain valid for unchanged product behavior.

This is the explicit known limit for the current device grant: the product
cannot read this repository with that grant. A separate read-only CLI GET as
the same `MikeMarcin` account succeeds, but the product deliberately cannot
adopt CLI credentials. The evidence does not prove whether GitHub App
installation policy is the underlying permission cause, and it does not
exercise the 1,523-file stress shape past repository access. No product code
or limit changed. The only live actions were CLI GET and the app's read-only
Open pull request; no review, comment, approval, label, push, publication or
other live write occurred.

This card was claimed on main after the human sign-in step. Only its live
record and bounded product/test paths are owned. Existing
`.gitignore` and `.mask.json` edits are unrelated.

The owner retained `EpicGames/UnrealEngine#1773` as a stress-test target after
the signed private alpha candidate reported `The pull request could not be
loaded.` for it while connected as the owner's account. It is an open 2015
pull request from a fork with 9311 commits (GitHub lists only the newest 250),
1523 changed files, 111 issue comments and 19 review threads. Read-only
inspection against the read model at `50f364b8` found no bound, nullable
field, status value, duplicate identity or GraphQL error that explains the
refusal; the observation is on
[BPR-0641](../40-parked/BPR-0641-enable-normal-inline-publication-for-private-alpha.md).

## Scope

After BPR-0642 records load outcomes, reproduce the failure with the owner's
account, read the failure class from the diagnostics log, and either lift the
limit that refuses this shape or make the refusal explicit in the session
panel with the reason. Reading only; publication to it is not in scope. Do not
retain the pull request's content in the repository.

## Acceptance

- The diagnostics log names the failure class for this target.
- Either the target opens read-only in the Mac candidate with correct account,
  file list and a readable selected diff, or the refusal is presented with its
  reason and recorded here as a known limit.
- No live write occurs.
