---
schema: "implement-rfc.card.v1"
id: "BPR-2240"
title: "The loading and canary courses expect what BPR-2216 and BPR-2219 changed"
status: "Done"
rfcs: []
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2240--BPR-2240--4c3e98a30c5b.json","origin":"seen failing during BPR-2236 work, 2026-11-16"}
---

# The loading and canary courses expect what BPR-2216 and BPR-2219 changed

## Origin

During [[BPR-2236]] work `qt_composed_github_scale_100`, `_125` and `_200`
("the prepared loading overview does not manufacture accepted content"),
`qt_agent_launch_canary` and `qt_agent_launch_canary_durable_course` failed in
files that change did not touch.

## Scope and acceptance

- **AC-a.** Each failure is reproduced at lane HEAD from a clean checkout and
  traced to the commit that introduced it.
- **AC-b.** Each is fixed at the right layer: the test where the product
  change was owner-accepted, the product otherwise.
- **AC-c.** The named courses pass, with the other courses on the same paths.

## Cause

All failures came from test expectations that were stale after owner-accepted
product changes. The product was not wrong.

| Course | Introduced by | What changed |
|---|---|---|
| `qt_composed_github_scale_*` | `5d809b68d` ([[BPR-2216]] round 3), then `e5e50701f` (round 4) | While the pull request is read, only the status bar says what loads. The Updates line (`comparing_button`) no longer says "Loading…" or the read's stage, and the overview summary lost "Provisional metadata; review data is still loading." The owner accepted [[BPR-2216]] on look v004 |
| `qt_agent_launch_canary`, `qt_agent_launch_canary_durable_course`, `qt_claude_process_wrapper` | `7b54f9121` ([[BPR-2219]] round 3) | The decoder starts each later text block of a turn with a paragraph break, so a report's title begins on its own line. The canary writes each fact as its own message item, so every fact after the first now arrives as `"\n\n<fact>"`. Exact matches and prefix matches miss it. The owner accepted [[BPR-2219]] on look review-report-v003 |

`qt_claude_process_wrapper` was not in the original report. It failed for the
same reason and was found while checking the other courses that read the
canary.

Bisect, each step a clean detached checkout built for only the course's
targets. The composed course passed at `cf4702b54` and failed at `5d809b68d`.
The canary courses passed at `e272b1711` and failed at `7b54f9121`.

## Delivered

`afeec5353` (tests only):

- `composed_github_tests.cpp`, the held-read course. The Updates line must not
  say "Loading". The status line (`workbenchStatus`) must say the read, with
  the stage and page count. A stale or foreign progress update must not
  replace it. The summary must keep the commit count and state and carry no
  loading sentence.
- `agent_launch_canary_tests.cpp` and `claude_process_wrapper_tests.cpp`. A
  `fact()` helper removes the one paragraph break the decoder adds before a
  later block. Each fact is then compared exactly at every place a course
  reads one.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | Clean detached checkout of `ee65ed5a1`, the failing courses' targets only | All five failing courses, plus `qt_claude_process_wrapper`, reproduced. The bisect is above |
| AC-b | The cause above | Test layer, owner-accepted behaviour kept |
| AC-c | `qt_composed_github_scale_100/125/200`, `qt_agent_launch_canary`, `qt_agent_launch_canary_durable_course`, `qt_claude_process_wrapper`, `agent_parent_death`, `qt_agent_steering_fixture`, `application_agent_invocation_composition_contract` at `d7543af82` (`ee65ed5a1` plus the fix, the same tree change as `afeec5353`) | Pass |
