# BPR-1403: v0.1.9 release gate repairs

Bounded assignment `/root/release_gate_repairs`, mandate
`release-v019-20261012-rfc-implementer-r2`, within RFC-BPR-0038. The owner
requests a new prerelease patch build with full macOS/Windows/Ubuntu 26.04
packages and requires an RFC implementer for actual fixes. The coordinator
owns native producer dispatch and packaging; its later r3 mandate narrows
v0.1.9 to Mac publication. No signing, notarization, tag, push, publication,
credential change or live GitHub mutation is performed by this implementer. The implement-rfc and existing publish-release workflows apply.

Release baseline: `0a35a94f263599d84da871aa7129a625cafd5b57`.
Entry: `c9a4164756bcd8ce3601115c0912845cb3ef5bcd`.
Non-timing repair source: `0829baad5c75c9d5aeac4dd4c3d311558d85e9bc`.
New commits change tests and the reviewed composition tripwire; product source
remains equal to entry. Earlier adopted fixture repairs stay present.

## Original gate and diagnosis

The original normal Mac run completes with exit **8**, **666 passed / 22 failed /
1 skipped**, in **2795.874512 seconds**. Its log, JUnit and timings remain in
`build/v0.1.9-tests.log` and
`build/test-results/dev/20261012T152956.824692Z/`. The run started with older
provider/replay/offline/attended binaries; repairs and the later textual reseal
occurred during it. It is a retained failure collection, not an exact final
candidate pass. Focused rechecks bind final source and rebuilt artifacts.

| Failed group | Cause and repair |
|---|---|
| `contained_temporary_usage_contract` | New GPG native fixture used raw TemporaryDirectory; adopted `ebb223c8e` uses the existing contained helper. |
| `architecture_boundaries` | A nested release clone was included in source auditing; coordinator moved it outside source. No audit rule or ignore was relaxed. |
| `github_provider` | Actor-conflict fixture lacked status and optional requirement operations. Adopted `ebb223c8e` supplies the empty status; `7d88970dd` supplies five explicit canonical unsupported terminals. Actor-conflict assertions and unknown-operation failure remain. |
| `application_composition_contract` | Accepted Draft/Home waiting source moved full/application-manifest digests after `493d08433`. `3cc1b2fc3` reviews every changed manifest member and reseals only those two digests. The mutation digest, inventory and semantic bypass guards remain unchanged. |
| `application_recent_account_isolation` | Fresh legacy phase had no accepted sign-in. Adopted `ab32dd295` explicitly connects it through the existing primary action; exact account/title masking still applies. |
| `application_attended_recent_replay` | UIforETW author equals viewer; adopted `903b196cc` looks in My pull requests while retaining the exact row/open target. |
| `large_review_replay_contract` | Missing status exchange and obsolete mandatory-before-optional order. Adopted `81a0477d6`, `5f3654ec5`, `f2f5010c4`, `c9a416475` restore strict recording/order/counts. `3cc1b2fc3` additionally checks the five exact optional refusal requests and their unsupported observations. |
| `application_rate_limit_render_recovery`, `application_active_changed_scaling` | Same strict paged replay mismatch; recording fix retains all recovery/account/selection and scaling assertions. |
| 13 large-review public courses | Same strict replay mismatch before course work: prepared-open, history, launch, cache-entries, first-tab, responsiveness, active-work, approximate, unavailable, benchmark, resources, pathological and loaded-close. Full affected set is rechecked below. |

An additional remaining history counter is corrected at `0829baad5`: its
per-epoch assertion now includes the same status exchange. The Recent legacy
fixture now removes Draft as well as account before stamping version four;
retaining a newer column had caused a correct transactional migration refusal.
Its exact account/destination/reading assertions are unchanged.

The recording has 31 exchanges including session identity and independent
timeline; the PR pager consumes 29 recorded exchanges, then the timeline makes
30. Five separate optional requirement attempts receive exact explicit
unsupported/unavailable terminals, without inventing actor or branch-policy
facts. The app fixture counts successful recorded reads as `1 + 30 * epochs`.
Every stored head and file/commit identity retains strict validation.

## Composition source review

Only `f170654e651cb57e262d6c4e4696378c5464c365` and
`393dfe602354b189a6e883c92dc66bdc6a66579f` changed production-manifest
members after the last matching seal. The full diffs preserve accepted account,
host and session authority. Optional Draft updates an existing matching Recent
destination; unknown facts cannot erase retained truth. Waiting/authorship
observations come from an accepted snapshot and carry host dispatch order
through the existing queue host and notification baseline. Main changes are
testing-only fixture/count changes. There is no new enrollment, provider,
credential, grant, mutation or publication capability, or source inventory entry.
Prior RFC0067/0070 evidence and BPR-1399 independent review consumption remain
candidate-specific. This is an engineering self-review of the reseal.

