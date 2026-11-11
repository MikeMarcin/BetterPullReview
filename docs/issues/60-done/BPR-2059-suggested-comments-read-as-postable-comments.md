---
schema: "implement-rfc.card.v1"
id: "BPR-2059"
title: "Suggested comments read as postable comments, with an editable format prompt"
status: "Done"
rfcs: ["RFC-BPR-0092", "RFC-BPR-0107"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2058--BPR-2059--bd1353aea511.json","origin":"owner report, 2026-11-10 (screenshot of a private repository; nothing of it is kept)","related_cards":["BPR-1861","BPR-2051","BPR-2061"]}
depends_on: []
acceptance: [{"criterion":"Scope 1-5","outcome":"pass","revision":"e4aeb679e","evidence":{"kind":"command","detail":"Focused set at 52e085d57 and qt_workbench_window_scale_100/125/200 test_a_reply_drafts_all_its_suggested_comments (fence rendered as one fixed-pitch block)"}},{"criterion":"Scope 6","outcome":"pass","revision":"02200853d","evidence":{"kind":"observation","detail":"Owner accepted BPR-2059-prompt-default (session fbdd40b2, decision 1342b7f4) and BPR-2059-code-blocks (session 5ea0ce22c14ed5083b9d2e7382717df9, decision 39e0cbaa-e39b-4af0-8942-f6cce2f2767a)"}}]
---

# Suggested comments read as postable comments, with an editable format prompt

## Origin

The owner, 2026-11-10, of a finding card's suggested comment: "just a big
wall of text". From the owner's screenshot (a private repository; its text
and image are not copied anywhere, and the tests use synthetic text):

- inline code showed raw backticks instead of rendering as code;
- the whole suggestion paragraph was italic;
- the header chips were cut ("possible · correctn", "mainta") because the
  file path squeezed them;
- summary, reasoning and suggestion ran together with no spacing;
- an internal line, "Continuation Replay omits 0 retained turns and cuts 0
  text bytes; earlier omissions are unknown.", showed in the reply;
- the agent restated its reasoning in the suggestion and led it with
  "suggestion (non-blocking):" though the card already shows severity.

## Scope and acceptance

1. A finding card renders its claim, evidence and suggested comment as
   Markdown (code spans, fences, line breaks), in separate spaced blocks; the
   suggestion is not italic.
2. The severity and kind chips keep their whole text; the file path elides
   or wraps instead.
3. The replay status line is not shown to the user; it stays available to
   diagnostics.
4. Agent settings › Built-in prompts gains an editable suggested-comment
   format: a new purpose ID in the scheme of `enhance.*`, a built-in default
   and version, and Reset. Its default asks for short, postable comments: one
   sentence on the problem then the fix, code in code spans, no severity
   prefix, no restated reasoning. Every Agent tab request carries it.
5. The path from reply to draft keeps the suggestion's Markdown (code spans,
   fences, line breaks).
6. Owner look on a frozen build: Accept or Reject the finding card's
   formatting (before and after), and the format prompt's default.

Load frontend-design before the Qt work.

## Implementation (2026-11-10, `52e085d57`)

Frontend-design was loaded before the Qt work.

- Card (`agent_tab_view::make_card`): claim, evidence and suggested comment are
  `markdown_body_view`s (the reply's own renderer: code spans, fences, line
  breaks), each fitted to the card's width, in spaced blocks; the suggestion is
  upright in an inset with a quiet "Suggested comment" caption, and a leading
  Conventional Comments label is shown once beside the caption rather than as
  text (the draft still opens with it chosen). Severity and kind keep their
  width; the anchor elides in the middle (`eliding_anchor`).
- The replay counts left the continuation line in the Agent tab and the tour
  chat; they stay as `replayTurnsOmitted`/`replayTextCutBytes` properties, and
  the tooltip names them only when a replay left something out. The typed
  diagnostic log carries no free text, so no new diagnostic event was added.
