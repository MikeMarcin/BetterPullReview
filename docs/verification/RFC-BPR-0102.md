# RFC-BPR-0102 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `agent-tab-asks`
([[BPR-2002]]). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.
Frontend-design was loaded before the Qt surface work. Debug timings are
nongating; nothing below is a timing claim.

## What was built

The core decides each question's subject and entry point
(`decide_agent_question`, `src/core/include/bpr/core/agent_question.hpp`):
a conversation chip makes the conversation the subject; a suggestion names its
own subject (Review this pull request: the pull request with `run_ai_review`;
Review *file*: that file; Explain lines: the selected lines); a typed question
is about the attached lines when the chip shows, otherwise about the pull
request (`open_full_agent_session`, which needs no selection). The chat host
decides a question whose subject is not its lines without the selection, so
the core's `selection_required` cannot refuse it.

The supplement gains `subject`, `subject.statement` (the core's fixed
sentence), `subject.path`, the lines' side, numbers and text, `subject.file.N`
(every changed file, for a pull request subject) and, apart from it,
`looking_at.path`/`.side`/`.line`. For a pull request subject the open file's
receipt (`file.*`) and the selection are not sent. Nothing else is added;
launches that are not questions keep their bytes.

The scope list is gone from the Agent tab. Selected lines show as a chip
(`renderer.cpp 5–8`) whose close detaches them until a new selection; the
`+` tool and Ask the agent about these lines attach them again. One line under
the field says the agent can read the whole pull request. Escape in a code
pane closes an open find bar first; otherwise it clears the line selection
and the chip (`clear_diff_selection_intent`).

The core's File, Pull request and Repository scope levels are retired from
the Agent tab; [RFC-BPR-0012](../rfcs/RFC-BPR-0012-ai-review-chat-findings-mcp.md#question-subject-amendment-rfc-bpr-0102-2026-11-10)
records the amendment. The core scope types remain for the hidden panel.

## Public-path course

`application_agent_chat_subject_contract`
([source](../../tests/end_to_end/agent_chat_composition_public_path.cpp),
course `subject`): the composed application on the fixture review, the Agent
tab's own suggestion buttons, composer and Send, the diff's own panes, and a
fixture agent that reports the subject fields its frame carried.

```text
agent-chat-subject overview-review: BPR_SUBJECT_2002 subject=pull_request in_scope=1 path=absent lines=absent-absent looking=absent selection=0 files=0 entry=run_ai_review
agent-chat-subject selected-review: BPR_SUBJECT_2002 subject=pull_request in_scope=1 path=absent lines=absent-absent looking=review.cpp selection=0 files=0 entry=run_ai_review
agent-chat-subject typed-attached: BPR_SUBJECT_2002 subject=lines in_scope=absent path=review.cpp lines=61-61 looking=absent selection=1 files=1 entry=ask_about_selection
agent-chat-subject typed-detached: BPR_SUBJECT_2002 subject=pull_request in_scope=1 path=absent lines=absent-absent looking=review.cpp selection=0 files=0 entry=open_full_agent_session
agent-chat-subject typed-nothing: BPR_SUBJECT_2002 subject=pull_request in_scope=1 path=absent lines=absent-absent looking=review.cpp selection=0 files=0 entry=open_full_agent_session
agent-chat-subject:pass
```

The fixture pull request has one changed file, so `in_scope=1` lists every
changed file.

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `overview-review`: from Overview, nothing selected, the suggestion sends with subject `pull_request`, `run_ai_review`, no selection, every changed file listed, no single `file.*` | Pass |
| AC02 | `selected-review`: with lines selected and the chip shown, the same request; the selection stays in the diff | Pass |
| AC03 | Installed Claude on a 17-file fixture pull request with README.md open: the review discusses 13 other changed files; see [installed agent](#ac03-installed-agent) | Pass |
| AC04 | `typed-attached` carries the lines as subject; after the chip's close, `typed-detached` is about the pull request and the diff's selection remains | Pass |
| AC05 | With the find bar open, Escape closes it and the selection remains; Escape again clears the selection and the chip | Pass |
| AC06 | No scope list in the Agent tab; no enabled button says "Select the lines to ask about"; no refusal across five sends | Pass |
| AC07 | `typed-nothing`: a typed question with nothing selected is sent and answered | Pass |

Unit evidence: `core_agent_question` (subjects, entry points, bounds),
`core_diff_semantics` (the clear intent), `core_agent_context_supplement`
(member guard), `qt_workbench_window_scale_100` (the chip, its close, a new
selection attaching again, no suggestion that is a refusal),
`qt_agent_picker_wrap` (the field's row without the scope list).

## AC03: installed agent

Recorded by the opt-in `live-claude` course
([RFC-BPR-0101 ledger](RFC-BPR-0101.md#installed-claude-course)): with
README.md open, "Review this pull request" was answered by installed Claude
Code 2.1.319 (Haiku) with "This pull request has three defects that would stop
it building or render correctly…", naming 13 of the 16 other changed files.
Earlier runs named 9, 12 and 11. Before this card the same suggestion was
refused for want of a selection.

## Closeout (2026-11-11)

[[BPR-2003]] judged every criterion against the evidence above; all pass. Evidence reused unchanged; no product change in the closeout round. Not run: Windows and Linux, the whole dev suite. RFC-BPR-0102 is Auto-Verified.
