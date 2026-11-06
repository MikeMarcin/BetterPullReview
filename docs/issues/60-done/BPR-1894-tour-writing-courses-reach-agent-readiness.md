---
schema: "implement-rfc.card.v1"
id: "BPR-1894"
title: "The tour writing and composition courses reach agent readiness after BPR-1881"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"BPR-1891 implementer, 2026-11-06","related_cards":["BPR-1881","BPR-1891"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5c774dbc9","evidence":{"kind":"command","detail":"ctest --preset dev -R 'application_review_tour_(composition|writing|cold)': application_review_tour_composition_public_path, its scale_125 and scale_200 rows (the same course, which failed the same way), application_review_tour_writing, application_review_tour_writing_wrapper and application_review_tour_cold_import_public_path all pass on macOS Debug. Each of the first five prints agent-setup:checked: the Tour tab showed Set up the agent, it opened the setup at Check it works, Check passed on the stub claude and the Tour tab's readiness became ready. Only test files changed; review_tour_editor, agent_tab_view and workbench_window are untouched."}},{"criterion":"AC2","outcome":"pass","revision":"5c774dbc9","evidence":{"kind":"command","detail":"application_review_tour_writing prints review-tour-writing:pass, which it reaches only after the PLUGIN_CANARY step and the diagnostic-log check for 'stderr_bytes=0 plugins=cc-plugin-sec-default@builtin' with no '/fixture/'."}},{"criterion":"AC3","outcome":"pass","revision":"5c774dbc9","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK (PYTHONPATH at the main checkout's .agents/skills/implement-rfc/scripts, which the worktree lacks). coding-format-check passes (clang-format 22.1.8)."}}]
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

## Result (2026-11-06)

- `tests/end_to_end/stub_claude_search.hpp` puts a stub agent on a search
  `PATH` of the course's own as `claude`, with no usual folders, so the
  developer's own `claude` is never found.
- `agent_ready` in the composition course drives the setup the app uses: the
  Tour tab's **Set up the agent**, then **Check** at Check it works. The
  scoped writing stub now answers `auth status --json` (signed in) and
  `--version`.
- The default course, which has no agent argument, gets the stub through
  `BPR_TOUR_COURSE_CLAUDE` when agent integrations are built. It also offers
  the built-in Claude on that stub, as the writing course does. With nothing
  offered the Agent tab counts as not set up, even at the setup's Ready step.
- The wrapper course takes its stub off `PATH` once setup has finished, so its
  "no claude found" control still finds none.
- Not this card: `application_review_tour_publication_maintenance`,
  `_reconciliation` and `_manual` fail in this tree on provider-recording
  steps ("published reorder offers explicit update preview" and others). They
  use no agent feature, and this change does not touch their target.