- Writing hook `suggested_comment`: purpose `review.suggested-comment`, version
  `suggested-comment-v1`, default
  `src/application/resources/review-preparation/suggested-comment.md`, listed
  under Built-in prompts › Agent tab with Reset; Test is off for it (it writes
  no passage). Profile schema 26 (`writing_suggestion_format`; the schema test
  bans "comment" in column names). `bpr tour hooks … suggested-comment`.
- Every Agent tab brief carries `brief.comment_format.{source,version,text}`
  (`core::supplement_brief::CommentFormat`, bounded and validated). The review
  skill is bpr-version 5: it follows that format and no longer asks for a
  Conventional Comments header itself (amends BPR-1861 design point 1; the
  label reader is unchanged).
- Reply → draft keeps Markdown: `core_agent_comment_suggestion`
  `test_markdown_survives_into_the_draft` (code span, fence and line breaks,
  with and without a label).

## Verification

Focused set at the candidate, 29 rows, all pass: `core_agent_draft_all`,
`core_agent_comment_suggestion`, `core_review_skill`, `core_review_preparation`,
`core_agent_finding_promotion`, `core_agent_answer_findings`,
`application_writing_delivery`, `application_agent_launch`
(`brief.comment_format.*` encoded), `application_review_skill_host`,
`sqlite_workbench_settings_storage` (25 → 26 migration, override survives),
`qt_settings_window` and its scales (list, default, version, override, Reset),
`qt_workbench_window_scale_100/125/200` (Markdown, no raw backticks, upright,
narrow card keeps kind whole and elides the anchor, no "Replay omits"),
`qt_composed_github_scale_*`, `qt_agent_launch_panel`, `qt_agent_setup_view`,
`qt_side_chat_panel`, `qt_finding_queue_view`, `qt_enhance_control`,
`qt_agent_activity_view`, `qt_tour_side_chat`, `architecture_boundaries`,
`application_review_round_trip_composition_contract` (pin resealed). Also
`application_composition_source_contract`, `test_network_isolation_manifest`,
the format check on changed files and the documentation gate.

Not run (disk): the tour-cli preset (`tour_cli_composition`, edited to list
the ninth hook in `a823552f6`), the e2e `application_agent_*` courses (the
brief assertion added to `agent_invocation_composition_public_path`), the
core-only preset and `coding-tidy`.

## Owner look (pending)

Session `fbdd40b2768761881d2a420844cb4ec2` (see BPR-2061), candidates
`BPR-2059-card-formatting` (before/after renders of the same synthetic
findings from the Qt harness, and the live frozen build) and
`BPR-2059-prompt-default`.

## Owner hand-back, session `fbdd40b2…` (2026-11-10)

Read with `attended_review.py status` (sequence 23); host 7089/7121 stopped,
root `suggested-comments-v001.irz2s4en` removed.

- `BPR-2059-card-formatting`: **Reject** (`fdadc56d`): "You first screenshot
  shows ```cpp which doesnt look like it's rendering the markdown correctly."
  A fence with a language must render as a code block.
- `BPR-2059-prompt-default`: Accept (`1342b7f4`). Scope 6's prompt half passes.

## Code blocks after the owner's look (2026-11-10)

The first image of `BPR-2059-card-formatting` was the "before" render (the
build before this card), which showed the fence as text; the "after" render
already drew it as a code block, but offscreen, in a fallback face that did
not read as code. Rendered natively (Cocoa, QMacStyle, Dark, off screen) the
suggestion's ```cpp fence is a code block in the code font. Now pinned:
`qt_workbench_window_scale_*` asserts no fence or language word is shown and
the fenced line is one block in a fixed-pitch font
(`test_a_reply_drafts_all_its_suggested_comments`). Refiled with native
renders only.

## Owner hand-back, session `5ea0ce22…` (2026-11-11)

Read with `attended_review.py status --after 18` (handed back at sequence
49); host 78319/78332 stopped, root `owner-look-v003.w0k3mfr3` removed with
`freeze_attended_build.py remove`.

- `BPR-2059-code-blocks`: Accept (`39e0cbaa`). The fenced code block renders
  as code; scope 6's formatting half passes and nothing else is open.
