# RFC-BPR-0067 verification and attendance

Mandate `home-draft-r1`; plan `home-draft-v1/draft-home`; immutable RFC base
`2beb98cc9a58eeeeeebb8bdd08cc0af4a49c4e20`. Product candidate `f170654e651cb57e262d6c4e4696378c5464c365`.
Regression refinement `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` changes tests only; product source is
byte-identical to the frozen candidate. [[BPR-1389]] records acceptance of all four criteria.

| Criterion | Evidence and disposition |
|---|---|
| AC01 | Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. |
| AC02 | Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. |
| AC03 | Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. |
| AC04 | Automated pass: four appearances and 100/125/150/200% shared rows, wrapping, selection, Reviewing/account chips, opening states and accessible wording; composed full-window captures. Pass: human ACCEPT of exact frozen bpr1389-home-draft-v001 in session df622dfed34516817f93fa5ac168bf03, decision 809ee3e4-ba73-49a3-941a-e0052c0c63cc at event 4; handed back at event 5 with no pending. |

The [checkpoint directory](../issues/_checkpoints/BPR-1389/verification-summary.json)
records actual commands, revision binding, runtime surfaces and review identities.
The initial integrated pass ran 21 checks; 18 passed, including architecture,
host/queue storage, composed Qt Home/Open, 100/125/200% row courses and the CLI
public course. Its three failures came from newly added test fixtures: omitted
review callbacks in the provider truth table, legacy/future Recent SQL assumptions,
and assuming the first Recent row’s identity after restart. Those were corrected
without product changes. The final affected provider/storage/native public-path
pass was 3/3 at the regression commit; final row courses were 3/3 with account-chip,
Reviewing and opening-state coexistence. The 150% isolated row course passed too.
Both complete authority and optional-value assertions can fail when draft status
is fabricated, erased, misattributed or omitted from accessible output.

Core-only configure/build and all **266 tests passed**, including both architecture
boundary checks, current Recent migration/cancellation tests and account isolation.
The dev build passed. Touched files passed clang-format **22.1.8**; documentation
verification and its 67 script tests passed. Focused clang-tidy 22 checked ten
production implementation units: nine passed; `workbench_window.cpp` retains one
preexisting `compact_below` naming error, identical at the immutable baseline.
No changed-source diagnostic was found. This does not claim a clean full tidy gate.

Four-appearance composed offscreen images use actual 1100×760 and 900×760 windows;
the request for 520 pixels is clamped by the existing native minimum. Shared row
rasters/checks run at 100, 125, 150 and 200 percent and preserve complete literal
wrapping, neutral status ink, selected text, native focus and existing actions.
They are layout/accessibility evidence; the owner accepted the frozen native candidate for AC04 in the session recorded below.
`frontend-design:frontend-design` was loaded from the installed plugin and applied
within the owner’s specified native theme, font and no-animation constraints.

The frozen source/build root is
`/Users/jaeger/bpr1389-home-draft-v001.nh7rl2k8`; executable SHA-256
`41f42b50631aa12a58ab3c974427f0a7c27a013670ca349901d6a9d978fed1d8`.
The first freeze check used the wrong fixture for Home’s synthetic repository and
reached a readiness timeout. The launcher was regenerated from the freeze tool’s
same template with the supported `github-com` fixture and its committed paths;
its own `--check` passed `ATTENDED_HOME_SHOWN:rows=3:delay=0ms`. No binary/source
was changed during that fixture correction. The tree is read-only. The short session bound one AC04 Accept/Reject judgment and now retains the matching human Accept. Its loopback URL/token remain in external runtime state. The consumed event cursor is 5; the completed host is stopped, and the frozen candidate remains protected.

Documentation impact: live Home contracts/specification and system architecture
now name optional observed metadata, exact numeric account binding, queue schema
8 and Recent schema 6. Enrollment, attention, sorting, notifications, provider
request shapes/limits, mutation authority, other review chrome and release policy
have no semantic change. Historical shell plan exits and RFC0032 A8 native/human
obligations remain intact; this work neither closes nor waives them.


## Attended acceptance and final closeout

The human accepted frozen candidate `bpr1389-home-draft-v001`, product revision
`f170654e651cb57e262d6c4e4696378c5464c365` and binary SHA-256
`41f42b50631aa12a58ab3c974427f0a7c27a013670ca349901d6a9d978fed1d8`.
Session `df622dfed34516817f93fa5ac168bf03` and fingerprint
`3dc0d110224cab33969d5ad212d02853480beaa07aa8d4659150cf88b7c76c88`
match the prepared handoff. Decision `809ee3e4-ba73-49a3-941a-e0052c0c63cc`
was saved by actor `human` at `2026-10-12T04:44:28.385297+00:00`, event **4**,
with outcome **accept** and an empty note. Event **5** handed back the same session
with no pending decisions. The coordinator persisted cursor **5** before the
fresh post-attendance implementer reconciled it. The
[acceptance receipt](../issues/_checkpoints/BPR-1389/human-acceptance.json)
contains the safe decision/event fields; no session token is committed.

The accepted product and existing passing checks are unchanged. The immutable
base-to-candidate diff is accounted for in the
[changed-path receipt](../issues/_checkpoints/BPR-1389/product-paths.json).
Required dependencies RFC-BPR-0010, RFC-BPR-0026 and RFC-BPR-0066 are
Auto-Verified; card prerequisites BPR-0776, BPR-1379 and BPR-1388 are Done.
The documentation gate is refreshed for terminal bookkeeping. Historical shell,
RFC0032 A8, platform and release qualification obligations remain outside this
milestone and retain their existing states. The preexisting tidy diagnostic
above remains classified; it does not fail an RFC0067 acceptance criterion.

The authenticated attended-review `stop` returned `stopped: true`; PID 5647 is
absent, and exec session 12891 is no longer registered in either owner’s runtime.
All task commands and writers are quiescent. Frozen source/build and safe runtime
evidence are retained under their existing protection. No additional product
change, live GitHub action, push, browser-profile read or desktop automation was
performed during closeout.


The [terminal verification receipt](../issues/_checkpoints/BPR-1389/documentation-closeout.json)
records `DOCUMENTATION_OK`, 67/67 documentation script tests, valid scoped board
state and the retained accepted binary digest. Initial global gate failures came
from another active RFC import and were resolved by that owner. Its source edits
are outside this closeout and are excluded from the reused product evidence.
The guarded accepted-card commit is `1cc5c3e71cbc18c1ebb07b31c9460ec5b04c8b36`;
RFC-BPR-0067 and its index now have the synchronized Auto-Verified status.
