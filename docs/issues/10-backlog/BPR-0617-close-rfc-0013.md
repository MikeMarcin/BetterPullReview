---
schema: "implement-rfc.card.v1"
id: "BPR-0617"
title: "Verify and close RFC-BPR-0013"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0507","BPR-0616","BPR-0618","BPR-0619","BPR-0620","BPR-0621","BPR-0622","BPR-0623","BPR-0624","BPR-0626","BPR-0627","BPR-0628","BPR-0629","BPR-0630","BPR-0631","BPR-0632","BPR-0633","BPR-0634","BPR-0635","BPR-0792"]
closeout_for: ["RFC-BPR-0013"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0013

## Current frontier — bpr0643-r2 / d8 / mandate r12

The owner renewed the read-only `EpicGames/UnrealEngine#1773` objective after
RFC-0015's Mac CLI read, fixture publish, and native qualification at
`26f1c22e`. The BPR-0643 known-limit acceptance remains terminal. Its later
off-product `gh` read reached HTTP 200 for exact repository and pull-request
metadata, then the current parser rejected the reported 9,311 total commits
against its 250 returned-record limit before file paging. The fixed
metadata-only observation and SHA-256 are recorded in the previous frontier
below. The original `invalid_relationship` terminal remains a separate
historical observation.

Main reserved BPR-0792 at `c26dc022` with its counter and immutable receipt.
The separate `large-pr-returned-commit-boundary` plan amendment binds
[BPR-0792](../30-in-progress/BPR-0792-distinguish-large-pr-total-from-returned-commits.md)
without changing BPR-0643's accepted exits. Its assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r2-d8-r12.json`.
The current working candidate distinguishes a reported total from returned
records. The fixed GitHub endpoint ceiling remains 250; a tighter caller
budget cannot redefine a complete result. The pager requires the exact
bounded returned count, current head and existing page/snapshot validation;
the host projects total and returned counts separately into visible copy.
The focused pager, application overview and rendered Qt overview tests pass
3/3 in `build/dev`; the pager includes a 9,311/250 positive, short, overlong,
wrong-head and tightened-local-budget negatives. A fresh exact-target,
read-only `gh api` memory probe without credential extraction reports
repository/PR identity true, 9,311 metadata commits, 1,523 files, commit
pages 100/100/50/0 and returned tail equal to head. It retained no private
response body or OID in a file or tool log. The isolated tests-disabled
`build/bpr0792-app-release` bundle is built, strict-signature verified and
has binary SHA-256 `6eaf79ed6d43fe098ce099b2580879a218ddea216c24ef8ac099426ce8b2c6c8`.
One independent critical review found the tightened-budget error in the first
candidate and rechecked the correction. Its narrow final recheck found no
remaining blocker after the wrong-head negative and accepted-snapshot-to-host
projection assertion. The composed dev GitHub.com public-path fixture passes
1/1 in `build/bpr0792-public-path-test.log`. Project format, documentation,
64 documentation behavior tests, scoped board validation and diff checks pass.
Current operator docs now describe the reported/returned distinction without
claiming the owner-retained target renders.

The exact tests-disabled product read is still unverified. Computer-use
launch/attachment of both the isolated ad-hoc bundle and the established
Developer ID signed `build/app-release` bundle timed out before any Qt window
or read request. Each task-owned process remained in `dyld` `__open` in a
bounded sample (`build/bpr0792-app-sample.txt`,
`build/bpr0792-signed-sample.txt`); the launch cause is unknown. Both task
processes were stopped and confirmed absent. A separate `build/app-release-d28`
process belongs to another owner and was not touched. The signed current
`build/app-release` binary passes strict code-signature verification and has
SHA-256 `bf07f6fa6a0a52694334aa2a21ae5a02700a3f51814b0f1721056ba6ab922d2e`.
BPR-0792 remains In Progress with its file list and selected diff **unverified**;
no product parser terminal can yet be assigned to the private target. Next:
restore a working Mac candidate launch, then perform one exact opted-in CLI
profile read and record only fixed diagnostic metadata and whether the file
list and selected diff render. No task child or command remains active.
No token extraction, live PR write, account change, or private content in
the checkout is authorized. BPR-0617's older Windows/Linux, canonical
predecessor, and RFC-wide obligations remain open.

## Current frontier — bpr0643-r1 / d7 / mandate r11

The renewed `retained-stress-pr-read` assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d7-r11.json`.
An external, memory-only read diagnostic used the owner's existing `gh api`
authority without extracting or transferring its credential or changing the
connected app session. The current product parser received HTTP `200` for
`GetRepository` and `GetPullRequest` on the exact target, then returned
`response_limit` after two calls. GitHub reported 9,311 commits and 1,523
files in PR metadata; the product's current 250-commit ceiling rejects the
count in `parse_pull_request` before whole-snapshot validation. The
metadata-only diagnostic is outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-10-cli-parser-diagnostics.log`
(SHA-256 `433971b034682127e0151e8d19f9aab16ca50f92beee87b02e878897586536c5`).
This is a current parser limit, not a reproduction or resolution of the
historical `invalid_relationship` attempt; that different app observation
remains open. No file list or selected diff was reached, and no live PR write
or private content retention in the repository occurred.
No new token or device-grant replacement is needed to establish this current
parser limit; the optional manual-credential path remains a separate choice.

The current app's CLI profile stays typed `cli_unavailable` with zero launch.
The owner's new request for a warning-labeled production CLI account profile
supporting reads and publishing belongs to a separate RFC-BPR-0015 assignment;
its deferred upstream and containment requirements must be reconciled there.
This BPR-0643 dispatch made no product authentication or mutation change. The
optional manual-credential wizard remains paused and the existing device
grant remains connected.

## Previous frontier — bpr0643-r1 / d6 / mandate r8

The owner cannot obtain `EpicGames` administrator assistance, so the prior
organization-installation inspection is unavailable. The renewed
`retained-stress-pr-read` assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d6-r8.json`.
The product supports an optional user-controlled path in its current
GitHub.com Native HTTPS profile: **Settings → Accounts & hosts → Connect**,
then the owner enters a GitHub-authorized personal access token in the app's
password-style **GitHub credential** prompt. The credential stays in the app
session and is not written to settings or logs. Submitting it removes the
remembered `MikeMarcin` device grant, even if the replacement cannot read the
repository; canceling before submission leaves that session connected. The
CLI's separate credential cannot be adopted automatically. This product path
is supported by the public contract, activation and enrollment code, and the
focused Qt/application tests. The restart fixture's widget search was updated
for the mounted Settings window; the test then passed through its manual
replacement checks. No product source change was warranted.

The optional two-step wizard `.mask/sessions/wizard/bpr0643-manual-read` is
paused at revision 1 with no human acknowledgment or credential. It should be
resumed only if the owner elects the replacement. The subsequent agent
observation would check the actual app account and one exact read-only
`EpicGames/UnrealEngine#1773` Open, retaining only fixed diagnostic metadata
and whether the file list and selected diff render. GitHub may restrict
personal access tokens for this organization or require fine-grained-token
approval, so this route is conditional on GitHub granting the chosen token
repository read access. The current app grant's HTTP `404` and earlier
unreproduced `invalid_relationship` remain open; the historical d1 Done
acceptance is not evidence of a readable diff. No token was entered or
inspected, and no live PR write occurred.

## Previous frontier — bpr0643-r1 / d5 / mandate r7

The renewed `retained-stress-pr-read` assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d5-r7.json`.
GitHub's authenticated `@MikeMarcin` personal Installed GitHub Apps page
lists BetterPullReview, but its Configure page and the Authorized GitHub Apps
page require sudo-mode reauthentication before showing current selection or
authorization. No reauthentication or setting change was attempted. A
metadata-only CLI repository GET reports the private `EpicGames/UnrealEngine`
repository as pull-readable but `admin=false` and `maintain=false`; the CLI's
organization-installation GET returns `404` and names `admin:org` as required.
The separate CLI token cannot use GitHub App user-token installation endpoints.
Thus the exact `EpicGames` app installation, selected repositories and possible
SSO authorization remain unknown. The d4 signed app's direct HTTP `404` at
`GetRepository` establishes only the current app grant's inability to read
this target. An eligible `EpicGames` organization owner or repository admin
would need to inspect the organization's app installation and repository
selection; if correct and SSO is enforced, inspect the app's SSO authorization. The current product
session is preserved, with no token inspection or live PR write. The original
`invalid_relationship` remains pending and the d1 Done criteria are historical.

## Previous frontier — bpr0643-r1 / d4 / mandate r6

The owner's account-level GitHub.com read access makes d3's manual-token
suggestion premature; no credential switch is requested. The renewed
`retained-stress-pr-read` investigation is bound by
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d4-r6.json`.
The product's connected `MikeMarcin` device session is preserved. The
archived pre-d2 and current signed bundles contain the same public production
GitHub App client identifier. The repository's 2026-08-28 registration
observation lists only `MikeMarcin/BPR-Test` in a selected-repository
installation; it is historical, not a live installation inventory. GitHub's
documented GitHub App user-token access is the intersection of the account's
and app's access. D4 is tracing the native `GetRepository` request, auth and
failure mapping before treating installation policy or a product bug as the
cause. The existing exact-target logs remain preserved; no credential is
inspected and no PR write is authorized.
The original preserved Settings window and current one both show a renewing,
remembered device grant. A fresh read-only CLI repository GET confirms the
target is private and account-readable with the CLI's separate credential.
The app's prior terminal diagnostic lost the failed HTTP status; its
`permission` class could not distinguish GitHub `403`/`404` from Qt transport
content denial. D4 added metadata-only status/source fields before a new
exact-target read.

The final Developer ID signed d4 candidate resumed `MikeMarcin`'s device
grant and attempted only `EpicGames/UnrealEngine#1773` read-only. The new
metadata-only diagnostic records GitHub's HTTP `404` at `GetRepository`
(`transport_source=http_response`), rather than a local transport failure;
the panel attributes the refusal to the app connection and allows for
different browser access. Its exact two-line log is retained outside the
checkout as `~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-9-d4-diagnostics.log`
(SHA-256 `d32e59b0604c1147bb02af1b3e2542a7c0b10811f76200dea474c812418749fd`).
The request is the correct `GET /repos/EpicGames/UnrealEngine` with the bound
app credential. GitHub App user-token access is limited to the intersection
of user and app access, so account-level website/CLI access does not establish
this app grant's access. The exact current installation selection or policy
behind the `404` is not proven; `gh`'s separate token cannot inventory App
installations. The earlier `invalid_relationship` remains unreachable on this
grant, with no file list or selected diff. No credential was viewed or
switched, and no live PR write occurred.

## Previous frontier — bpr0643-r1 / d3 / mandate r5

The renewed `retained-stress-pr-read` correction is bound by
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d3-r5.json`;
its definition and obligation fingerprints match d2. In wizard
`bpr0643-reconnect`, the owner acknowledged the manual device reconnect at
revision 1. The agent's revision-2 observation then saw `MikeMarcin`
connected in the final Developer ID signed `build/app-release` product, but
one exact read-only `EpicGames/UnrealEngine#1773` Open again ended
`permission`/`GetRepository` (request 1/window session 16). The session panel
named the current GitHub sign-in's missing read permission. Its exact
two-line metadata-only log is outside the checkout as
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-8-reconnect-diagnostics.log`
(SHA-256 `9d41955ce9521f34e37878d9221c9fe6cf321475e851d0f1dcafcbeefac20664`).
No file list or selected diff was reached. The original later
`invalid_relationship` invariant still cannot be diagnosed from this product
grant, and neither the d1 acceptance nor the d2 bounded search change proves
it solved.

The acknowledged device reconnect did not grant this app repository read access.
The owner's next product-controlled route is to enter, directly in the app,
a manual token they can establish has access to this exact repository. That
replaces the remembered device grant with a session-only credential, so the
current connected session should be preserved until the owner chooses that
action. The product cannot silently adopt the CLI credential. A follow-up
would verify the actual product account and one exact read-only Open, inspect
only metadata-only failure/page/`relationship_detail` and whether the file
list and selected diff render, and repair only a demonstrated invariant
issue. No further source change is justified by this permission refusal.
No live PR write, credential exposure or private PR content retention
occurred in d3. The unrelated `.gitignore` and `.mask.json` edits remain
outside scope.

## Previous frontier — bpr0643-r1 / d2 / mandate r4

The owner reported a different live BPR-0643 refusal than d1's accepted
permission attempt. D2 is bound to `retained-stress-pr-read` by
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d2-r4.json`.
The running `build/app-release` Mac bundle, launched at 11:05 PDT, produced
two read-only attempts at about 18:05–18:06 UTC. Metadata-only diagnostics
show both got through repository, pull request, files, commits, reviews,
threads, issue comments and checks, then ended `invalid_relationship` after
`SearchPullRequests`; the named page is the last page before whole-snapshot
validation and is not yet proven causal. This agrees with the owner's
`GitHub returned data that BetterPullReview could not safely use` panel text.
The 11-line attempt records are retained outside the checkout as
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-3-diagnostics.log`
(SHA-256 `b6843ce662bf54ca4782d0fa9e2d14d43831c5b7080e9be4e78efebfdeafdf3d`)
and `attempt-4-diagnostics.log` beside it
(SHA-256 `572bb9175e26e4bf40922d51bed5b680d57b26cb2bf7b525b6cdc4240ffd2637`).
The actual PID 23251 window's rendered refusal was captured outside the
checkout as `attempt-3-window.png` beside those logs (SHA-256
`e3807e748cb45358f56eeefc158e19135b2157dac9f1ffbe411ebf632e824bcc`).
That observed binary was `build/app-release/src/app/BetterPullReview.app`, modified
10:39 PDT (SHA-256 `d845e2929fa7e47f68132afdc099525c922d000c9246215e1c0780ed1fd494ad`),
distinct from d1's `build/bpr0643-release` candidate (SHA-256
`91a9479d7b1b980ceaef8f1fcbca78bd051202bb0dee3adb3a6387f7575d2e86`).
The earlier d1 `permission`/`GetRepository` observations remain accurate for
their separate rebuilt candidate attempts and do not explain this later one.

The originally observed app's window and account state were captured outside
the checkout before it was closed. The rebuilt tests-disabled ad-hoc bundle
showed `MikeMarcin` but its read-only attempt again stopped at
`permission`/`GetRepository`, before validation. Its two-line log is outside
the checkout at `~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-5-ad-hoc-diagnostics.log`
(SHA-256 `e40beba3e3ddb031f94f85c2f157ad0b86c76acd234100e5769cb99bb700e969`).
The original `build/app-release` bundle was Developer ID signed; its 10:39
binary and signature were preserved as `app-release-pre-d2.app` beside the
logs. The rebuilt bundle retains that configured signing identity and passes
`codesign --verify --deep --strict`, but its new read-only attempt under
`MikeMarcin` also stopped at `permission`/`GetRepository`. That exact log is
`attempt-6-signed-rebuild-diagnostics.log` beside the others (SHA-256
`1658fef5fe85b90d8ee8f1b690bf1a0df8ab2618020785a73b25bf7e114b4a8c`).
The archived old binary did not restore a usable product window from its
evidence location and was stopped. The source or grant of the access change
is unknown. The pre-rebuild UI showed a renewing, remembered `MikeMarcin`
device grant, so session-only manual-token use is not established.

The bounded read-only CLI structural probe found 1,523 files, 250 returned
commits, one review, 111 issue comments and 19 complete GraphQL threads.
The checked file, thread, actor and search relationships showed no mismatch;
the CLI credential is separate from the product grant and cannot reproduce
the original `invalid_relationship` cause. The exact repository-wide open-PR
search returned 2,104 hits, above the product's 1,000-result cap. This is a
separate demonstrated search limit, not a proven cause of the original
terminal. Source commit `4c90aa70` bounds that ancillary search as
`result_limit`, retains no search rows, and permits the exact PR snapshot
after validating the bounded received page. Malformed pages still refuse
atomically. The same commit adds fixed metadata-only `relationship_detail`
while preserving `failure=invalid_relationship` and all existing relationship
checks. The deep worker's synthetic success and fail-closed controls and six
focused courses pass. An independent reviewer found two first-candidate
fail-closed gaps; the worker corrected both, and the reviewer rechecked the
focused pager course without a remaining critical finding. The documentation
gate, source format check and board validation pass.

The final rebuilt tests-disabled `build/app-release` candidate remains
Developer ID signed and passes strict code-signature verification (binary
SHA-256 `60f0025fcb007f034be0ceb2f6838a0bbf0573e4b9a9ea3cf56f084f454cfe9e`).
Its connected `MikeMarcin` session's exact read-only target attempt ended
earlier at `permission`/`GetRepository`, with the session-panel reason
`This GitHub sign-in does not have permission to read this pull request.`
The final two-line log is outside the checkout as
`attempt-7-final-signed-diagnostics.log` beside the other attempts (SHA-256
`51973173839fa9281df37b5d393901fe188e8ea5bdd611896bc603ae4f274ce9`).
No file list or selected diff was reached. The d1 Done acceptance remains
historical and accurate for its then-current candidate; it does not resolve
the owner's later malformed-data report. That root cause and the change in
product repository access remain unknown. The pending native follow-up is
for the owner to sign in to the product with authority to read this exact
repository, then retry the final candidate read-only, inspect any new
`relationship_detail`, and repair only a demonstrated invariant issue. No
PR mutation, credential extraction or private PR content retention occurred.
The task app and worker/reviewer work have ended. The unrelated `.gitignore`
and `.mask.json` edits remain outside scope.

## Previous frontier — bpr0643-r1 / d1 / mandate r2

The `retained-stress-pr-read` milestone is accepted through its explicit
known-limit alternative. All bound obligations and both exits pass without
claiming a file list or selected diff. BPR-0643 is Done at `8285cb4f`.
BPR-0642 is Done; [BPR-0643](../60-done/BPR-0643-load-the-retained-stress-test-pull-request.md)
records the exact criterion evidence. The r2 binding is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-bpr0643-r1-d1-r2.json`.
The owner completed wizard `.mask/sessions/wizard/bpr0643-signin`; the agent
observed `MikeMarcin` and `GitHub account connected` on the fresh tests-disabled
Mac candidate's GitHub.com Native HTTPS profile. One read-only Open attempt
for `EpicGames/UnrealEngine#1773` failed at `GetRepository` with the typed
`permission` class. The first session panel attributed the refusal to the
authenticated account. The exact two-line attempt log is
outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-1-diagnostics.log`
(SHA-256 `f3b35aa9220d238f082295fe65937b53aa2308fa216dd1239570ff08f3badea9`).
The CLI GET under the same account succeeds, but the product grant cannot
adopt that credential. This is a permission boundary before file paging, so
no file list or selected diff claim is made; whether GitHub App installation
policy caused the denial is unproved. The card records the explicit known
limit, not a size-limit repair. The narrow Qt copy change at `5d5c8b66` names
the current sign-in rather than the account. The rebuilt tests-disabled
candidate resumed `MikeMarcin`; its second read-only attempt again ended in
`permission` at `GetRepository`, while the real session panel displayed
`This GitHub sign-in does not have permission to read this pull request.`
That exact two-line attempt is outside the checkout as
`~/Library/Application Support/BetterPullReview/BPR-0643-evidence-2026-09-13/attempt-2-diagnostics.log`
(SHA-256 `a7fe87f2880f68e97d578c53997c56d9b0a93175c0654643a36d7f3d2aa50b9f`).
The focused Qt course and format check pass after the change; the earlier
five diagnostic, pager, Qt and public-path courses remain relevant. The
documentation gate, scoped board validation and diff check pass. No live
write or credential transfer occurred. The task-owned candidate app and
commands have exited. BPR-0622's Windows/Linux native evidence, BPR-0507,
earlier canonical RFC prerequisites and final BPR-0617 closeout remain open;
this exact-target dispatch is complete and does not authorize those slices.

The previous accepted `default-cache-byte-pressure` milestone remains as
recorded below. BPR-0622's native evidence and final RFC closeout remain open.
The unrelated `.gitignore` and `.mask.json` edits are outside this scope.

## Previous frontier — rfc0013-r2 / d4

### D4 owner disposition and milestone reconciliation

Dispatch `rfc0013-r2/d4`, producer `/root/rfc0013_nonrepro_close`, carries the
owner's exact amendment, "mark it as can not reproduce and close that issue,"
for BPR-0751's isolated historical 2.152s source-selection outlier. BPR-0751's
first acceptance bullet now records that event as *cannot reproduce* with its
stage, cause and pre-BPR-0635 origin unknown. The unchanged failed
`build/bpr0635-composed-final.log` (test 279, 2,151.817 ms against 2,000 ms)
remains historical failure evidence; neither a retroactive pass nor a limit
waiver is inferred. The matched 929-read pre-BPR-0635 product replay did
reproduce the 228.745–265.034-ms refresh gaps, but epoch 25 source selection
was 959.528 ms and the source maximum was below 1.018s
(`build/rfc0013-d3-pre0635-history.log`). The repaired public 929-read course
passes its unchanged refresh and source timing limits with exact heads, reads,
selection and retained storage (`build/rfc0013-d2-history-indexed-overview.log`).
This owner decision removes only the unprovable historical-cause requirement;
the measured refresh attribution, repaired product behavior, public-byte
pressure, focused checks and all four authored milestone exits remain binding.
The D3 checkpoint below retains the prior open finding and negative evidence.

The renewed D4 binding is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-rfc0013-r2-d4.json`.
The owner amendment and binding were committed at `f378da20`; the guarded
board finish moved BPR-0751 to Done at `42bd1ed1` with three criterion records
in `build/rfc0013-d4-bpr0751-acceptance.json`. The documentation gate reports
`DOCUMENTATION_OK`, all 64 documentation behavior tests pass, scoped RFC-0013
board validation is valid, and `git diff --check` passes after the move. D2
focused Qt 8/8, overview 3/3, byte and preserved public courses remain
relevant because no product source changed in D3 or D4. Both bound cards and
all four unchanged milestone exits now pass; the
`default-cache-byte-pressure` milestone is complete. The exact failed log's
SHA-256 is `f206ee5270db29b7fad03655826a564dbdefe6496d2f6e7e8f0e55e049b3ccf7`.
This does not close RFC-0013:
BPR-0622 still needs Windows/Linux native evidence, BPR-0643 waits on
BPR-0642 and the owner's read authority, and BPR-0617 waits on BPR-0507,
BPR-0622, the remaining RFC acceptance matrix, earlier canonical RFC
prerequisites and its own final closeout checks. No remote or credential
action, real pull-request mutation or push was done. Prior D3 writers and
commands were quiescent; D4 had no child writers and all task commands have
finished. Only the unrelated `.gitignore` and `.mask.json` edits preceded this
dispatch and remain outside its ownership. This substantial integrated
boundary is ready for a fresh assignment when the remaining native/owner
dependencies make another milestone eligible; there is no immediate Mac-only
RFC-0013 card to claim.

### D3 attribution checkpoint (historical)

Dispatch `rfc0013-r1/d3`, producer `/root/rfc0013_attribution`, mandate `r1`
renewed the live `default-cache-byte-pressure` binding after BPR-0635 became
Done. BPR-0751 remains In Progress. The controlled pre-BPR-0635 product tree
with the identical 929-read fixture reproduced 31 refresh maxima of
228.745–265.034 ms, while its epoch-25 source selection took 0.960s
(`build/rfc0013-d3-pre0635-history.log`). The original 897-read fixture on
that same tree failed revision preparation before timing
(`build/rfc0013-d3-pre0635-original-history.log`, diagnostic stopped after the
failure). The four repaired product paths are byte-identical across the
BPR-0635 fixture commit (`build/rfc0013-d3-attribution.json`). This establishes
that the 929-read projection pause occurs on pre-commit product code, but it
cannot supply an unmodified pre-commit 929-read timing result. The isolated
2.152s source selection has not recurred; sampled outstanding diff work
localizes the delay but does not identify its worker stage or scheduling cause.
BPR-0751 AC1 and milestone exit 4 remain open under the unchanged acceptance
wording. Independent read-only review confirmed that the refresh attribution is
supported, while the isolated source-delay cause remains an AC1 blocker. No
production source change or timing limit change was made in D3.
The disposable baseline CTest process group and its descendants are stopped;
the task-owned worktree was removed after recording the evidence. The next
discriminating step, if the historical cause is still required, is a scoped
job-stage timestamp trace in a new network-isolated history run, with the
understanding that a nonrepeating event cannot be assigned a deeper cause
retrospectively. Existing D2 passing public, byte and focused evidence is
unchanged. D3 documentation, scoped board validation and diff checks pass
(`build/rfc0013-d3-documentation.log`,
`build/rfc0013-d3-board-validation.json`). The renewed assignment is
`docs/rfcs/plans/rfc-bpr-0013-hardening/assignment-rfc0013-r1-d3.json`.

### D2 handoff (historical)

Checkpoint after `b1743179`: BPR-0635 is Done on its own Mac byte-pressure
criteria; BPR-0751 remains In Progress, so the bound
`default-cache-byte-pressure` milestone and RFC-0013 are not complete. The
public projection repair, tests and evidence are committed at `4a2be1b1`.
The final network-isolated history passes with refresh maxima 64.617–91.147 ms
under the unchanged 100-ms limit, 32 exact heads/Git/catalog pairs, 929 reads,
31 supersession selections, zero model resets and source under 0.952s
(`build/rfc0013-d2-history-indexed-overview.log`). The repeated byte course
passes with 70,353,760 admitted bytes, default-byte eviction and exact
stale-safe revisit, 147.750-MiB sampled after-drain growth under 160 MiB and
40.313-MiB destruction growth under 96 MiB
(`build/rfc0013-d2-byte-post-projection.log`). Entry, loaded-close and
resource preservation pass 3/3 (`build/rfc0013-d2-preserved-public-courses.log`);
focused Qt 8/8, overview 3/3, documentation/behavior, format, scoped tidy,
diff, board and plan checks pass. BPR-0751's remaining AC1 asks for a matched
pre-BPR-0635 numerical baseline and the cause of one historical 2.152s source
selection. The old fixture failed before timing and the older BPR-0632 course
used 897 rather than 929 reads; source identity proves the product path
predated the fixture commit but not the numerical pause. The outlier has not
recurred and its deeper scheduling cause remains unproven. Independent review
calls this an acceptance blocker, preserved on BPR-0751 rather than waived.
The committed product paths are the Qt conversation cards/window, pure
application overview projection and focused tests; fixture and transport
changes remain in the earlier BPR-0635 commit. All D2 child owners and test
commands are quiescent. Outside this closeout, the worktree has only unrelated
pre-existing `.gitignore` and `.mask.json` changes, which are not owned here.
The D2 return retains its exact dispatch binding; the mechanical continuation
check requests a renewed assignment because card evidence and BPR-0635's board
status changed after that binding. A successor must recompute the live binding
before further milestone work, while preserving both obligations and all four
exits.
Next bounded work is to resolve that exact historical evidence obligation or
amend it with the obligation explicitly retained under owner authority; only
then re-evaluate the bound milestone. No remote or credential work was done.

### D2 diagnostic sequence (historical checkpoints)

The owner-authorized RFC-0013 continuation remains on local `main` at `c202d11c` with no production-path change from BPR-0635's `1681994a` fixture/course commit. BPR-0635 is In Progress: its exact 20-key / 70,353,760-admitted-byte Mac public course passed and the existing 32-epoch preservation course did not. The serial failure in `build/bpr0635-composed-final.log` reached 32 exact heads, 929 account reads, source and retained Git/catalog checks, but 31 refresh timer maxima were 237.735–254.774 ms against 100 ms; source selection 25 took 2,151.817 ms against 2,000 ms. BPR-0751 is now In Progress for the independent product projection repair. The 25-second main-thread sample at `build/bpr0635-post-lock-sample.txt` places work in accepted PR handoff, synchronous workbench publication, and repeated Qt conversation card layouts over 1,000 threads. That is a measured attribution candidate, not yet a complete cause or accepted repair.

The authored plan now binds BPR-0751 alongside the still-open BPR-0635 and preserves all three original exits, adding a history-projection exit. `assignment-rfc0013-r1-d2.json` is the renewed binding; d1 is historical. The mechanical `check-amendment` rejected the proposal because a newly discovered card adds an obligation. This is an explicit within-RFC requirement reconciliation under the owner's finish-RFC-0013 mandate, not a helper-approved preservation-only refinement. Plan, generated view, card link and renewed assignment were committed at `989310a7`; Ready promotion at `94e9b1b0` and claim at `c202d11c`. The documentation gate passed after binding. The standard Qt worker's first six-file candidate skips equal card snapshots, adopts anchor and forest together, and refreshes changed geometry without rebuilding measured content; focused Qt/composed tests pass 8/8 in `build/rfc0013-d2-projection-tests.log`, with checkpoint `build/rfc0013-d2-worker-projection.txt`. Independent read-only review found no demonstrated blocker, but notes that geometry refresh still traverses all retained cards and must not be sold as RFC-wide active-set scaling. The first uninstrumented 32-epoch rerun (`build/rfc0013-d2-history-candidate.log`) preserved 32 exact heads and pairs, 929 reads, and all 31 selections. It reduced all refresh gaps to 156.803–163.321 ms and the worst source selection to 1,036.656 ms, but still fails the unchanged 100-ms refresh gate on all 31 refreshes. A second diagnostic run was sampled for 30 seconds at `build/rfc0013-d2-post-qt-sample.txt`, then its entire process group was deliberately stopped and confirmed quiescent; it is not timing evidence. The sample still places about 16 main-thread samples in card relayout/measurement, seven in thread-map rebuild and five in model apply after an anchor-only clear/reselect. The same worker now owns a bounded follow-up to reuse retained conversation/card measurements and avoid needless reprojecting on those anchor changes; a read-only scout and reviewer are quiescent. The pre-repair same-course metrics are extracted at `build/rfc0013-d2-history-baseline.json`. Next: rerun exact history after the correction, preserve source/selection checks, and adjudicate both cards without relaxing limits. The BPR-0622 Windows/Linux, BPR-0643 owner-account, earlier human/live and broader RFC rows remain open. No remote or credential action is authorized.

The second seven-file Qt candidate built and its eight focused Qt/composed tests passed (`build/rfc0013-d2-projection-build.log`, `build/rfc0013-d2-projection-tests.log`, `build/rfc0013-d2-worker-projection.txt`). The exact uninstrumented 32-epoch course still failed: all 31 refresh gaps were 150.595–160.849 ms, while 32 heads/pairs, 929 reads and 31 selections passed and source selections stayed under 1.008s (`build/rfc0013-d2-history-second.log`). The correction reviewer found no demonstrated blocker, but requested a fresh-view geometry comparison because cached-refresh-versus-cached-relayout tests can share a defect. A third 30-second sample `build/rfc0013-d2-post-cache-sample.txt` observed real text shaping on the GUI thread. The same Qt worker's third candidate validates ordered post bodies, excerpt, header/action state and presentation parameters per root, prunes removed roots, and adds fresh-view geometry comparisons plus removed-root reappearance. Its four-target build, 8/8 focused tests, eviction regression, formatting and diff pass (`build/rfc0013-d2-projection-build.log`, `build/rfc0013-d2-projection-tests.log`, `build/rfc0013-d2-projection-eviction-test.log`); independent re-review found no blocker. The exact third uninstrumented 32-epoch course still failed at 152.506–159.974 ms on all 31 refreshes, with all 32 heads/pairs, 929 reads, 31 selections and source under 1.009s (`build/rfc0013-d2-history-third.log`). A fourth 30-second sample `build/rfc0013-d2-post-per-card-sample.txt` still observes real card text shaping on the GUI handoff. All diagnostic CTest process groups were deliberately stopped and confirmed quiescent. The Qt worker now owns temporary test-only aggregate cache-miss diagnostics; its result will distinguish clear, changed content/ID and global-presentation invalidation before another product edit. No other source writer or test command is active. The sample also shows a smaller synchronous ledger-opening branch; it remains an RFC structural concern, not an accepted explanation for the current 150-ms gap.

The bounded test-only card trace (`build/rfc0013-d2-card-diagnostic-command.log`) found the concrete projection churn: with 2,000 retained conversation nodes, each refresh temporarily widened the visible card scope from two file cards to 1,000 review-wide cards, then narrowed it. The widened adoption missed 998 measurements and took about 32 ms; a subsequent width change invalidated all measurements before the next refresh. A Qt visual-anchor guard retained the file scope through a newer-generation workbench observation gap while leaving the semantic cursor cleared; focused Qt tests covered same-generation explicit clear, empty successful terminal, failure and authority clear, and independent review found no blocker. The full uninstrumented course nevertheless still failed all 31 refresh intervals at 152.274–159.341 ms (`build/rfc0013-d2-history-guarded.log`), with all 32 exact heads/storage pairs, 929 reads, 31 selections and source selections under 1.011s. The guard therefore is not yet accepted as the public-path repair. A narrower metadata-only trace of the actual workbench publication/anchor sequence is pending; no timing budget is relaxed and both BPR-0751 and BPR-0635 remain In Progress.

That actual-sequence trace (`build/rfc0013-d2-guard-diagnostic-command.log`) showed the initial gap guard worked, but the successful workbench observation first published 515 files with an engaged cursor lacking a selected file. The guard released there, widened the cards from two to 1,000, remeasured 998, then the following local selection narrowed them. A second Qt correction holds the visual file through that terminal only when the exact file remains in the observed list, with tests for removed/empty results and authority release. Its focused build/tests/format pass and independent re-review found no blocker. Yet the uninstrumented 32-epoch course still fails all refresh intervals at 169.394–178.810 ms (`build/rfc0013-d2-history-terminal-guard.log`); 32 heads/pairs, 929 reads, 31 selections and source under 1.003s remain exact. A new 25-second sample (`build/rfc0013-d2-post-terminal-sample.txt`) still reaches card text measurement from the workbench terminal and also observes whole-PR overview publication and SQLite receipt reads on the GUI lane. This sample does not isolate the worst callback. A further bounded predicate/card-scope trace is underway. No Qt product candidate is committed or accepted while the history gate fails.

The final predicate trace (`build/rfc0013-d2-terminal-diagnostic-command.log`) confirms the exact file is present in the 515-file observation and the corrected guard keeps two cards with no fresh text measurement through its terminal. The remaining repeated work is in whole-PR overview publication: a bounded main-thread trace (`build/rfc0013-d2-overview-identity-command.log`) measured every one of 25 post-initial overview projections equal, yet each republished 515 file rows at about 38–45 ms, including 6–10 ms of receipt reads, about 8 ms of pure projection and about 23 ms in Qt. The Qt window now skips equal overview page/tree rebuilds while restoring file facts after workbench rows clear and resetting the equality on authority clear. The pure application overview uses a per-call path index for thread facts rather than a linear search. Focused Qt 8/8 and overview 3/3 tests, builds and formatting pass; independent review found no demonstrated blocker. The first uninstrumented combined history run narrowed all 31 refresh gaps to 95.259–101.638 ms, but six remained over 100 (`build/rfc0013-d2-history-overview-guard.log`). After the pure path index, the final network-isolated 32-epoch history course passes 1/1 in 307.47s (`build/rfc0013-d2-history-indexed-overview.log`): refresh maxima 64.617–91.147 ms, all 32 heads and Git/catalog pairs, 929 account reads, 31 supersession selections (30 during work), zero model resets, and source selections under 0.952s. No limit changed. The byte-pressure course also passes again in 333.17s (`build/rfc0013-d2-byte-post-projection.log`): 20 keys admit 70,353,760 bytes, first weak diff/index owners expire before 64 entries, B remains exact under stale A, revisited A rematerializes, all 32 pairs persist and shutdown/drain/destruction release owners. Sampled post-drain physical-footprint growth is 147.750 MiB under 160, destruction growth 40.313 MiB under 96. Earlier entry/loaded-close/resource preservation is being rerun after the Qt changes. This does not close RFC-0013 or BPR-0751: independent acceptance review found BPR-0751's historical AC1 still lacks a comparable pre-BPR-0635 929-read baseline, and the isolated 2.152s source-selection scheduling cause remains unproven. Product handoff/Qt source files were unchanged across the fixture commit, establishing that the costly code path predates it, but not that the observed numeric pauses did. The older BPR-0632 897-read course cannot answer that exact comparison. Preserve this obligation rather than recording it as passed.

## Current frontier — rfc0013-r1 / d1

The adopted [RFC-0013 milestone plan](../../rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json) binds `default-cache-byte-pressure` to BPR-0635 on local Mac, with the exact assignment and legacy-plan preparation receipt beside the plan. BPR-0612 and BPR-0634 are Done, so BPR-0635 is dependency-ready; its next public course must exceed the unchanged 64-MiB byte ceiling before 64 entries, prove exact eviction/revisit and owner release, then record native timing, input, memory, bounded work and close evidence. The current candidate starts at `21e7ae34`; no BPR-0635 source outcome is yet accepted. Preserve BPR-0622's Windows/Linux recovery gate, BPR-0643's BPR-0642 and owner-account dependencies, all remaining rate/offline/fuzz/diagnostic and live/human rows, and the canonical Proposed predecessors. No remote mutation, production account action, Actions, push or publication is part of this Mac course.

The first BPR-0635 composed attempt is a diagnostic failure, not byte-pressure evidence. The unchanged history course fails epoch revision preparation before selecting a large key, and unchanged loaded-close reaches 29 valid account reads but records replay `unexpected_operation` (code 9) on a subsequent request. The task logs under `build/bpr0635-` retain the exact attempt. The current bounded correction owns only the synthetic large-review replay and its direct/composed tests; the card stays In Progress until its original public byte and cleanup criteria pass. No production behavior change is inferred from this fixture failure.

The corrected 30-operation synthetic replay passes the original entry-pressure, loaded-close, resource and direct replay courses. Independent review found a historical fixture fetch-authority bypass and a weak stale-selection assertion; both are corrected in the current candidate. The historical fixture now rejects substituted or additional disconnected wants at the acquired transport's actual `fetch()` boundary with zero consumer delivery, before positive installed-Git fetch. The B-held public supersession observes source changes until all work drains, verifies exact B source/key, then revisits exact A. The final corrected byte course (`build/bpr0635-composed-final.log`, test 275) passes in 359.25s: 70,353,760 cumulative admitted bytes at selection 20, first weak diff/index owners expired, peak 20 entries / 66,839,408 accounted bytes below the unchanged 64-MiB ceiling, eight B-held source observations, exact A revisit, 32 retained Git/catalog pairs, zero drained cache/jobs, and diff/index/window/documents released on destruction. Sampled physical-footprint growth is 151.938 MiB after drain and 40.438 MiB after destruction; byte-only 160-MiB drain and unchanged 96-MiB destruction limits pass. The critical review's fixture authentication race is corrected with a typed post-relock refusal before script publication/fallback, and the deterministic actual-transport S1→S2 negative passes (`build/bpr0635-session-race-direct-final.log`); no S1 read consumes the rewound S2 replay, which remains readable under S2. The earlier corrected 32-epoch history regression reached all 32 exact heads, 929 account reads and exact retained storage, but failed its pre-existing 100-ms Mac UI-interval budget: refresh pulses reached about 247 ms. Attribution (`build/bpr0635-history-timing-sample.txt`) showed the GUI readiness timer waiting on the fixture state mutex while the worker compiled the next 1,000-thread historical script under that lock; malformed-fetch probes took just 0.025–0.064 ms off the GUI thread. The new fixture compiles only the next script outside that lock with cursor and authentication checks before publication. The fresh five-course serial run passes byte pressure, entry pressure, loaded close and resources, but history still fails its unchanged 100-ms Mac UI-interval budget (`build/bpr0635-composed-final.log`, test 279): all 32 exact heads, 929 account reads, source and storage checks pass, yet each refresh has a 237–255-ms GUI timer gap. One source selection took 2.152s against its existing 2s limit. Out-of-lock compilation alone did not remove the stall. The current-source 25s/5ms sample (`build/bpr0635-post-lock-sample.txt`) places the remaining main-thread work in accepted pull-request handoff, workbench publication and Qt conversation projection, while fixture compilation runs on a worker. BPR-0751 now tracks the baseline comparison and wider product-path repair; the independent reviewer found BPR-0635 and this bound milestone cannot pass while history fails. Neither timing limit is waived. The interrupted/partial serial run `build/bpr0635-history-lock-final.log` remains diagnostic only. BPR-0635 stays In Progress until history, source integration, documentation and acceptance complete. Earlier diagnostic and observational logs remain under `build/bpr0635-`; none indicates production data loss.

The byte fixture/course and its CTest registration are checkpointed at
`1681994afab3ad9075616ce641dad49544a25d1c`, with BPR-0635 still In Progress.
BPR-0751 is Backlog pending an authored-plan amendment and baseline/product
diagnosis. The documentation gate and 64 documentation behavior tests,
network-isolation manifest, project formatting check and scoped diff check pass
(`build/bpr0635-documentation-final.log`,
`build/bpr0635-documentation-tests-final.log`,
`build/bpr0635-isolation-final.log`, `build/bpr0635-format-final.log`). None
waives the failing history regression or closes the bound milestone.

RFC base: `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`.

This is the first parent of `e61b156d5d11add32452c669550c47adaaddeac3`,
the earliest historical RFC-0013 crash slice recorded by BPR-0215. Existing
Done BPR-0215, BPR-0217 and BPR-0218 retain shutdown/liveness and temporary-state
fixes; later hygiene cards are not proof of the entire resilience RFC.

Requested set: remaining nondeferred RFC pack through RFC-BPR-0014, with
RFC-BPR-0008 the current closeout frontier and RFC-BPR-0013 before 0014.
The canonical index permits available-target implementation while earlier
live checks remain open. Closure still waits for BPR-0507 and every remaining
criterion below. Advanced logging remains deferred. Owner steering now puts the
full Glaze migration (BPR-0612) ahead of further performance work; completed
cache and lifetime coverage remains regression evidence, not a final parser baseline.

Planned cards: BPR-0616 (review-publication kill recovery, Done), BPR-0618
(Git-fetch/epoch kill recovery, Done), and BPR-0619 (live SQLite journal
transaction kill recovery, Done), BPR-0620 (agent parent-death cleanup, Done),
BPR-0621 (composed AI-session state recovery, Done), and BPR-0622 (durable
workspace reclamation, open). Allocate later slices after inspecting retained product coverage; do not introduce a parallel
performance subsystem or fabricate timing budgets before measurement.

## Closeout checklist

| Criterion | Check / remaining work | Result and commit |
|---|---|---|
| Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content. | BPR-0623/0624 supply the 515-file, 20,000-line, binary/generated and 1,000-comment workload. BPR-0631 independently checks all 31 stored head deltas; BPR-0632 drives all 32 application epochs, exact source and retained refs/catalog through public refresh/history/selection. | Fixture and Mac composed coverage pass in `75d1494e` (320.25s history replay), with 897 exact reads and 32 retained epochs. Broader latency/platform criteria remain separate. |
| UI input, scrolling, and navigation remain responsive while background work runs. | BPR-0627 corrects selected-file scope, BPR-0628 yields native measurement, and BPR-0629 moves semantic-index preparation off the GUI publication turn. BPR-0632 fixes pending single-anchor supersession and measures 31 rapid epoch changes, 30 during active work, with exact final source. | Partial passing Mac evidence: ordinary large-file maximum input interval 68.662 ms; final many-epoch maximum 71.247 ms and supersession 10.848 ms in `75d1494e`. One prior 112.448-ms source-selection sample missed the provisional 100-ms target and did not repeat unchanged; retain the outlier. Broader/native-platform acceptance remains open. |
| Memory and cache usage remain within documented budgets and recover after closing/pruning reviews. | BPR-0630 proves prune/delete cleanup, BPR-0632 bounds many-epoch caching, BPR-0633 proves independent loaded-window destruction, and BPR-0634 forces default 64-entry eviction/revisit plus cleanup through public selection. Default byte pressure remains required in BPR-0635, along with other-platform budgets. | Partial passing evidence through `470adea7`; entry peak 64 / 212,992 bytes, evicted raw/index release and exact revisit pass. Loaded-close footprint/reclamation and provisional 160/96-MiB budgets remain documented. No RFC-wide passing result claimed. |
| Primary and secondary rate-limit simulations produce bounded backoff and useful UI state. | Reuse and assess provider/host rate-limit fixtures and their visible projection; add uncovered backoff cases. | Open; no RFC-wide passing result claimed. |
| Offline mode uses clearly labeled cached data and never implies freshness. | Inspect selected cached-data behavior and exercise explicit stale/offline presentation. | Open; no RFC-wide passing result claimed. |
| Process-kill tests recover Git cache and SQLite state without corruption or duplicate mutations. | BPR-0616: `live_review_submission_provider` and `live_review_submission_restart_recovery` passed 2/2, including five SIGKILL courses, SQLite integrity and zero duplicate POSTs. BPR-0618: all four real fetch/spool/prepared/visible kill courses pass, with repeated restart, exact epoch/ref identity, Git fsck, SQLite integrity and owned-spool cleanup. BPR-0619: all six receiving/spool/install transaction kills pass, including prior-history identity and two fresh restarts; unowned artifacts are preserved and new incoming admission is bounded. BPR-0620 adds forced owner PID/process-group death and fresh-launch evidence (`bdcd3837`); BPR-0621 exercises the real application during an AI-session kill, independent Git/SQLite reads, two no-replay reopens and explicit session #2 (`430b7c51`, 43.68s). BPR-0622 now integrates exact Unix workspace reclamation and guarded Git materialization (`0427dc76`), including live-owner/substitution negatives and preserved Git/SQLite identity. | Partial passing evidence in BPR-0616 (`72022b52`) BPR-0618 (`081fc9d1`, `21255d9c`) BPR-0619 (`62c62c6d`) and BPR-0621 (`430b7c51`); Mac state/workspace recovery lanes pass, including BPR-0622 (`0427dc76`); non-Unix integration and other target-native evidence remain open. |
| Fuzzers cover diff parsing/mapping, Markdown, settings/themes, API normalization, Git output parsing, and AI structured output. | Inventory custom parser entry points and add bounded fuzz targets/corpora for uncovered surfaces. | Open; no RFC-wide passing result claimed. |
| Logging and diagnostics remain metadata-only under large/error conditions. | Reuse basic Quill metadata-only and bounded-backpressure evidence (BPR-0602, BPR-0606); assess pathological workloads. | Open; no RFC-wide passing result claimed. |
| Architecture benchmarks and instrumentation prove active-set/changed-set scaling, serialized decision application, incremental Qt updates, deterministic cancellation, and stale-result rejection. | Reuse diff scale / host identity / incremental Qt evidence where inputs are unchanged; measure uncovered public paths. | Open; no RFC-wide passing result claimed. |

## Continuation

BPR-0616, BPR-0618, BPR-0619, BPR-0620 and BPR-0621 are complete. BPR-0622
remains In Progress with its Unix production integration committed in
`0427dc76`. The application now retains durable exact ownership through Git
materialization and agent execution and recovers it on ordinary startup through
the effect lane. Unknown/substituted ownership is preserved with a visible
repair/refusal. The SQLite session catalog format is unchanged.

Its final materialization crash/cancellation course passes in 45.66s and the
running-agent application crash course passes in 43.41s. Both independently
check exact Git/SQLite state; the latter adds two no-replay reopens and explicit
session #2. Restart excludes a paused live supervisor, then reclaims its
workspace after child teardown. This supersedes BPR-0621's preserved-orphan
observation where durable ownership exists. Original preparation/cleanup
journal crash evidence remains in BPR-0622.

Independent review caught unsupervised materialization Git in the first wiring;
all workspace Git commands now retain a guard through the bundled supervisor.
The new test also exposed Darwin reporting CLD_STOPPED despite WEXITED. The
supervisor now distinguishes stopped children from actual exit events. Final
launcher, parent-death and materialization checks pass 3/3 in 142.86s. The normal
composition/cancellation/cleanup/diagnostic lanes, Git contract, native journal,
host, architecture, isolation manifest, dev/tests-off builds and documentation
checks pass as recorded with precise revision relevance on BPR-0622.

Next implement the remaining non-Unix journal/lifetime integration and obtain
required target-native evidence. No Windows or Linux passing result is inferred
from the Mac. BPR-0622 and this closeout stay open. Separately inventory retained
public-path scale, memory, backoff/offline and parser-fuzz coverage before
allocating missing bounded slices; reuse unchanged direct evidence from earlier
RFCs. No GitHub Actions, workflow dispatch, push, paid CI or live mutation is
part of this work. Production sign-in is preserved.


BPR-0623 completes the deterministic large-review workload fixture boundary
in `ee1717c2`. The existing 2,000-line workbench and paged epoch model evidence is
retained, but the two/five-file served PRs do not cover the RFC-wide workload.
This slice supplies independently verified inputs; a following application
benchmark slice must establish selected-path loading, interaction measurements
and justified platform budgets. BPR-0622's remaining Windows/Linux work is now
explicitly platform-filtered and remains open with its native handoff.


BPR-0624's composed latest-head course now passes in `e9da7abc`, including all
29 paged account requests, 515 files, 1,000 exact source threads/comments,
installed-Git blob identity, both 20,000-line displayed documents, full-window
render, change actions, idle wheel invariants and rapid-selection precedence.
BPR-0625's core fix `10520a43` extends the unchanged matrix ceiling with an exact
ordered unique-line route, separately bounded by actual index/comparison work.
Independent review's undercount finding is fixed; small reference equivalence,
normalization, refusal and exact/exhausted limits pass in dev and core-only.

BPR-0624 retains hardware and measured phase details. On this M4 Pro/48 GiB,
macOS 26.6.2 arm64, Debug Qt 6.10.3 offscreen/Fusion Mac, the composed course
passes in 27.30s. Maximum timer intervals of 387.48 ms during large selection
and 201.09 ms during rapid selection are material remaining work, not passing
responsiveness budgets. Final diff cache is two entries/3,517,512 bytes, which
is not a process-memory or pruning proof. Only the latest application epoch
is exercised. Core-only diff/architecture checks pass 4/4; dev architecture,
fixture, isolation, retained single-page application and composition checks
pass 6/6. Documentation and diff checks pass after recording the completed core
dependency. Source changes are local; no paid/remote CI was used.

BPR-0626 now presents typed unavailable/binary comparisons truthfully in
`945043fd`, retaining selectable wording and genuine equal-text counts. The
real composed compute-limit/binary/supported-recovery course passes (15.72s),
with its full-window render inspected. Qt, default large-review, isolation and
composition checks pass 5/5 in 98.80s; documentation/diff checks pass. Production
limits and remembered sign-in are unchanged.

BPR-0627 integrates the selected-file scope correction in
`d698b338`: clearing the old diff no longer repopulates the entire conversation
tree while the next file loads. The composed 32-selection course now caps the
transient root set at two relevant threads; its maximum timer interval falls
from 201.09 ms to 44.34 ms and elapsed burst from 12,650.85 ms to 8,262.84 ms.
Qt workbench, composed large-review and unavailable/recovery checks pass 3/3 in
42.16s; full-window render, documentation and diff checks pass. Initial large-file
selection still observes a 338.75 ms maximum interval, so no responsiveness
closeout or universal budget is claimed.

Next Mac work: attribute that remaining large-file pause, starting with measured
pane geometry/publication work identified in the retained profile observations,
and establish justified local targets through BPR-0628 and BPR-0629. Keep RFC-0013 open for remaining memory/pruning,
backoff/offline, fuzz/diagnostic, active-work scrolling and target-native
criteria. Preserve the Windows/Linux BPR-0622 handoff and earlier live gates.


Phase instrumentation now attributes that remaining initial pause: native pane
measurement takes 164.10/163.25 ms; semantic index preparation takes 111.14 ms;
text installation 62.12 ms; axis build/adoption 1.20/0.43 ms. These diagnostic
numbers split the remaining work into BPR-0628 (cooperative native measurement)
and BPR-0629 (index preparation outside the UI publication turn). The temporary
instrumentation is removed. BPR-0627's file-switch correction and diagnosis are
complete; the RFC responsiveness criterion remains open on these new cards and
broader active-work/platform coverage. The provisional target for rapid file
switch input on this recorded Mac is a maximum 100 ms event-loop interval,
roughly twice the corrected 44.34 ms observation; it is not a portable CI limit.


BPR-0628 completes cooperative native measurement in `49aefc1cf2c196168c280d0d7d3d9176251a72e9`
(helper `52a33b68`). Final focused axis/workbench/window/large-review checks pass
6/6 in 31.54s; the full-window raster is inspected. On the same recorded Mac,
94 measurement continuations admit 93 input callbacks; gaps between these
callbacks are maximum 5.356 ms / p95 4.488 ms. Each continuation has a hard
512-line cap and checks a 4 ms target between groups. Exact native geometry,
selection, cached scrolling, supersession and layout cancellation remain intact.
No incomplete axis can be adopted, and stale geometry input is suspended while
retaining the remeasurement anchor. No remote CI or live account was used.

The whole initial selection still records a 297.119 ms maximum interval
(2,329.20 ms total), distinct from the measured batching interval. Next Mac work
is BPR-0629: the existing host already owns the effect job, while both its finish
path and Qt publication repeat full-result validation/index preparation on the
UI decision lane. Bind a prepared immutable index to the exact accepted result,
include its retained allocation in cache accounting, and keep cancellation and
late-completion checks. Do not bypass validation or move Qt work to a worker.
Native text installation remains a separate measured limit. Rapid selection
remains below the provisional 100 ms local target (50.437 ms maximum here),
which is not a universal responsiveness claim. RFC-0013 remains open.


BPR-0629 completes prepared semantic-index publication in `fb55c7976715abb54dfda3ac65a551b1bbe05fa1`.
The existing effect job performs full validation/index construction once, then
host/Qt accept only the exact immutable index/result pair for the current
request. Cache hits reuse that index and its retained vector/object bytes fit
within unchanged cache ceilings. Missing/mismatched/stale Qt publication,
cache-budget refusal, reordered/cancelled/shutdown terminals and core/Qt
boundaries pass. Focused dev core/host checks pass 2/2 (0.95s), selected
Qt/application lanes 4/4 (41.75s), fixture/boundary/composition checks 5/5
(144.37s), and core-only semantic/boundary checks 3/3 (93.55s). The rendered
public source/navigation/scroll/selection course is preserved.

On the same recorded Mac, clean large-file maximum/p95 UI interval falls from
297.119 ms to 65.308/5.786 ms, and rapid selection is 28.196/5.679 ms maximum/p95.
A separate temporary timer attributes 64.730 ms to native text conversion and
installation; its code is removed and the clean benchmark rebuilt. The
provisional 100 ms same-Mac observation target covers that remaining measured
cost with roughly 50% headroom, without becoming a universal/native-platform
claim. The final two-result cache now accounts for both prepared indexes at
3,521,008 bytes. Native measurement still yields: 82 batches, 81 input
callbacks, maximum/p95 gap 5.236/4.209 ms.

Next inventory and exercise the remaining RFC-0013 memory/pruning, many-epoch,
backoff/offline, fuzz/diagnostic and broader active-work criteria, reusing earlier
relevant checks. The BPR-0622 Windows/Linux handoff and earlier live gates remain
open. All work remains local to this Mac; no remote CI or live account was used.


The next available Mac slice is BPR-0630, a direct large-review resource course.
Existing diff host ceilings and on-disk maintenance evidence remain relevant,
but neither shows native memory or immutable diff/index release after public
prune/delete/close. Measure those phases through the real composition, correct
demonstrated stale ownership, and establish local evidence-based thresholds.
Keep broader cache pressure, many-epoch and other native-platform claims open.


BPR-0630 completes the local maintenance resource slice in `316390ce13411ff1dfbae1ba3896256a62629a78`.
The new public course exposed retained stale source/diff/index after successful
prune. Accepted exact-epoch invalidation now releases those objects and their
cache while preserving other epoch selections and late-completion rejection.
Public prune/delete/window-close/destruction, exact Git/catalog consequences
and the before/pruned window renders pass. Final resource course is 14.32s;
large-selection footprint 86.69 MiB falls to 61.09 MiB after prune with zero
derived cache/jobs. The measured Mac-only sampled-growth/cleanup budgets are
160/96 MiB above process baseline; allocator/Qt retention is explicitly distinct
from object lifetime. Core/host/Qt/boundaries/isolation, original/unavailable
public courses, composition and documentation checks pass as recorded on the
card. This does not establish unsampled peaks, independent loaded-close memory,
cache-pressure or other platform budgets.

Next inventory existing evidence for many-epoch application replay, independent
loaded-close/cache-pressure, backoff/offline, fuzz/diagnostic and active-work
criteria; select the narrowest uncovered Mac slice. BPR-0622's Windows/Linux
work and earlier live gates remain open. No Actions, remote CI, push, paid work
or production account operation is authorized by this continuation.


The next many-epoch slices are BPR-0631 (compact independently checked per-head
input deltas and exact Glaze replay) and BPR-0632 (32 real composed refreshes,
historical/current selection, cancellation and measured Mac evidence). Existing
32 Git objects do not establish 32 application epochs. Preserve the original
latest-head workload and treat input/compiler evidence separately from the
required composed benchmark.


BPR-0631 completes exact earlier-head input/replay in `d0047177aa0305666b7ee811d445f3ea0b14621f`. A compact
14,569-byte sidecar is independently checked against every stored Git transition;
all 32 snapshots pass the production pager with complete 515-file/1,000-thread
counts and exact PR/commit/GraphQL/check/file identities. Original stored inputs
and the default latest-head course are preserved. Fixture, replay and latest-head
application checks pass (7.43s / 5.84s / 22.17s); negatives reject substituted or
missing epoch data. BPR-0632 is now dependency-ready for actual 32-epoch composed
refresh/selection/cancellation and measured evidence. Do not promote fixture-only
coverage to the whole many-epoch acceptance claim.


BPR-0632 completes the bounded 32-epoch application course in `75d1494e9b993b19ce2822fcdb7cc4d5023b837b`.
It exposes and fixes a real one-sided selection defect: while resolution was
pending, enabled selectors lost subsequent choices because the accepted pair
had been cleared. The core now uses the pending explicit pair, cancels the old
request and rejects stale results. The isolated test fails before the fix and
passes afterward; catalog/authority boundaries and existing host tests pass.
The fixture inspection getter also now uses the actual hexadecimal retained-ref
ordinal encoding beyond epoch 9; stored Git format is unchanged.

All 32 refreshed heads and retained catalog/ref pairs match independent stored
input. The final 31-choice public supersession course includes 30 changes during
active work, displays exact first/latest/previous-upload source and drains
shutdown. Both full windows are inspected. Final history replay passes 320.25s;
refresh/source UI maxima are 71.247/52.518 ms, and rapid epoch supersession is
10.848 ms maximum. The unchanged 100-ms target had one earlier 112.448-ms sample
at source selection 20; its cause is unproven and it did not recur on the single
unchanged repeat. Preserve this evidence and provisional status. Other four
focused application lanes, core/host, core-only/boundaries, network isolation,
builds and documentation checks pass as recorded on the card. No remote CI or
production account was used.

The next uncovered Mac slice is independent loaded-close memory: BPR-0630's
close occurred after pruning/deleting, and BPR-0632 does not sample loaded-close
memory. Reuse its native sampling and weak ownership observations in a fresh
public-window close course before claiming recovery for a still-loaded review.
Then address remaining cache pressure, backoff/offline, fuzz/diagnostic and
broader active-work criteria. BPR-0622's Windows/Linux work and earlier live
human gates remain open. No Actions, workflow dispatch, push or paid CI.

BPR-0633 owns the next independent loaded-review close course, preserving the
existing prune/delete resource lane and its measured provisional Mac budgets.


BPR-0633 completes independent loaded-window close evidence in `5590d3ff953bb3be29cf1445270048b201f15893`.
The large raw diff/index and 20,001-block native source documents remain live
when public close starts. Shutdown drains cache/jobs in 2.703 ms; subsequent
ordered application destruction releases the weak diff/index and watched
window/documents. Exact Git/catalog base/head identities survive with zero
prune/delete dispatches. A fixture-only retained comparison allows the existing
inspector to read after shutdown clears its live authority; product lifecycle
and storage policy are unchanged.

On the recorded Mac, sampled physical footprint is 92.095 MiB before close,
91.391 after drain and 55.595 after destruction (baseline 4.516). RSS retention
is reported separately; unchanged provisional 160/96-MiB growth/cleanup limits
pass. Loaded-close, legacy resources and composition pass 3/3 (14.66/14.29/53.33s),
as do network isolation, builds and documentation. The rendered loaded state is
inspected and task artifacts are removed. No Actions, remote CI or live account
operation occurred.

Next exercise composed default cache pressure beyond the 64-entry / 64-MiB
immutable diff/index limits, checking eviction, retained ownership and exact
source when revisiting an evicted selection. Existing host unit tests cover
small-capacity eviction and byte refusal; they do not establish the full native
application's default-capacity behavior. Reuse the stored large-review/epoch
inputs and direct public paths. Backoff/offline, fuzz/diagnostic, broader active
work, BPR-0622 Windows/Linux and earlier live/human gates remain open.

BPR-0634 and BPR-0635 separately exercise the default entry and byte ceilings
through real composed pressure. Both are required; small-capacity unit evidence
or many small entries does not establish the 64-MiB byte-pressure criterion.


BPR-0634 completes default entry pressure in `470adea7d7683577893b954f7a0c3d38c1f60689`.
The real public app selects 65 distinct small files at unchanged limits. Cache
entries stop at 64; the oldest weak raw diff/index survive through capacity and
expire at eviction. Revisiting the evicted file rematerializes its exact stored
blobs/source through the ordinary host. Peak bytes are only 212,992, explicitly
not byte-pressure evidence. Public close/drain/destruction empties cache/jobs,
releases current native/immutable owners and preserves Git/catalog data without
maintenance. Full-window source is inspected.

The same Mac records 54.600 ms maximum UI interval, 64 peak cache entries and
one diff/one workbench/two epoch jobs. Footprint is 52.954 MiB before close and
52.173 after destruction versus 4.547 baseline; retained Qt/allocator memory
and provisional 160/96-MiB growth budgets are explicit. Entry pressure, both
existing resource courses and isolation pass 4/4 (77.49/13.44/14.41/4.06s).
Build/documentation/diff checks pass; no production code or remote/account state
changed, and consumed task artifacts are removed.

Next is to split and implement BPR-0612, the full Qt JSON-to-Glaze migration,
before further performance work. After migration, BPR-0635 must use enough
distinct large comparisons across stored heads to exceed 64 MiB before reaching
64 entries. Verify byte eviction, weak ownership,
exact public revisit and cleanup, and measure native footprint for that larger
working set. Do not promote the small-file entry course to byte-budget proof.
Earlier live/human, Windows/Linux, backoff/offline, fuzz/diagnostic and broader
active-work criteria remain open; all execution remains local without Actions.
