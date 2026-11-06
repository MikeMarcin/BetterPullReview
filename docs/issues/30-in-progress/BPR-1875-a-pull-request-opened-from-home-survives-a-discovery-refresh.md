---
schema: "implement-rfc.card.v1"
id: "BPR-1875"
title: "A pull request opened from Home is not cancelled by a discovery refresh"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05, split from BPR-1873"}
acceptance: []
---

# BPR-1875: A pull request opened from Home is not cancelled by a discovery refresh

## Observed

A PR open started from Home is cancelled a few seconds after a discovery
refresh starts. It restarts only when that refresh's long retained read
finishes, despite `home_loading_yields` / `yield_discovery`.

## Change

- Find who cancels the open and stop it: the open must keep running (discovery
  yields to it, not the reverse).
- Log the cancelling cause on `github.pull_request.cancelled`.

## Acceptance

- AC1. On fixtures, an open started from Home, then a discovery refresh,
  completes without cancellation or waiting for the refresh's retained read.
- AC2. Every `github.pull_request.cancelled` names its cause.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.
