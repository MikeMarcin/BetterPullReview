---
schema: "implement-rfc.card.v1"
id: "BPR-1320"
title: "Qualify the affected Git installation and exact-file recovery"
status: "Blocked"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1319"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
acceptance: [{"criterion":"C1","outcome":"pending","revision":"980c088733bbed1938ad322f851f5e4e9fc4e7cb","evidence":{"kind":"observation","detail":"Direct native read-only acquisition identifies local /usr/bin/git2.50.1 Apple155 and canonical Homebrew gh2.101 on macOS26.6.2; exact commit/tree/blob/pack, denied outside writes, bounded owner-private temporary store, unchanged identity/executable/config metadata and cleanup pass. New required-operation capability probe exit129/3113B passes. Owner reports Git2.54.0 Apple157 but configured paths/source-build correspondence remain unavailable; local proof and fixture acceptance do not qualify that installation or private GHES authorization."}},{"criterion":"C2","outcome":"pass","revision":"980c088733bbed1938ad322f851f5e4e9fc4e7cb","evidence":{"kind":"command","detail":"Fresh adapter+privacy export2/2 pass18.33s. Composed multi-file GHES source/retry/reopen/persisted failed-state course passes17.22s through real installed Git/cache with recorded smart transport; two exact file endpoints, stale account/host/head/source/cancel controls pass in actual adapter fixtures. Actionable typed unsupported/probe-failed flow host0.72s, Qt3scales pass, core-only0.41s; anonymous/identifying closed exports accept only fixed new failure/step names and reject unknown canaries. Changed-line integrator7TUs tidy and worker changedTUs clean; inherited paged/fixture warnings recorded. Independent critical risk review no blocking findings; actual affected owner rendered PR remains separate pending."}}]
blocked_by: "prerelease-affected-feedback"
---

# Qualify the affected Git installation and exact-file recovery

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Direct installed-tool acquisition, confinement, bounds, credential isolation and exact-object evidence qualifies only a precisely identified installation; no arbitrary range gate.
- C2: Composed multi-file GHES exact source, retry/reopen and persisted failed-state recovery pass; stale head/account/host/selection and cancellation cannot publish wrong content; genuine refusal is typed/actionable.

## Owner support-policy amendment and evidence boundary

The owner rejects exact Git executable-version/banner equality. Required
operation admission uses Git 2.49+ because exact revision cloning needs
`clone --revision`, then a bounded credential-free local clone-help probe.
Successful admission still performs confined acquisition, exact object and
pack verification and identity checks; an unsupported tool and a failed probe
reach the core and Qt as distinct actionable reasons. The CLI opt-in, account,
host, request identity, cancellation and supervised store bounds remain.

The direct native read-only proof identifies `/usr/bin/git` 2.50.1 (Apple
Git-155), canonical Homebrew gh 2.101.0 and macOS 26.6.2 build 25G83. It proves
exact commit/tree/blob/pack material, denied outside writes and cleanup for
that installation. The owner reports Git 2.54.0 (Apple Git-157); its configured
executable paths and source/build correspondence remain unavailable. Local
Git-155 evidence does not qualify that installation or private GHES access.
Original C1/C2 remain intact; actual affected installation and rendered PR
recovery obligations are not replaced by fixture or capability-policy evidence.

## Native diagnosis candidate v001 (dispatch r5, 2026-10-08)

A tests-off build of exact source `980c08873` is frozen in the ignored
`artifacts/four-drafts-r1/ghes-native-frozen-v001/`, with a self-contained
ad-hoc-signed arm64 bundle (executable SHA-256 `bcc9fdad…7548`, macOS 26+),
a launcher (`6eb8ba94…1911`) that starts it in a fresh isolated profile via
`CFFIXED_USER_HOME` and refuses early, and read-only operator instructions
with a facts script. In the frozen source the CLI path takes the first `git`
on the app's PATH with no override (`src/app/src/github_cli_runtime.cpp:194`);
an app opened from Finder gets `/usr/bin:/bin:/usr/sbin:/sbin`, so it selects
the `/usr/bin/git` xcode-select shim. `gh` remains admitted only at exactly
2.100.0 or 2.101.0 (`gh_cli_git_smart_transport.cpp:451`); the owner's 2.101.0
is inside it. No diagnostic export records the selected tool paths or
versions, so those facts come only from the facts script.

Fresh session `b366df1f04624eb90e90d47c9262e52d` (cursor 9) asks three
separate questions: the reviewed tool facts from the affected Mac
(`BPR-1320-tool-facts-v001`), whether the affected PR loads
(`BPR-1321-pr-loads-v001`), and whether changed files show exact contents
(`BPR-1320-files-render-v001`). This is a diagnosis run, not BPR-1322
acceptance. Local smoke on the build Mac (Git 2.50.1 Apple Git-155) is not
the affected installation. C1 stays pending.

## Owner strategy change: affected evidence deferred to pre-release (2026-10-07)

The owner cannot answer the native diagnosis review. Session
`b366df1f04624eb90e90d47c9262e52d` is **withdrawn as unanswerable**: an
objective observation records the withdrawal on each of its three candidates
(cursor 12). No verdict exists; nothing was accepted or rejected. Its server
was stopped and candidate v001 stays protected.

The strategy is now to diagnose from the two existing anonymized diagnostics
logs (private, ignored and never committed), make the most defensible fix and
take affected-installation confirmation from the next pre-release.

**Re-scope of C1.** Direct evidence about the affected installation (selected
Git and gh paths, versions and provenance on the owner's Mac) is deferred to
pre-release feedback. C1 stays pending and is an explicit open gap. Local
installed-tool evidence and capability-policy changes do not satisfy it.

**What the logs support.** In the 0.1.4 code (tag `v0.1.4`,
`gh_cli_git_smart_transport.cpp`), the step is reported as `git_qualification`
only when both version processes returned and gh matched the 2.100.0/2.101.0
allowlist, but the Git `--version` output differed from the exact
`git version 2.50.1 (Apple Git-155)` banner. Both affected loads report
exactly that step, so gh passed its gate and the selected Git produced a
different banner, or a result whose output did not match. That is consistent
with the owner's reported Git 2.54.0 (Apple Git-157), but the logs carry no
banner, path or exit code. The build identifier does not name a source
revision, so matching it to the tag is an inference. Under `980c08873` a Git
2.54 that offers the required clone options would pass the version and
capability checks; this is reasoning from source, not an observation.
