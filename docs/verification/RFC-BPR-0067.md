# RFC-BPR-0067 verification and attendance

Mandate `home-draft-r1`; plan `home-draft-v1/draft-home`; immutable RFC base
`2beb98cc9a58eeeeeebb8bdd08cc0af4a49c4e20`. Product candidate `f170654e651cb57e262d6c4e4696378c5464c365`.
Regression refinement `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` changes tests only; product source is
byte-identical to the frozen candidate. [[BPR-1389]] remains human-pending for AC04.

| Criterion | Evidence and disposition |
|---|---|
| AC01 | Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. |
| AC02 | Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. |
| AC03 | Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. |
| AC04 | Automated pass: four appearances and 100/125/150/200% shared rows, wrapping, selection, Reviewing/account chips, opening states and accessible wording; composed full-window captures. Pending: owner verdict in short attended session df622dfed34516817f93fa5ac168bf03. |

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
They are layout/accessibility evidence; AC04 native visual judgment is pending.
`frontend-design:frontend-design` was loaded from the installed plugin and applied
within the owner’s specified native theme, font and no-animation constraints.

The frozen source/build root is
`/Users/jaeger/bpr1389-home-draft-v001.nh7rl2k8`; executable SHA-256
`41f42b50631aa12a58ab3c974427f0a7c27a013670ca349901d6a9d978fed1d8`.
The first freeze check used the wrong fixture for Home’s synthetic repository and
reached a readiness timeout. The launcher was regenerated from the freeze tool’s
same template with the supported `github-com` fixture and its committed paths;
its own `--check` passed `ATTENDED_HOME_SHOWN:rows=3:delay=0ms`. No binary/source
was changed during that fixture correction. The tree is read-only. The ready
short session binds one AC04 Accept/Reject judgment, retains candidate protections
and has no recorded decision. Its loopback URL/token stay in external runtime
state, never Git. The coordinator owns the handed-back wait at cursor 3.

Documentation impact: live Home contracts/specification and system architecture
now name optional observed metadata, exact numeric account binding, queue schema
8 and Recent schema 6. Enrollment, attention, sorting, notifications, provider
request shapes/limits, mutation authority, other review chrome and release policy
have no semantic change. Historical shell plan exits and RFC0032 A8 native/human
obligations remain intact; this work neither closes nor waives them.
