---
schema: "implement-rfc.card.v1"
id: "BPR-1935"
title: "A tour chat turn is refused as invalid_agent_context once its prompt passes the 16 KiB chat-message bound"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1914"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-07: a question in the Solution's side chat on a 12-stop tour ended with 'Refused before it started: invalid_agent_context'; Draft the tour on the same pull request had finished","related_cards":["BPR-0517","BPR-1900","BPR-1914","BPR-1916","BPR-1930"],"related_rfcs":["RFC-BPR-0012","RFC-BPR-0092"]}
acceptance: []
---

# BPR-1935: A tour chat turn is refused as invalid_agent_context once its prompt passes the 16 KiB chat-message bound


## Observed

The owner, 2026-11-07, a build of `main` at `07f6fac9f`. The pull request
under review changes 15 C++ files. Draft the tour had finished ("Proposal
done") with a Problem, a Solution and 12 stops. The owner selected a phrase
in the Solution and asked "What does this mean?" in the Solution's side
chat. The turn ended at once:

```text
Steps
  Ended   Refused before it started: invalid_agent_context
Didn't finish: Refused before it started: invalid_agent_context
```

No agent process started.

## Cause

1. `request_tour_ask` (`src/app/src/github_application/window_tours.cpp`)
   builds the turn's whole prompt with `application::build_tour_ask_prompt`:
   the pull request's title and description, every changed file with its
   head hunks, the whole draft tour (Problem, Solution, every stop), the
   conversation so far, the passage and the question. [[BPR-1916]]'s skill
   directive is then appended.
2. `dispatch_tour_run` passes that prompt to `dispatch_agent_invocation` as
   `aChatMessage` (`window_agent_chat.cpp`), which puts it in
   `agent_context_observation::ChatMessage`.
3. `build_agent_context_supplement` (`src/application/src/agent_launch.cpp`)
   refuses any `ChatMessage` longer than `core::maximum_chat_message_bytes`
   (16 KiB, `src/core/include/bpr/core/agent_context_supplement.hpp`) with
   `agent_context_failure::invalid_observation`. `core::valid_chat_message`
   makes the same check again in the core.
4. `agent_invocation_host` reports that as `invalid_agent_context`, and
   `tour_run_refused` writes it into the side chat.

The three bounds this path crosses don't agree:

| Bound | Value |
|---|---|
| `maximum_ask_prompt_bytes` (`tour_agent_conversation.cpp`) | 160 KiB |
| `maximum_draft_prompt_bytes` (`tour_agent_drafting.cpp`) | 96 KiB |
| `core::maximum_chat_message_bytes`, which both prompts actually travel through | 16 KiB |

The `ChatMessage` bound is the one that applies. Because the prompt
builders accept anything up to 160 KiB, `request_tour_ask`'s own refusal
("This question, with the pull request and the conversation so far, is too
large to send in one request.") never fires. The turn instead fails later
with a code the reviewer can't act on.

Draft the tour works for now only because its prompt is built while the
draft is still empty. Every chat turn sends the whole tour again, so the
prompt grows with each stop, every bit of stop text and each earlier turn.
A draft against a long description and many files can cross the same
bound.

[[BPR-0517]] sized `maximum_chat_message_bytes` for text a reviewer types
into the Agent tab's side chat. [[BPR-1900]] and [[BPR-1914]] reused that
field to carry a composed, app-built prompt. The writing-hook prompt
(`build_writing_prompt`) stays within the bound on purpose and checks it
itself, so it is not affected.

## Probe (2026-11-07)

This probe was added for a while to `tour_agent_conversation_tests` and
then reverted. It is shaped like the observed pull request but kept small:
15 modified files with three hunks each, a 2.5 KB description, Problem
600 B, Solution 700 B, 12 stops with 500 B bodies, a Solution passage, no
earlier turns.

```text
PROBE draft_prompt_bytes=7205 ask_prompt_bytes=16273 chat_cap=16384
```

Even this small version comes within about 100 bytes of the bound. The
observed tour, with longer stops, a longer description and one earlier
turn, goes over it.

## Change

### 1. Carry the tour prompt outside `ChatMessage`

Give a tour run's composed prompt its own carrier, separate from the typed
chat message. For example: a bounded `core::supplement_tour_request` (or a
`tour_launch::Prompt`), admitted only for a `ReadOnlyTools` tour launch and
encoded once at a documented position, in the way [[BPR-0517]] placed
`chat_message`. Its bound must be at least the builders' bounds (160 KiB)
and must fit inside `maximum_agent_input_bytes` along with everything else
the frame carries.

- `ChatMessage` and `maximum_chat_message_bytes` stay as they are for the
  Agent tab's side chat and the writing hooks.
- `build_agent_context_supplement` refuses a tour prompt on a launch that
  is not a tour run. A missing prompt is refused by name, not dropped
  silently, matching how a workshop operation that rides on another gesture
  is refused.
- The prompt's wire form is the same text the agent receives today, so the
  existing ask and draft answers decode unchanged.

To settle at pickup: whether the tour prompt should go without the review
`Brief` that `dispatch_agent_invocation` attaches to every non-writing,
non-workshop launch. The tour prompt already says what the brief says about
the pull request. Recommended: no brief on a tour run. Confirm with the
probe that each prompt still carries what the agent reads.

### 2. One bound per prompt, checked where it is built

`build_tour_ask_prompt` and `build_tour_draft_prompt` check the bound of the
carrier their prompt actually travels in. `request_tour_ask` then shows its
plain "too large to send" refusal when that bound is crossed. Add the
[[BPR-1916]] skill directive before that check, not after it.

### 3. A refusal that says what happened

If a supplement is still refused for size, the side chat says so in words,
not as `invalid_agent_context`. Map the size refusal to its own stable name,
for example `agent_context_too_large`, and to a sentence in
`tour_run_refused`. Keep the log line's code.

### 4. Tests

- `application_tour_agent_conversation`: a 12-stop tour with 15 files and an
  earlier turn builds an ask prompt larger than 16 KiB and within the new
  bound. A prompt one byte over the new bound is refused.
- `application_agent_launch` (or the supplement tests): a tour prompt over
  16 KiB builds a supplement for a read-only tour launch. The same prompt on
  a non-tour launch is refused. A `ChatMessage` over 16 KiB is still
  refused.
- `core_agent_context_supplement`: the new field's bound, its single wire
  position, and that it does not pass the private notes and drafts opt-in.
- A composed course (the tour chat course behind `qt_tour_chat_in_tab` or
  the `--attended-tour-authoring` offscreen course) sends a Solution question
  on a tour whose prompt is over 16 KiB. A stub agent receives the prompt
  exactly once and its reply lands in the side chat.

### 5. Documentation

- `docs/architecture/ai-agent-boundary.md`, the paragraph "The typed
  message rides the supplement, not a second channel": the tour prompt is a
  separate, app-built field with its own bound, and is never filled from
  typed text alone.
- RFC-BPR-0092's tour amendments ([[BPR-1900]], [[BPR-1914]]): record the
  carrier and its bound.

## Acceptance

- AC1. On a tour whose ask prompt is larger than 16 KiB (fixture: 15 files,
  12 stops, an earlier turn), a question in the Solution's, a stop's or the
  tour's side chat reaches the agent, and its answer appears in that chat.
- AC2. A prompt over the new bound is refused before dispatch, with
  `request_tour_ask`'s "too large to send" sentence in the side chat. No
  agent process starts.
- AC3. Draft the tour, against a pull request whose draft prompt is over
  16 KiB, finishes with a proposal.
- AC4. The Agent tab's side chat still refuses a typed message over
  `maximum_chat_message_bytes`, and writing hooks are unchanged.
- AC5. A tour prompt on a launch that isn't a read-only tour run is refused
  by name.
- AC6. No tour refusal shows `invalid_agent_context` for a size reason.
- AC7. The documentation above is updated, and
  `python3 scripts/verify_documentation.py --root .` passes.
- AC8. `application_tour_agent_conversation`, `application_agent_launch`,
  `core_agent_context_supplement`, `qt_tour_chat_in_tab`,
  `qt_tour_side_chat` and `architecture_boundaries` pass.

## Out of scope

- Shrinking the tour prompt (sending only the asked thread's stop, or
  summarizing earlier turns). Whether that is worth doing can be judged once
  the prompt is no longer capped at 16 KiB.
- Raising `maximum_chat_message_bytes` for typed messages.
