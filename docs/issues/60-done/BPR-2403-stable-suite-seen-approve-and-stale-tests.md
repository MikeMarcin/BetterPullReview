---
schema: "implement-rfc.card.v1"
id: "BPR-2403"
title: "The Seen and Approve again courses pass on main and say why Approve waits; four rows the full dev suite found stale follow their sources"
status: "Done"
rfcs: []
depends_on: ["BPR-2374", "BPR-2217", "BPR-2249", "BPR-2366", "BPR-2381", "BPR-2393"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2403--BPR-2403--4d6586c48ef6.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"f5afce476","found_by":"coordinator: application_review_seen_public_path and application_home_approve_again_public_path failing since at least BPR-2367","related_cards":["BPR-2364","BPR-2374","BPR-2367","BPR-2217","BPR-2249","BPR-2366","BPR-2381","BPR-2393"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2403: The Seen and Approve again courses, and the stale rows of the full dev suite

## Report

`application_review_seen_public_path` and
`application_home_approve_again_public_path` stopped at "the review's
comments are read" and "Approve again can be submitted once the review's
comments are read": Approve stayed disabled for the course's 20 s wait. The
BPR-2367 lane (912f3852d) saw both fail the same way with its change backed
out. A Linux Release junit of d8768775b (2026-11-18) also lists both.

## Cause

Neither the product's Seen/Approve again nor the course was wrong. The
lane had [[BPR-2364]] (606cf91a1, c88b2bfa6), which refuses a whole-history
fetch when the repository's size is unknown, and lacked [[BPR-2374]]
(fa87cd4d5, merged 5880ddbf7), which gives the fixture repositories their
size. The review's history read was refused, its pending comments never
loaded, and Approve kept "Loading your pending comments… You can submit
once they have loaded."

- Rebuilt from 912f3852d^ with this card's course, both fail and the course
  says: `status "Loading your pending comments… You can submit once they
  have loaded.", Approve's tooltip "Loading your pending comments…", pull
  request ready`.
- The same tree with fa87cd4d5 applied: both pass.
- On main (5bc62c938) both pass, five times each, alone and under a load
  average of 60.

The 2026-11-18 Linux failures predate BPR-2364 and were not reproduced here
(no Linux host); their cause is not known.

## Change

1. **The course says why Approve waits.** `home_oversized_public_path_tests.cpp`:
   when Approve (or Approve again) stays disabled, the failure names the
   review's status, Approve's tooltip, whether the pull request is ready and
   the window's status line. What the course checks is unchanged.
2. **qt_profiles_settings_panel.** [[BPR-2366]] put a hidden Saved update
   drafts command in the window's Help. The windowless bar's Help is
   compared with the commands the window's Help shows, so a command hidden
   until it applies is not counted as one it lacks.
3. **application_thread_words_public_path.** Since [[BPR-2381]] (4caef315a)
   the report view writes its byline on every render, and with no report
   that was an empty `<a href="conversation">` link: a link to a screen
   reader with nothing to say. The byline links the question only when there
   is one; with no report it is empty. `qt_agent_report_view` checks both
   (negative control: it fails on main's view).
4. **contained_temporary_usage_contract.** `fixture_agent_tests.py`'s held-turn
   check (BPR-2394) takes its root from `contained_temporary_directory`.
5. **command_correctness_registry_contract, qualification_case_inventory_contract.**
   `tests/workbench_case_inventory.json` follows `workbench_window_tests.cpp`
   again: `test_a_pinned_conversation_card_offers_what_it_names`
   ([[BPR-2393]], f5024e9d4) in the default list and `--agent-send-refusal`,
   source SHA-256 resealed; 134 default cases, 265 qualification
   occurrences.

## Acceptance

- AC1. Both courses pass on main, and a stall at Approve names what the
  review waits for.
- AC2. The cause is shown, not inferred: the old revision fails, and passes
  with the fix that landed.
- AC3. Each stale row fixed here passes and still fails on what it guards.
- AC4. The whole dev suite was run once; every other failing row is listed
  with its cause.

## Evidence

Revision `f5afce476` (on main 5bc62c938), macOS arm64 Debug, offscreen.

- AC1: `ctest --preset dev -R '^application_(home_oversized|home_approve_again|review_seen|review_seen_stays|review_seen_fails)_public_path$'`:
  5 of 5 passed (7 to 8 s each). The two named courses passed in the full
  suite run and four times more each under `--repeat until-fail:4`.
- AC2: an exported tree of 912f3852d^ built for `test_home_oversized_public_path`
  with this card's course: both fail at the Approve wait with the status
  above; with fa87cd4d5 patched in, both pass.
- AC3: `qt_profiles_settings_panel`, `application_thread_words_public_path`,
  `qt_agent_report_view`, `contained_temporary_usage_contract`,
  `command_correctness_registry_contract`,
  `qualification_case_inventory_contract`, `fixture_agent_answers` passed.
  `qt_agent_report_view` with main's `agent_report_view.cpp` fails on "with
  no report the byline is empty, with no link in it".
- AC4: `ctest --preset dev -j 4`, 1098 rows, run in two passes because the
  disk filled while linking: the second pass ran the rows the first had not
  reached and every row it failed. Every row has a result. Still failing:
  - `qt_workbench_conversation_reveal`: the head pane's edit rows are 14 px
    against the comparison's 15 px, and line 6 sits at 75 px against 80 px
    ("the edit keeps the head pane's row pitch"). Fails every run, alone and
    in the suite. Cause not found; not test-only on its face.
  - `application_large_review_responsiveness`, `_launch`, `_active_work`,
    `_pathological`, `_history`: Debug timing budgets, nongating by policy
    (AGENTS.md "Debug timings"). `_cache_bytes` skips.
  - `documentation_structure` and `verify_documentation.py`: the lane's
    `docs/issues/NEXT_ID` (BPR-2395) is behind main's high water (BPR-2403
    at the time); clears when the lane takes main.
  - `documentation_issue_graph_contract`, `issue_id_allocation_contract`:
    need the MASK-deployed `.agents/skills/implement-rfc` in the checkout; both
    pass with it present.
  - The first pass also failed some 40 rows whose executables had not linked
    (`ld: write() failed, errno=28`); all passed in the second pass.
- Format: `format_check.py` (clang-format 22.1.8) passed on the four touched
  C++ files. `tidy.py` over the four units reports nothing on a changed line.
  `tests/scripts/verify_documentation_tests.py`: 82 tests OK.

The test executables of this tree were stripped of local symbols
(`strip -x`) to fit the disk; the app binary was not. `live_mutation_isolation`
and `composed_application_hosts`, which read symbols, passed.
