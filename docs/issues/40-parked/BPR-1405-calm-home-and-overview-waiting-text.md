---
schema: "implement-rfc.card.v1"
id: "BPR-1405"
title: "Calm the waiting and merge-requirement text on Home and Overview"
status: "Parked"
rfcs: ["RFC-BPR-0069", "RFC-BPR-0070"]
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1405--BPR-1405--0e43de1c0fb9.json","origin":"owner request 2026-10-12"}
parked_on: "human-review"
---

# Calm the waiting and merge-requirement text on Home and Overview

The owner, 2026-10-12: recent changes made the home screen and the overview
page "flood with text and generally less elegant, mostly related to required
reviews from code owners". The merge-gates pack (BPR-1394, BPR-1397) made each
fact a sentence and joined them all. A Home row for the three-team example
reads its reason, every team, a people/team count, "Required review
outstanding" and each failed check on one wrapping line. Overview's
**Merge requirements** stacks the merge assessment, review state, approval
minimum with its caveat, each missing and failed check, and a comma-joined
roster with team display names, as one paragraph above Participants.

## Acceptance

- AC1: A Home row's reason says who or what is awaited once. Named reviewers
  replace the generic "waits on reviewers" clause and the people/team count;
  the aggregate "Required review outstanding" appears only without names.
  Names and failed checks are bounded with "and N more", the complete roster
  being in Overview. The RFC-BPR-0070 example still reads "Waiting on
  code-owner review from …; Branch Freshness failed".
- AC2: Waiting on others' collapsed summary no longer repeats that its rows
  await review.
- AC3: Overview's merge section leads with a short, toned assessment and lists
  each unmet condition on its own short line. Evidence-limit caveats (approval
  minimum is not a remaining count, merge permission not established) stay in
  the details disclosure. Outstanding requests appear as a roster, one
  reviewer per line, code owners marked, not a comma-joined paragraph.
- AC4: RFC-BPR-0069 AC01–AC06 and RFC-BPR-0070 AC01–AC05 facts remain
  visible and accessible: every returned request, all three code-owner teams,
  the blocked assessment, failed and missing required checks, stale and
  unavailable states. Existing focused suites pass with updated wording.
- AC5: The owner accepts the calmer Home and Overview look (attended).

## Evidence

Candidate `06f2e5dff` (product) and `7011e21b3` (attended stop in the Home
waiting course). The frontend-design skill was loaded for this work.

- AC1, AC2: `qt_review_landing_model` pins the RFC-BPR-0070 example sentence
  exactly, the bounded "and N more" form, discovery's person/team counts, the
  aggregate "Required review outstanding", and a non-authored row that keeps
  its failure without its author's roster. Waiting on others' summary keeps
  only the blocked count.
- AC3: `application_pull_request_overview` pins the toned state, one
  condition per line (review with configured minimum, failures before
  missing results), the empty complete roster, and the caveats moved into
  Requirement details. `application_overview_summary_{1,1_25,2}` composes the
  three-team scenario: Blocked, three short lines, five roster rows with code
  owners first and team names in the accessible name, keyboard details, and
  narrow enlarged rows that fit with middle elision.
- AC4: 52/52 of `overview|landing|home|personal_discovery|composed_github|workbench_window|review_queue`
  pass on the dev build at `7011e21b3`; format gate passes; clang-tidy on the
  three changed sources reports only `remainder_path`, pre-existing and fixed
  on main by `6673a3be3`. `git merge-tree` with main is clean.
  `verify_documentation.py` passes.
- Offscreen renders (light/dark/high contrast, 1100/520 Home, 1280 and
  enlarged 900 Overview) were inspected before and after. The 520 Home render
  in the course grabs before the list relays out; with events settled the
  row wraps correctly. That is the course's timing, not the product.

## Owner acceptance

AC5: the owner accepted both looks on 2026-10-12 in attended-review session
`5cbc651b593c9deafbab6516ef62ecdd`, a fresh short session, against the frozen
build of `7011e21b3`: Home (decision `9ef9f86c`, fingerprint `f53b284b63f5`)
and Overview (decision `53c3ea12`, fingerprint `b31387aaae9d`), no notes.
The frozen root was removed afterwards.
