---
schema: "implement-rfc.card.v1"
id: "BPR-2411"
title: "The Agent tab's running turn is folded in core from what its run is seen doing, and the core says what an ended turn's answer is made of"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2409"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2411--BPR-2411--ac3a477ee44a.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"2bda18460","origin":"RFC-BPR-0130 stage 5 (draft card DRAFT-0011 of the attended review 3cd822672efb50572b10ab10747889ba, its first part: the reducer); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2409","BPR-1941","BPR-2004","BPR-2005","BPR-2153","BPR-2252","BPR-2305"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3"]}
acceptance: []
---

# BPR-2411: The Agent tab's running turn is folded in core

## Why

RFC-BPR-0130 AC07: "`agent_tab_view` holds no control owned by another
widget and no state of a running turn; the turn's state comes from a core
reducer covered without Qt." DRAFT-0011 is the largest card of the RFC and
names its own split ("by observation"). This card is its first part: the
running turn's state and its ending are the core's. The borrowed controls
(the picker, the replies, the composer and the private history the tab
reparents, and its reads of `agent_launch_panel`, `side_chat_panel` and
`finding_queue_view`) are the second part, with `src/app`. Visible: nothing.

### What was there (BPR-2409)

- `progress` kept the run's phase as an `int` (`LastPhase`) and began a turn
  when it went from idle to preparing.
- `update_turn` read the running turn off the launch panel each time (the
  phase, what was written, the latest line, what was steered, the cost, what
  the runtime was doing, whether it read only the pull request's files) and
  decided there which stage the turn's line says, and, once it ended, which
  of five endings its answer is made of.

## Change

1. **Core** `agent_live_turn.hpp`: `agent_live_observation` (what the host
   sees of the run: phase, workspace checked, written, latest line, steered,
   cost, activity, files only); `agent_live_turn` (the running turn: phase,
   stage, answer, latest, steered, cost, activity, files only);
   `reduce_agent_live_turn` folds one observation and says what changed (a
   turn began, the phase changed, the steered messages changed, the run
   ended); a turn that began carries nothing of the last; an ended turn keeps
   what it had with the words it ended on; what the runtime does is said only
   while it says it. `decide_agent_live_end` says what an ended answer is made
   of: the answer, a silent answer, every write failed, kept on a stop, kept
   under unfinished helpers, or why it failed.
2. **Agent tab**: `observe_run` is what the run is seen doing;
   `progress` folds it into `Live` (which replaces `LastPhase`) and begins a
   turn when the core says one began; `update_turn` presents the turn the
   core folds (its stage, answer, latest line, cost, activity, steered
   messages) and words the ending the core decided. The words stay Qt's.
3. **Courses**: `core_agent_live_turn` (begin, check, prepare, think with the
   latest line, write, cost and activity as reported and only while
   reported, steered once as a change, end keeping what it had with its last
   words, idle again folding nothing, the next run carrying nothing; the six
   endings).

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: the borrowed controls and the tab's reads of the other
panels (DRAFT-0011's second part, which touches `window_agent_chat.cpp` and
`window_agent_setup.cpp`). AC07 holds once that lands.

## Acceptance

- AC1 (RFC AC07, its second clause). The running turn's state is a core value
  folded by a reducer covered without Qt; the tab keeps no phase of its own.
- AC2. What an ended turn's answer is made of is the core's decision.
- AC3. The Agent tab's tests and the composed course in the native look pass
  with every render byte-identical; format and tidy on touched units.

## Evidence

Revision `2bda18460` (on stage 5b, `1c4dc7206`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1, AC2: `core_agent_live_turn` passed.
- AC3: the 62 rows of the 5b list with `core_agent_live_turn` (the Agent
  tab's Qt and end-to-end courses, `qt_workbench_window_scale_100` and
  `_200`, the side chats, the drawer-fit and composed courses at 100% and
  200%) passed. The composed course under the native look (Cocoa, kept off
  screen, light, the display's 200%): "composed-drawer-fit: 39 states, 0
  faults, 50 transcript edges drawn, dpr 2, cocoa/macos".
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy (`tidy.py --warnings-as-errors`) over
  `agent_live_turn.cpp`, its test and `agent_tab_view.cpp`: clean but for
  the one older finding (`press_offer`'s copy of `aChosen`).

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is `stage-5b-after/cocoa-macos/`, after is `stage-5c-after/cocoa-macos/`.
All 274 renders are byte-identical, the running states (`agent-queue-*`,
`agent-stopped-*`) among them.

## Owner judgement

- None asked: nothing changes on screen.