Full application digest:
`53e078071d948595e885f66648e92421a7339f6848bed95254ca1ab4d485ae79`.
Application manifest:
`50e895b63e95628d9ac692e06aaa52eca6d834a4de75d7aae426508198ff022d`.
Unchanged mutation branch:
`e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.

## Verification frontier

The targeted build passes. Nine of ten first focused contracts pass, including
all 32 heads through production replay, architecture, contained temporary use,
anonymous read/native HTTPS and provider content policy. The remaining provider
fixture failure is repaired at the current source and its final recheck passes.
The retained first focused log is `build/v0.1.9-repairs-contracts.log`.

The positive composition source inventory and hostile-source direct-bypass
controls pass; `build/v0.1.9-repairs-composition-counterexamples.log` retains
all cases in that contract. Clang-format **22.1.8** checks all nine changed C++
files. Focused clang-tidy 22 with warnings as errors passes the replay TU after
a pre-existing inefficient concatenation is replaced with byte-identical
append construction; the failed and final tidy logs remain. Documentation
reports DOCUMENTATION_OK and all 67 documentation contract tests pass.

The serial 26-row affected/public CTest gate ends at **21 passed / 4 failed /
1 skipped**, exit **8**, in **338.51 seconds** at `7d88970dd`. Its log/JUnit
are `build/v0.1.9-repairs-public.log` and `build/v0.1.9-repairs-public.xml`.
The four failures are responsiveness, launch, the remaining history counter,
and the newer Draft column retained by the simulated legacy store. Provider,
complete tests-disabled composition, first-tab, active work, prepared open,
resources/cleanup, replay, round-trip, rate recovery, scaling, offline inventory
and attended replay pass.

After `0829baad5`, Recent account isolation passes in **41.08 seconds**, including
all four separate-process application phases and the exact persisted account,
destination and reading-position postchecks. This uses the unchanged product
composition with the corrected Python legacy schema procedure. The restored
benchmark rebuild passes; the full-history course passes through all 32 heads
in **155.23 seconds**, exit **0**, with exact retained source/count assertions
and unchanged timing budgets. Its log/JUnit are
`build/v0.1.9-repairs-history-final.log` and
`build/v0.1.9-repairs-history-final.xml`.
All original non-timing failures now have passing affected checks; the two
remaining PR-load timing failures retain their explicit owner release exception.

`application_large_review_cache_bytes` is a configured Debug skip (exit 77),
qualified only for an optimized build by BPR-1265. A skipped row is not a pass.
Mac Debug/offscreen timings are diagnostic platform observations, not universal
latency, compositor or native desktop guarantees. Normal Windows and Ubuntu 26.04 gates remain coordinator-owned and pending
for the follow-up patch, rather than conditions for the amended Mac release.

## Documentation impact and continuation

No new product semantics, public API, package path, authority or UX design is
introduced by these repairs. Existing product/architecture documents remain
accurate; this ledger and BPR-1403 record fixture/evidence corrections only.
RFC0038 and BPR0958–0962/BPR0973 remain open, preserving native Intel/clean
accounts and desktops, vault/removal, trust/licensing/source access, crash/privacy,
and vulnerability qualification. Earlier evidence and failed scans are unchanged.

Durable runtime checkpoint:
`.mask/sessions/release-v0.1.9/repairs-handoff.json`.
Exact candidate binary hashes:
`.mask/sessions/release-v0.1.9/repairs-candidate-identities.json`.
All independent unattended work finishes before handoff. No attended review
criterion or review server is introduced by this bounded fixture repair.


## Owner release amendment and retained timing failures

Mandate `release-v019-20261012-macos-r3`, 2026-10-12: "Just get the current
mac build out, timing misses be damned, and fix everything else including
windows and ubuntu for a follow-up patch". The coordinator now owns the
macOS-only v0.1.9 package/sign/notarize/publication path. The implementer supplies
one frozen clean source candidate and releases its writer ownership. No new
version is chosen. [BPR-1404](../issues/20-ready/BPR-1404-verify-followup-release-gates.md)
retains timing repairs and Windows/Ubuntu native gates/packages for follow-up.

PR-load input gaps miss 100 ms at **111.602 / 107.923 ms**, then reproduce
in isolation at **104.440 / 101.098 ms**. The raw failed timing logs are
`build/v0.1.9-repairs-public.log` and `build/v0.1.9-repairs-timing-repro.log`.
The owner explicitly accepts these failures for this release. No assertion,
threshold, skip or failed observation is changed or relabeled as passing.

Bounded native sampling of only the owned offscreen benchmark and temporary
GUI event timing are diagnostic. The event probe sees adjacent QObject
MetaCall work around 90 and 42 ms during load. Sampling is sparse under Debug;
it proves no exact root cause or fix. Source is restored byte-for-byte to the
pre-instrument candidate and the normal benchmark rebuilt. Every sampling and
instrumented course/build is complete; no review server is introduced. The
ignored checkpoint retains actual commands/PIDs and logs for follow-up.
