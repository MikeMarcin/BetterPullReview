---
schema: "implement-rfc.card.v1"
id: "BPR-1876"
title: "A failed agent answer offers Ask again, and says when its cause is fixed"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: confused by a failed answer's Check again button after the agent setup check passed"}
acceptance: []
---

# BPR-1876: A failed agent answer offers Ask again, and says when its cause is fixed

## Observed

The owner, 2026-11-05. An agent question failed ("Stopped after 0:19"). The
failed answer card, saved in the agent tab, carries a **Check again** button.
Pressing it re-runs the agent setup check (the green "<agent> is ready" box
refreshes and passes) but does not re-send the question, so the red failed
answer stays, still showing its old cause. The owner read the red box as the
check result and concluded the check still failed.

## Change

- A failed answer's action is **Ask again**: it re-sends the same question
  (same prompt, same context) as a new run. Keep any setup re-check as a
  step that run performs, not as the button's whole effect.
- Once the cause the failed answer names is no longer true (the setup check
  passes, or the workspace cause is gone), the failed answer says so, e.g.
  "Fixed since. Ask again", instead of presenting the old cause as current.
- The setup check's own control keeps its own wording and is visually
  distinct from a failed answer.

## Acceptance

- AC1. Pressing a failed answer's action starts a new run of the same
  question; covered by a Qt or public-path test.
- AC2. After the cause clears, the failed answer reads as fixed-since with
  Ask again; while the cause holds, it shows the cause.
- AC3. The owner accepts the look (attended review).
- AC4. `python3 scripts/verify_documentation.py --root .` passes.
