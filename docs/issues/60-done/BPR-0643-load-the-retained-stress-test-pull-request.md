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

## Current authorization frontier — bpr0643-r1 / d5 / mandate r7

The renewed assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d5-r7.json`.
On 2026-09-13, the owner's signed-in Chrome session at
`https://github.com/settings/installations` identified `@MikeMarcin` and
listed BetterPullReview among GitHub Apps installed on that **personal**
account. Opening its Configure page at
`https://github.com/settings/installations/157150648` reached GitHub's
`Confirm access` sudo-mode reauthentication page before exposing repository
selection; the `Authorized GitHub Apps` page was gated the same way. No
reauthentication, credential entry, permission change or installation action
was attempted. The current personal installation's selected repositories are
therefore not verified. A personal installation would not itself grant access
to a private repository owned by `EpicGames`, and this personal-account page
cannot establish whether a separate `EpicGames` organization installation
exists.

A fresh read-only CLI `GET /repos/EpicGames/UnrealEngine` reports
`private=true`, `permissions.pull=true`, `permissions.admin=false`, and
`permissions.maintain=false`. That credential's
`GET /orgs/EpicGames/installations` returned HTTP `404`; `gh` identified
`admin:org` as required for the organization-installation API. The CLI
credential also cannot call `GET /user/installations`, which requires a GitHub
App user token. No CLI scope refresh was attempted. Thus neither the browser
page available without sudo mode nor the separate CLI credential establishes
whether BetterPullReview is installed on `EpicGames`, whether that installation
includes this repository, or whether organization SSO affects the app's
authorization. The signed app's prior HTTP `404` remains the direct evidence
that its **current** grant cannot read this target.

