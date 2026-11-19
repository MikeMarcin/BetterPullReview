---
schema: "implement-rfc.card.v1"
id: "BPR-2280"
title: "Draft a description writes a pull request description, and the Author tab can edit the description"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2280--BPR-2280--3417485e427c.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2274","BPR-1774","BPR-2196"]}
---

# BPR-2280: Draft a description writes a pull request description

## Owner (attended review, 2026-11-19)

Rejecting BPR-2274's "Draft a description" look: "That's not working at all.
It looks like it tried to make a comment not draft a PR description. Also
seems like we can't edit the PR description from Author tab which seems like a
gap."

## Change

- "Draft a description" asks for a description: its result is a pull request
  description (title-free Markdown: summary, what changed, how to test), the
  answer is presented as a description and never offered as a review comment,
  and the fixture agent answers it with a description.
- Author: the author reads and edits the pull request description in the
  Author drawer's Workflow tab and saves it to GitHub through the existing
  description mutation ([[BPR-1774]]). The draft's answer offers "Use as
  description", which fills that editor; nothing is published until Save.
  A description changed on GitHub since it was read is refused and offered
  for reload; it is never overwritten.

## Acceptance

- AC1. The shipped prompt's ask kind is a description; such a question is
  sent without the comment-label guidance and its answer offers Use as
  description, not Draft (core/application tests).
- AC2. Fixture-agent course: Draft a description returns a description with
  summary, what changed and how to test; no Draft comment; Use as
  description fills the Author editor; Save sends the description mutation
  through the fixture transport; a stale description is refused.
- AC3. Qt test for the Author description editor.
- AC4. architecture_boundaries, coding-format-check and the documentation
  gate pass.

## What was wrong

BPR-2274 shipped "Draft a description" as an ordinary pull request
*question* (`prompt_ask::question`). Everything downstream then treated its
answer as an Agent tab answer, which in this app is comment-shaped:

- The request carried the Comment labels Feature (`PostedComments` was true
  for every Agent tab turn), telling the agent how "each comment you write
  for the reviewer to post begins".
- The answer's one keep action was **Draft** (a draft comment where the
  question was asked), and a Markdown answer with two headings or more
  became a report card whose page offers Keep summary and Draft comment.
- The prompt asked for "no fixed sections", so a description had no shape a
  reader recognises, and the fixture agent's canned answer was two loose
  paragraphs with the "_Fixture agent: a canned answer_" line, which read as
  a reply.
- Nothing in the app could take the text as a description: the only editor
  was Overview's ([[BPR-1774]]); Author had none.

## Decisions

- A third ask kind, `prompt_ask::description` (stable name `description`,
  `prompt_ask_count` 3), allowed only in the Agent tab about the pull
  request. The prompt pack, the profile store and the queued-prompt codec
  read it; Settings › Prompts offers it as **Answer as: Description**.
- `decide_agent_question` marks a description suggestion's question
  `Description`; `application::agent_turn_audience` gives such a turn the
  Writing style and no Comment labels.
- The Agent tab: a description turn is never a report and offers **Use as
  description** in Draft's place (disabled, with the reason, for a viewer who
  cannot save the description); it never offers Draft or Keep local.
- The prompt asks for title-free Markdown under `## Summary`,
  `## What changed`, `## How to test`, keeping an existing description's own
  structure, with no preamble, no findings and no comment label.
- Author's Workflow tab holds `qt::author_description_editor` above the
  checklist for a pull request (not a local draft, whose description is its
  own): the description read through the app's Markdown reader, Edit/Write,
  the Markdown editor, Save, Cancel. Save is the same gesture and write as
  Overview's ([[BPR-1774]], `updatePullRequest` body through the existing
  review-mutation host; no new mutation). The composition routes the answer
  to the editor that saved. A body changed on GitHub meanwhile is refused
  ("Someone changed the description on GitHub since you began. Nothing was
  saved; reload it to edit from GitHub's text.") with **Reload from GitHub**;
  the typed text is one Undo away.
- Use as description switches to Author, Workflow, and fills the editor as
  one undoable edit; nothing is sent until Save.
- Enhance on the description editor was not added: the shared
  `enhance_control` needs a new surface and its own agent request wording
  through the composition, which is not cheap here.

## Verification

macOS, dev preset, lane `claude/v0-5-0-prerelease-targets-52265d` on
`771dce076` plus this change. Full dev build. `coding-format-check` passed;
`verify_documentation.py` DOCUMENTATION_OK.

33 of 34 focused rows pass: `architecture_boundaries`, `core_prompt_macro`
(AC1: kind, allowed places, the question's Description, the template's three
parts), `core_agent_queue` (a queued description keeps its kind),
`application_writing_delivery` (AC1: no Comment labels on a description),
`qt_author_description_editor` (AC3), `application_draft_description_public_path`
(AC2: Ask menu → Draft a description → a title-free three-part answer, no
Draft, no report, Use as description → Author Workflow editor holds it →
Save writes once through the fixture's GitHub and the next read shows it →
a description changed on GitHub is refused, nothing written, Reload edits
from GitHub's text), `fixture_agent_answers`, `application_fixture_agent_every_ask`,
`application_description_edit_contract`, `qt_pull_request_overview_page`,
`qt_workbench_window_scale_100`, `qt_prompts_page`, `qt_prompt_macro_chips`,
`qt_ask_menu`, `sqlite_prompt_pack_storage`, the prompt, quick-ask,
author-mode and agent-queue rows, `application_agent_keep_action_public_path`,
`application_ask_menu_public_path`, `application_commit_asks_public_path`,
`application_agent_chat_composition_contract`, the local-draft author and
workflow courses.

Not this change: `test_network_isolation_manifest` still names
`sqlite_prompt_pack_storage` and `application_agent_chat_held_turn_public_path`
(recorded by [[BPR-2274]]); this change's own new row is entered.

No live GitHub write was made: the course's writes go through the fixture
transport. No attended look was prepared here.
