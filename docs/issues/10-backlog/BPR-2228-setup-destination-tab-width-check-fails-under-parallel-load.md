---
schema: "implement-rfc.card.v1"
id: "BPR-2228"
title: "The setup-destination course's code-font tab-width check fails intermittently under parallel load"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"kind":"test-flake","found_by":"BPR-2216 regression run, 2026-11-16","reservation":"docs/issues/_reservations/BPR-2228--BPR-2228--7e08cabb0315.json","related_cards":["BPR-2216"]}
---

# BPR-2228: The setup-destination course's code-font tab-width check fails intermittently under parallel load

## Observed

`application_setup_destination_public_path` failed once, Debug, on the lane
`claude/v0-5-0-prerelease-targets-52265d` at `648aa4f30`, while it ran in a
`ctest -j4` batch with eight other composed GUI courses on a loaded machine:

```
PROGRESSIVE_OVERVIEW first_useful_ms=23 terminal_ms=50 course=recorded_cli_first_file_page_gate
FAILED: composed source layout uses the selected code-font tab width, not Qt's default
```

## Not caused by BPR-2216

Run alone, the course passed on the same tree with BPR-2216's product
changes removed (`c41707298` and `648aa4f30` reversed in the working tree),
and passed four times out of four with them in place. Each run measured
13.5 px against 13.5 px expected (monospace 9 pt, width 4) on all three
source panes. The failure needs a reproduction under load.

## Change already made

The check's failure message now names the pane, width, measured and expected
tab stops and the font, so the next failure says what it saw
(`verify_composed_code_metrics`, BPR-2216's commit).

## Acceptance

- **BPR-2228:cause.** The failure is reproduced under parallel load and its
  cause recorded (a font or appearance gesture racing the check, or a product
  ordering problem).
- **BPR-2228:fixed.** The course waits for the condition it checks, or the
  product applies the tab width before it reports the gesture done; twenty
  runs under `ctest -j4` beside other GUI courses pass.

## Round 2026-11-17: reproduced, cause narrowed, not fixed

Left in Backlog: neither acceptance item is met.

- Lane HEAD failed every run, alone, before reaching the tab check: the
  course waited for "Provisional metadata", which [[BPR-2216]] round 4
  (`e5e50701f`) took out of the overview summary. Fixed in `7ffff2638`, as
  `qt_composed_github` was in [[BPR-2240]].
- Reproduced by running the course binary directly eight (or four) at a
  time beside `ctest -j4` loops of other composed GUI courses (load averages
  13 to 86). Across about 330 such runs: the tab check failed 7 times and a
  second check, "accepted exact setup opens the recorded pull request",
  failed 9 times. `ctest -j4` of the course itself, 20 runs beside that
  load, passed 20 of 20, which is the original acceptance wording but not
  evidence of a fix.
- **Tab width.** Every miss is the third start in the course (the remounted
  recovery after the progressive-overview step), never the first two. All
  four panes stop at 6.75 px against 13.5 px expected (monospace 9 pt): a
  width of 2, the first width the course's gestures chose (2, 8, then 4).
  Settings in that window shows 2 as well, so the host's restored record is
  2; it is not a presentation lag (waiting 15 s changed nothing). Traced
  runs (a temporary print of every adopt, store and load in
  `appearance_preferences_application_host`) wrote 4 last every time, and
  the second start loaded 4; tracing also stopped the miss from appearing in
  156 runs, so the writer of the 2 is not yet seen. One loss path found and
  tested but not shown to be this one: `shutdown_now()` cancels a settings
  store still queued on `AppearanceEffects` (which also runs syntax
  classification), so a setting chosen just before the profile closes can
  be lost; letting the newest store run at shutdown (red/green in
  `application_appearance_host`) did not stop the miss (2 of 40), so it was
  not committed.
- **Accepted setup.** Every miss is the Enterprise half: after the final
  `connect_native`, the session stays "No GitHub host is connected." with
  the account setup visible at `choosing_server`, Connect enabled and no
  account made (`AccountFactoryCalls` 0). Waiting for Connect to be enabled
  and for no connection to be establishing before pressing it made it rarer
  but did not stop it, so that wait was not kept.
- The course now prints both states when it misses (`7ffff2638`).

Next: find who adopts a width of 2 on the third start without changing the
timing (for example, record adopts in memory and print them only on a
miss), and follow the Enterprise connect gesture after a replaced target.