GitHub documents that a user token can access only resources available to
both the user and app, that organization owners can review installed Apps and
their repository selection, and that SSO-protected organizations may require
an active SSO session before app authorization
([user-token rule](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-a-user-access-token-for-a-github-app),
[organization review](https://docs.github.com/en/organizations/managing-programmatic-access-to-your-organization/reviewing-github-apps-installed-in-your-organization),
[SSO behavior](https://docs.github.com/en/enterprise-cloud@latest/apps/using-github-apps/saml-and-github-apps)).
An `EpicGames` organization owner or repository administrator permitted by
organization policy would need to inspect the organization's BetterPullReview
installation and exact repository selection to identify the missing grant;
if those are already correct and the organization enforces SSO, the app
authorization's SSO state would need inspection. This signed-in user and CLI
credential have not demonstrated access to that administrative view, and no
authorization setting was changed. The product session remains
connected. The original `invalid_relationship` attempt is still not
reproducible on this refusing grant; no file list or selected diff was reached.

## Previous correction frontier — bpr0643-r1 / d4 / mandate r6

The owner clarified that the `MikeMarcin` account can read this repository on
GitHub.com. D3's suggestion to switch to a manual token was premature and is
withdrawn; the connected product device grant remains untouched. The current
investigation is whether the product's exact `GetRepository` request or
failure mapping is defective, or GitHub is limiting this GitHub App's grant
independently of the user's website access. Both the archived pre-d2 signed
binary and current signed binary embed the same public production GitHub App
client identifier. Its 2026-08-28 registration observation recorded an
installation limited to selected repositories, then only
`MikeMarcin/BPR-Test`; that historical record does not prove today's
installation selection. GitHub's documented user-token rule is the
intersection of user and app repository access. D4 traced the exact request
and current result without reading credentials or changing sign-in.
The preserved original Accounts & hosts window and the current product both
show a renewing, remembered `MikeMarcin` device grant; no manual-token session
is evidenced for the original malformed-data attempt. A fresh read-only CLI
`GET /repos/EpicGames/UnrealEngine` reports this target as private and
account-readable, but uses a separate credential. The product's prior
terminal log did not retain the failed HTTP status, so the recorded
`permission` class alone could not distinguish GitHub `403` or `404` from a
Qt content-denial transport result.

The native `GetRepository` request uses `GET
https://api.github.com/repos/EpicGames/UnrealEngine` with the current
credential bound as a bearer token. D4 added fixed, metadata-only HTTP status
and transport-source fields to the provider and diagnostic path, while keeping
the refusal policy unchanged. The final Developer ID signed `build/app-release`
candidate (binary SHA-256
`b13e1fa8650579b507a0611889046c4f029f567fe93a4cccb85751d4c0075f77`)
resumed the remembered `MikeMarcin` device grant after launch. One exact
read-only Open of `EpicGames/UnrealEngine#1773` produced
`failure=permission`, `page_operation=GetRepository`, `http_status=404`, and
`transport_source=http_response` (request 1/window session 16). The panel
displayed `Pull request unavailable` and `This app's GitHub connection cannot
read this pull request. Your browser sign-in may have different access.` The
two-line appended log is retained only outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-9-d4-diagnostics.log`
(SHA-256 `d32e59b0604c1147bb02af1b3e2542a7c0b10811f76200dea474c812418749fd`).
This is an HTTP refusal from GitHub, not a Qt transport denial or an invalid
JSON/relationship result. The separate CLI credential still reads this
private repository; its `GET /user/installations` cannot inventory this App's
grant because GitHub requires a GitHub App user token for that endpoint. The
app grant's precise current installation selection or organization policy is
therefore unverified. No file list or selected diff was reached, and the
original `invalid_relationship` failure remains unproven solved. No token was
viewed or replaced, and no live PR write occurred.

## Previous correction frontier — bpr0643-r1 / d3 / mandate r5

The owner completed the new `bpr0643-reconnect` wizard's manual device
reconnection step; its human acknowledgement is revision 1. At revision 2,
the agent recorded the observed result: the final signed Mac app showed
`MikeMarcin` connected on GitHub.com Native HTTPS, but one exact read-only
Open of `EpicGames/UnrealEngine#1773` again stopped at
`permission`/`GetRepository` (request 1/window session 16). The session panel
said `This GitHub sign-in does not have permission to read this pull request.`
The appended two-line metadata-only log is retained only outside the checkout
as `~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-8-reconnect-diagnostics.log`
(SHA-256 `9d41955ce9521f34e37878d9221c9fe6cf321475e851d0f1dcafcbeefac20664`).
No file list or selected diff was reached, and the original
`invalid_relationship` invariant remains unreproduced on the current grant.

The acknowledged reconnect did not restore repository read access. The
product supports an owner-entered manual token on GitHub.com; that session is
ephemeral, and replacing the device grant removes the prior grant. A token
the owner can establish has access to this exact repository is the next
product-controlled route to test. The owner must enter it directly in the
app, not in the wizard, chat or checkout. Its access is not assumed from the
separate CLI credential. Preserve the current connected product session until
the owner chooses that replacement. Once product read access is observed,
retry this target read-only and use any fixed `relationship_detail` to
diagnose a reproduced validation failure. No live write, credential reading,
or private PR content retention occurred in d3. The Done acceptance records
above remain the historical d1 outcome, not a claim that this later reported
failure is solved.

## Previous correction frontier — bpr0643-r1 / d2 / mandate r4

The owner's later UI report was accurate for a distinct signed Mac app
attempt. The running `build/app-release` window showed `MikeMarcin` connected
on GitHub.com Native HTTPS and displayed `Pull request unavailable` with
`GitHub returned data that BetterPullReview could not safely use.` Its two
read-only target attempts got through repository, pull request, file, commit,
review, thread, comment and check reads, then recorded
`failure=invalid_relationship` after `SearchPullRequests`. The page was the
last observed operation before whole-snapshot validation; it is not a proven
cause. The exact metadata-only attempts and rendered window are retained
outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-3-diagnostics.log`
(SHA-256 `b6843ce662bf54ca4782d0fa9e2d14d43831c5b7080e9be4e78efebfdeafdf3d`),
`attempt-4-diagnostics.log` (SHA-256
`572bb9175e26e4bf40922d51bed5b680d57b26cb2bf7b525b6cdc4240ffd2637`),
and `attempt-3-window.png` (SHA-256
`e3807e748cb45358f56eeefc158e19135b2157dac9f1ffbe411ebf632e824bcc`).
No file list or selected diff appeared in that window.

Read-only CLI structural checks under the owner's separate configured account
found 1,523 files, 250 returned commits, one review, 111 issue comments and
19 complete GraphQL review threads. The checked file/thread/actor/search
relationships had no mismatch; the original `invalid_relationship` cause is
still unknown, including capability-shaped observations that the CLI grant
cannot reproduce. The exact product search query matches the CLI probe:
`is:pr is:open repo:EpicGames/UnrealEngine`. That repository currently reports
2,104 open-PR search hits, exceeding the product's 1,000-result bound. This
is a separate demonstrated load coupling, not an explanation of the earlier
`invalid_relationship` terminal.

Commit `4c90aa70` makes oversized ancillary repository search explicitly
`result_limit` with zero retained search rows, after validating the bounded
received page, and keeps the exact PR snapshot eligible. Malformed rows,
contradictory pagination and other read failures still refuse. It also keeps
the `invalid_relationship` failure class while adding a fixed metadata-only
`relationship_detail` for a future reproduction; it does not relax any
relationship check. Synthetic 2,104-hit, malformed-page and exact PR
identity/file/base/head controls pass. Six focused courses, including the
rendered GitHub.com public path, pass; an independent reviewer found two
first-candidate fail-closed gaps, which were corrected and narrowly rechecked.
The documentation gate, source format check and scoped board validation pass.

The final tests-disabled `build/app-release` Mac bundle is Developer ID signed
and passes `codesign --verify --deep --strict` (binary SHA-256
`60f0025fcb007f034be0ceb2f6838a0bbf0573e4b9a9ea3cf56f084f454cfe9e`).
Its exact read-only target attempt under a connected `MikeMarcin` session
stopped earlier at `permission`/`GetRepository`; the session panel said
`This GitHub sign-in does not have permission to read this pull request.`
The two-line final log is outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-7-final-signed-diagnostics.log`
(SHA-256 `51973173839fa9281df37b5d393901fe188e8ea5bdd611896bc603ae4f274ce9`).
The rebuilt ad-hoc and an earlier signed rebuild likewise stopped at
`GetRepository`; their exact logs are preserved on the live BPR-0617 closeout.
Why the original signed app could reach snapshot validation while restarted
builds cannot read the repository is unknown. A product sign-in with authority
to read this repository is needed to exercise the repaired exact path, inspect
any new `relationship_detail`, and determine whether the original malformed
snapshot has a safe repair. The app cannot adopt the CLI credential. No live
write, credential extraction or private PR content retention occurred in d2.
The d1 acceptance records above remain historical; this correction does not
claim that the originally reported malformed-data failure is solved.

## Previous frontier — bpr0643-r1 / d1 / mandate r2

At d1, the bound `retained-stress-pr-read` milestone had a direct read-only
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
and `application_logging` passed 5/5 at the d1 source. The pager's fixture
accepts 3,000 files and 250 returned commits, but this cannot establish the
target's runtime outcome. A task-launched dev UI process was stopped after its
computer-use attachment timed out; it produced no new product diagnostic.
The d1 tests-disabled Release candidate was built at
`build/bpr0643-release/src/app/BetterPullReview.app` from that source. Its
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

For the d1 candidate's then-current device grant, the explicit known limit
was a repository-read permission refusal. A separate read-only CLI GET as
the same `MikeMarcin` account succeeds, but the product deliberately cannot
adopt CLI credentials. The evidence does not prove whether GitHub App
installation policy is the underlying permission cause, and it does not
exercise the 1,523-file stress shape past repository access. No product read
limit changed in d1. The only live actions were CLI GET and the app's read-only
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
