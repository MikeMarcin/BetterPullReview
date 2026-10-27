---
schema: "implement-rfc.card.v1"
id: "BPR-1766"
title: "Investigate the Windows Debug PR Find timeouts"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"Owner's v0.3.3 Debug timeout waiver and follow-up request, 2026-10-24","reservation":"docs/issues/_reservations/BPR-1766--BPR-1766--9665eb769091.json","related_cards":["BPR-1694","BPR-1765"],"release_candidate":"b2801f2aaa858fd5e7d18d76253834d34024abc7","diagnostic_candidate":"134711456e97b8162b771f637c1cb155e09dedb3","external_evidence":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032","triage_20261026":{"date":"2026-10-26","disposition":"stale","reason":"Superseded by the owner's standing Debug timing policy (2026-10-26); Release completes the same courses in about 30 s."}}
---

# Investigate the Windows Debug PR Find timeouts

## Owner amendment

On 2026-10-24 the owner instructed: "Waive any debug timeouts for this release
and keep going, file cards to investigate after". The release coordinator
relayed this v0.3.3-only amendment and stopped PR Find diagnosis and repair.
It broadens the earlier exception recorded in [[BPR-1765]] to Debug test
timeouts on the selected native platforms. The release candidate remains
`b2801f2aaa858fd5e7d18d76253834d34024abc7`; passive diagnostic candidate
`134711456e97b8162b771f637c1cb155e09dedb3` remains separate and unshipped.

Actual timeouts remain failed, waived observations. The amendment supplies no
passing result for them, no completed result for an interrupted or unrun row,
and no waiver of compilation failures, non-timeout failures, Release gates,
security, packaging or native trust. It does not carry into a later release.
This separate documentation card records the three observed PR Find failures;
the diff workbench investigation remains on BPR-1765.

## Evidence and scope

The affected executable is `test_source_pr_find_public_path`, built from
`tests/end_to_end/source_pr_find_public_path_tests.cpp`. These are PR-wide
search and result-reveal courses, distinct from the pane-Find courses and
their previous event-turn repair. All three original registered rows retain
their 120-second limit:

| Row | R188 Windows Debug, exact `b2801f2aaa858fd5e7d18d76253834d34024abc7` |
| --- | --- |
| `application_source_pr_find` | Timeout, 120.08 s |
| `application_source_pr_find_scale_1_25` | Timeout, 120.08 s |
| `application_source_pr_find_scale_2_0` | Timeout, 120.08 s |

Each R188 `LastTest.log` block has empty process output. The remainder suite
reports 298 passes, one configured skip and these three timeouts. The missing
success marker does not locate the timeout within startup, navigation, event
delivery, rendering or shutdown.

Older passing evidence is retained for comparison, not substituted for R188:

| Run and exact source | Default | 125% | 200% |
| --- | --- | --- | --- |
| R99 Windows Debug, `c68c7d629020d709c6cee2470ae7dc372ae1d4fe` | 26.30 s | 27.37 s | 30.63 s |
| R168 Windows Release, `b2801f2aaa858fd5e7d18d76253834d34024abc7` | 30.04 s | 30.45 s | 31.09 s |

Diagnostic commit `134711456e97b8162b771f637c1cb155e09dedb3` adds flushed
semantic-phase and wait boundaries, plus progress after each 100 of the
existing 1,000 synchronous PR navigation triggers. Independent review reversed
those additions and recovered every original test token: no predicates,
condition-call sites, event processing, actions, 180 ms settles, 20-second
waits or registered 120-second limits changed. No native diagnostic run was
performed before the owner stopped this work. Its instrumentation can affect
timing and is not a repair or a qualification result.

Raw evidence remains outside source under the metadata evidence root:
`windows-v033-r188/windows-debug-remainder.stdout.log`, its `LastTest.log` and
`windows-debug-remainder-suite/junit.xml`; the R99 and R168 comprehensive logs
and source-bound receipts; and
`r188-pr-find-debug-diagnostic-handoff.json`. The cause remains unestablished.

## Investigation and acceptance

- AC1: Bind a bounded native investigation to exact source, binary, Debug
  configuration, Qt/runtime identity, fixtures and the original row properties.
  Preserve all failed and older passing evidence. Locate the actual expensive
  or blocked phase before attributing a cause to the traversal loop, prior pane
  Find behavior, scheduling or generic Debug cost.
- AC2: Implement a scoped causal repair with a deterministic counterexample or
  work-count control that distinguishes the old and corrected behavior. Retain
  all 1,000 PR navigation actions, exact accepted comparison reuse, zero added
  compilation/source-read/scan assertions, result-side and span identity,
  cancellation, tab, focus, layout, rendering and shutdown checks.
- AC3: Run all three original Windows Debug rows with unchanged 120-second
  timeouts and original internal waits, argv, environment, scale, scrubs,
  working directory, render ownership and resource/scheduling properties.
  Complete native compilation prerequisites before long suites, then select
  focused Release and integration checks justified by the changed surface.
  Preserve valid unchanged evidence; do not restart every full suite by default.
  Record actual failures rather than retrying until a pass, increasing budgets,
  reducing actions or dropping assertions.
- AC4: Remove temporary diagnostic-only instrumentation when no longer needed,
  retain meaningful regression controls, review affected source/input seals
  against exact final bytes and evidence, and run the documentation gate. Keep
  this card open until those criteria are met; the v0.3.3 waiver supplies no
  follow-up acceptance or automatic waiver for a future release.

## Closed in triage, 2026-10-26

Closed as stale under the owner's standing Debug timing policy of
2026-10-26
([policy](../../development/release-test-profiles.md#standing-debug-performance-policy)).
All three rows still time out in Windows Debug, at R188 and again in
v0.3.4's `windows-dev` junit (120.08 to 120.11 s). The Release build of the
same source completes them: 30.04, 30.45 and 31.09 s at R168, and they pass
in v0.3.4's complete Windows Release suite. So the courses finish their
semantic work; what remains is Debug cost, which the policy no longer gates.
The timeouts stay recorded as failures. Nothing here shows a hang. The
policy allows one bounded larger-watchdog Debug run to tell the two apart.
If such a run does not complete, file a new hang card that starts from this
card's diagnostic commit
`134711456e97b8162b771f637c1cb155e09dedb3`.
