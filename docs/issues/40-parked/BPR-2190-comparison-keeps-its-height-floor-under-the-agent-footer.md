---
schema: "implement-rfc.card.v1"
id: "BPR-2190"
title: "The comparison keeps its height floor under the agent footer"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
parked_on: "human-review"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"regression found closing BPR-2180, 2026-11-15","governing_rfc":"RFC-BPR-0101","related_cards":["BPR-2051","BPR-2180"],"regressed_by":"f51063b5b","reservation":"docs/issues/_reservations/BPR-2190--BPR-2190--1e723b85df6d.json","lane":"claude/v0-5-0-prerelease-targets-52265d","commits":["0e5f2e1e1","984dd4696"]}
---

# BPR-2190: The comparison keeps its height floor under the agent footer

`application_uiforetw_selected_diff_contract` fails its capture layout check:

```text
UIFORETW_CAPTURE_LAYOUT_FAILED:985:861:open=827 width_floor=750
  folded_height_floor=872 open_height_floor=838 window=1280x1000
  menu_bar=0+20 strip=20+30 review_bar=50+28 epoch_line=hidden tabs=78+28
  comparison=0,106 1262x861 status_bar=967+33
```

At a 1280x1000 window the folded comparison is 861 px tall against its floor
of 872 (and 827 against 838 with the Updates line open). The status bar
measures 33 px.

The regression is f51063b5b, "Layout round: agent footer, Agents roster, stop
editing and tour panel (owner decisions 2026-11-11)". It put the agent footer
in the pull request window's status bar
(`statusBar()->addPermanentWidget(AgentFooter, 0)`), the owner's "A + footer 1"
decision under [[BPR-2051]], specified in
[RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md). The footer's
picker is taller than the status line, so the status bar grew and the
comparison above it lost the height the floor protects.

Found while closing [[BPR-2180]]. Each point was built through the dev preset
in a clean scratch worktree and the contract run alone:

| Commit | What it is | Result |
|---|---|---|
| 2af8e7665 | BPR-2180's last code commit | fails: 985:861:open=827 |
| 775bd2d76 | its parent | fails, same numbers |
| 503046cb4 | before any BPR-2180 code | fails, same numbers |
| 44ddc0eee | f51063b5b's parent | passes |
| f51063b5b | the agent footer enters the status bar | fails, same numbers |

The contract last recorded a pass at bbe13b2ba (BPR-1852, 2026-11-03).

## Acceptance

- AC01: At the contract's window sizes the folded and open comparison keep
  their floors (872 and 838 per mille of a 1000 px window) with the agent
  footer in the status bar, which keeps every state the owner accepted for it
  (picker, running run with Stop, cost).
- AC02: `application_uiforetw_selected_diff_contract` passes, with the floors
  unchanged unless the owner decides otherwise.
- AC03 (human, only if the footer's look changes): the owner accepts the
  footer in a frozen build.

## Fix (`0e5f2e1e1`, pin `984dd4696`)

The floors are unchanged. The footer was made to fit the status bar.
`QStatusBar` takes its height from its tallest item, and the footer was
fixed at the height of its tallest control: Stop all and the runs count were
28 px offscreen, the picker 25 and Agent settings… 25. Every footer control is
now fixed to one line of the footer's words (their font height plus 2), no
taller than the review's line beside it, and the footer is that height in
every state. Offscreen the bar stays 22 px (the footer is 17) whether it
shows the picker, a run with Stop all, several runs, Agent settings… or the
cost.

The capture's later checks had drifted behind the layout failure, which ended
the run before they were reached. The expected review bar now has the local
draft's leading and publish controls (hidden over a pull request), Ask
(BPR-2164) and the view picker. Comment is gone from it (BPR-2176), so Compose
review closes the bar.

**Visible change (AC03):** the footer's chips and the picker's pill are drawn
one line tall, shorter than the 25 to 28 px the owner accepted under BPR-2051.
Offscreen renders: `agent_footer --render DIR` (states idle, one run, several,
quiet, answered, needs setup, check failed).

## Evidence

- AC01, AC02: `application_uiforetw_selected_diff_contract` passes 3 of 3 at
  `0e5f2e1e1`. The comparison is 872 folded and 838 open at 1280x1000. Before
  the fix it was 861 and 827.
- `qt_agent_footer` passes 3 of 3. It now checks that the bar stays at the
  review line's height idle, with two runs and in setup (status_line=22,
  with_footer=22, footer=17).
- `application_review_round_trip_composition_contract` passes over the
  resealed app pin.
- AC03 is pending: the owner judges the shorter footer in a frozen build.
