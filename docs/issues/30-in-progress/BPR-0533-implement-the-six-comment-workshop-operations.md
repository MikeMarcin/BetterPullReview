---
id: BPR-0533
title: Implement the six comment-workshop operations
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0503]
---

# Implement the six comment-workshop operations

## Why

`docs/ux/ai-experience.md` promised six comment-workshop operations —
clarify, shorten, make actionable, phrase as a question, make more direct,
and check whether the premise is supported — and [[BPR-0506]] found that
none of them exists in the tree. What shipped under [[BPR-0503]] is one path
from a proposed draft edit to a before/after proposal, with
`workshop_comment` as a single `core::agent_entry_point`. On 2026-09-02 the
product owner chose to card the six rather than withdraw the promise; this
card holds them, outside RFC-BPR-0012's closeout, which [[BPR-0507]] does not
make depend on it.

## Required final state

- Each of the six operations is a typed request the workshop can express,
  carried through the existing `workshop_comment` entry point with the
  operation named in the supplement rather than as six new entry points.
- The Qt workshop offers the six as controls on the proposed-revision
  surface, each producing a before/after proposal through the one shipped
  path; nothing publishes without the reviewer's normal GitHub controls.
- The MCP helper and the agent boundary document name the operation set
  exactly once; the UX guide's promise and the tree agree.
- Tests cover every operation's request shape, its refusal when the draft
  is absent, and the unchanged single-path composition.

## Non-goals

- A second entry point, agent, or workshop surface.
- Any change to finding anchoring or the side chat.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal. A scout confirmed the
shape before any edit: `workshop_comment` is an `agent_entry_point`
enumerator with no live dispatch of its own, the shipped proposal path is
`agent_comment_workshop_host::propose` called from the finding-promotion
outcome, no type enumerates the six operations anywhere, the revision
editor offers only accept and reject, and the MCP helper names no
workshop concept. The slice runs on `bpr-0533-work` in a worktree; the
UX guide, the agent boundary document, and the helper's single naming are
the integrator's at close.

## Part one landed — 2026-09-03

At `43ce2b74`. `core::workshop_operation` in `agent_chat_scope.hpp` names
the UX guide's six in its order — `clarify`, `shorten`, `make_actionable`,
`phrase_as_a_question`, `make_more_direct`, `check_the_premise` — with
`workshop_operation_count` pinned by a `static_assert`, `stable_name`,
`declared`, and `workshop_operation_for(entry_point, operation)`, which
admits an operation only under `workshop_comment`. The operation rides as
an optional `Operation` on `agent_supplement_request`,
`agent_context_supplement`, `agent_comment_workshop_proposal`, and
`qt::comment_workshop_proposal`, and reaches the agent's wire frame in one
position after `chat_message`. `agent_comment_workshop_host::propose`
takes it as a third parameter and names it in the proposal; finding
promotion keeps proposing with no operation named, because a promoted
finding is a proposal nobody workshopped.

`review_draft_editor` offers one action per operation
(`clarify_action()` … `check_the_premise_action()`, plus
`workshop_action(operation)`), disabled with a tooltip and never hidden
without a drafting draft, and emits a typed
`comment_workshop_request{Draft, Operation}` through `set_workshop_dispatch`
— a setter, so the workbench window and its pins are untouched. The app
records the asked-for operation and spends it on the next proposal. The
members aggregate grew by three with its pins extended. A new selector,
`application_agent_comment_workshop_host`, covers the host; two mutations
fail on both sides of the application/Qt seam, one of them refused by the
compiler first.

Reproduced on the merged tree: build clean, 20/20 selectors in 235 s, boundaries, the
touched files tidy and format clean; both composition pins recomputed
(`full `ad6816235872…`, manifest `e4f895975685…``).

Not yet delivered, and why part two exists: pressing an operation records
it and labels the surface, but `workshop_comment` has no live dispatch, so
no agent turn starts and the before/after appears only if a revision
arrives through the finding-outcome path. Part two starts one invocation
per press through the existing invocation path with the operation in the
supplement, pairs the outcome to the press by turn identity, and proves
press → invocation → proposal in the composed graph. The UX guide, the
agent boundary document, and the MCP helper's single naming follow at
close.
