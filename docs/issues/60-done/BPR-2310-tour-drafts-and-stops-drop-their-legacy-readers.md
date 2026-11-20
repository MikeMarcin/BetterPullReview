---
schema: "implement-rfc.card.v1"
id: "BPR-2310"
title: "Tour drafts and published stops drop their legacy readers (Problem/Solution, Author's tour)"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"cleanup","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2310--BPR-2313--b73895df6230.json","origin":"owner, 2026-11-20: \"All, incl. notes check\" (vestigial items cut; no legacy support for old data)","related_cards":["BPR-2274","BPR-1806","BPR-2311","BPR-2312","BPR-2313"],"acceptance":["AC1","AC2","AC3"]}
---

# BPR-2310: Tour drafts and published stops drop their legacy readers

## Asked

Owner, 2026-11-20, approving the vestigial-item list: "All, incl. notes
check". The owner's standing policy: no legacy support for old data.

## What changed

- **Problem and Solution** ([[BPR-2274]] left readers for them). The local
  draft's preparation (`local_draft_tour.cpp`) and the workflow action record
  (`workflow_action.cpp`) no longer carry `LegacyProblem`, `LegacySolution`
  or their two revision counters, nor the `skip` hook that kept them out of
  the stored form. The four keys are named once each as `glz::skip{ }`, so a
  draft or record saved before BPR-2274 still loads (the store refuses
  unknown keys) and the values are dropped. A Problem or Solution chat or
  proposal in such a draft is still dropped by `retired_brief`, and a turn
  about either still counts as about the whole change: those are values, not
  fields, and without them such a draft would not load at all.
- **`### Author's tour ·`** headings. `map_tour_stop` and
  `recognize_published_tour_stop` accept only `### PR Tour · N — Title`. A
  stop published under the old heading is no longer a tour stop: it is an
  ordinary comment. Its tests (`legacy_stop_heading`, the legacy envelope)
  go; fixtures and contract tests that used the old heading as an ordinary
  stop now use the current one.

## Acceptance

- AC1. A stored preparation or workflow record holding `problem`,
  `solution`, `problem_revision` or `solution_revision` decodes, equal to the
  same draft without them, and the stored form never writes them.
- AC2. A body under `### Author's tour ·` is neither mapped nor recognized as
  a tour stop; `### PR Tour ·` bodies behave as before.
- AC3. Build, focused tests, `architecture_boundaries`, format and tidy on
  changed files, and the documentation gate pass.

## Evidence

Dev preset, macOS, worktree branch `claude/vestigial-cuts` on main
`c937eab1b`:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `core_local_draft_tour`: a preparation with the four keys inserted decodes equal to the one without; the stored form has no `"problem"` or `"solution"` | Passed |
| AC2 | `core_review_tour_markdown` (the retired heading is not a stop, by either reader), `core_review_tour_update`, `core_review_tour_import`, `core_feedback_tour_coexistence`, `github_paged_read`, `github_review_comment_permalink` | Passed |
| AC3 | `architecture_boundaries`; `format.py` and `format_check.py` (clang-format 22.1.8) on every changed C++ file; `verify_documentation.py` | Passed |
| AC3 | `tidy.py --warnings-as-errors` on every changed C++ file | No finding on a changed line. One finding predates this card: `workflow_action.cpp` `bugprone-branch-clone` in the check-gap chain (from `affe49d2d`), left as it was |

`qt_feedback_label_threads` and the composition fixture
(`github_public_path_fixture.cpp`) changed only a heading string; their
objects compile, and the suites were not run (the Qt adapter build did not
fit the shared disk).
