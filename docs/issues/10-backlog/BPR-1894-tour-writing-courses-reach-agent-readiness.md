---
schema: "implement-rfc.card.v1"
id: "BPR-1894"
title: "The tour writing and composition courses reach agent readiness after BPR-1881"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"BPR-1891 implementer, 2026-11-06","related_cards":["BPR-1881","BPR-1891"]}
acceptance: []
---

# BPR-1894: The tour writing and composition courses reach agent readiness after BPR-1881

## Observed

Since BPR-1881 (`101c59fe2`), Tour tab agent features stay disabled until
agent setup is ready. The end-to-end fixtures never reach ready: the Tour
editor's readiness stays `unfinished`. So three courses fail on main before
they reach their own checks:

- `application_review_tour_writing`, at "recovery gesture accepted";
- `application_review_tour_writing_wrapper`, at "tour saved through admitted
  SQLite store";
- `application_review_tour_composition_public_path`, at "Problem Tab reaches
  the visible assistance control".

With readiness forced to ready, both writing courses pass, including
BPR-1891's plugin step.

## Change

Bring these courses to a real ready state, the same way a person does: a
stub agent that the setup check accepts, driven through the setup path the
app uses. Do not bypass the BPR-1881 gate in product code. Prefer a fixture
helper the courses share. If any other course has the same failure, fix it
too.

## Acceptance

- AC1. All three courses pass on main, with BPR-1881's gate unchanged.
- AC2. BPR-1891's plugin step and its `plugins=cc-plugin-sec-default@builtin`
  log check run and pass inside `application_review_tour_writing`.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.
