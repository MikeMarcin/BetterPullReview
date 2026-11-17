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
