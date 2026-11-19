---
schema: "implement-rfc.card.v1"
id: "BPR-2274"
title: "A tour is only its stops: Problem and Solution are removed everywhere; a shipped prompt drafts the pull request description"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"change","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2274--BPR-2274--fddc088f0e78.json","decided_by":"owner, 2026-11-18"}
---

# BPR-2274: A tour is only its stops

## Owner decision (2026-11-18)

"I agree with the no fixed sections. I think the tour just becomes the tour
stops. That streamlines it ... gives back some space and complexity on the
tour tab." "Tours right now have a message saying problem and solution in
description ... get rid of that language and just have the tour focused only
on stops." "It's also part of the cli."

## Change

Remove the tour's Problem and Solution (the tour brief) from core, application,
Qt, publication, CLI and the pr-tour skill. Publish posts the stops only and
never writes or edits the pull request description. A new shipped prompt,
"Draft a description", drafts the pull request description from the diff and
the tour's stops when there is one.

## Acceptance

- AC1. No Problem/Solution field, step, editor, chat, hook, CLI command or
  publication text remains outside migration code and its tests.
- AC2. Publish never touches the pull request description; the "problem and
  solution are in the pull request description" line is gone.
- AC3. (Revised by the owner, 2026-11-18: "there are no existing tours with
  Problem/Solution text anywhere ... just remove the fields", then "tours with
  Problem and Solution do exist, but when published that text lives in the
  pull request description, so it is already safe on GitHub".) No
  text-preserving migration: a stored draft's Problem and Solution are read
  and dropped, never failing the load. Reading or reconciling an
  already-published tour whose description holds an older build's brief and
  whose comment holds the old "problem and solution are in the pull request
  description" line ignores both, turns neither into a stop, and never edits
  or strips the description (test).
- AC4. "Draft a description" is a shipped prompt that returns a description
  from the diff, with and without a tour (fixture agent test).
- AC5. Focused tests, architecture_boundaries, coding-format-check and the
  documentation gate pass.

## Decisions

- No text-preserving migration (owner, AC3). Stored Problem and Solution are
  read and dropped: the local draft's JSON skips its old `problem`,
  `solution`, brief proposals and brief conversations; the tour codec is
  version 13 and reads a version 12 or earlier draft's Problem, Solution and
  their baselines without keeping them, and drops a conflict on either.
- Publication posts the stops only. The review body is the fixed line
  `core::tour_comment_lead` (a review needs a body); the plan says "The PR
  description will not be edited." and has no description step, option or
  flag. The description's text, an older build's brief included, is only
  read for the tour's link, never written.
- Prompts. The Problem and Solution hooks are retired: the writing hooks
  renumber (outline and comment first, `writing_hook_count` 16), the
  profile's settings columns for them (15 and 16) stay and are written empty,
  and the agent setup's hook tables move their rows down by two. The
  review-feedback hook is `review-feedback-v4`. The tour-part prompt place,
  subject and surface are gone; a queued prompt's wire numbers keep their
  meaning.
- Agent schemas: `bpr.tour-draft.v2`, `bpr.tour-ask.v2`,
  `bpr.tour-revise.v2`, `bpr.tour-guide.v3`, `bpr.tour-context.v2`. The
  owner channel is protocol 9.
- CLI. `show`/`edit problem|solution`, the Problem and Solution assist hooks,
  `--description` and `--no-description` are removed and refused with "Use
  stop:ID". `cli_contract` stays 1: the change removes commands, so a caller
  of the removed forms gets a refusal, not a different meaning.
- "Draft a description" (`agent-tab.draft-description`) is a shipped Agent
  tab prompt about the pull request, in the pull request's Ask menu. It is
  not a default chip: a third chip under an empty thread would stack three
  buttons (RFC-BPR-0085). It drafts or rewrites the description from the
  diff and the tour's stops, as plain text; nothing is published by it.
- Courses that used the Problem or Solution only as a vehicle for other
  behaviour were moved to a stop where that was direct; the rest were
  trimmed and are [[BPR-2275]].
- The owner protocol is 9 wherever it is stated (`bpr cycle capabilities`,
  the CLI pages, the threat model, the cycle security course's stand-in
  owner). The review-feedback guidance lost its problem-statement and
  solution-summary words after [[BPR-2276]] shipped `review-feedback-v3`, so
  it is `review-feedback-v4`.

## Verification

macOS, dev preset, on main `7d2c37659` plus this change:

- Build of every target; `coding-format-check`; `verify_documentation.py`.
- 355 focused rows (tour, prompt, writing, hook, settings, workbench window,
  quick ask, chips, SQLite, every `core_` row, fixture agent, skill, local
  draft, review cycle, profile activation, Enhance, Ask menu and
  `architecture_boundaries`): all pass but two below. Among them
  `core_review_tour_import` (AC3: a published tour whose description holds an
  old brief and whose comment holds the old note loads as its stops, plans
  no write and leaves the description alone), the publication courses
  (AC2: no description write in any course), `fixture_agent_answers` and
  `core_prompt_macro` (AC4), and `skill_pr_tour_package`.
- Not this change: `application_review_cycle_parity_public_path` fails on
  its fixture's replacement text (`retur kNoValue`);
  `qualification_case_inventory_contract` and
  `command_correctness_registry_contract` fail because [[BPR-2262]] added a
  workbench case without the inventory; `test_network_isolation_manifest`
  names `sqlite_prompt_pack_storage` and
  `application_agent_chat_held_turn_public_path`, which this change does not
  register.
