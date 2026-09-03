---
id: BPR-0533
title: Implement the six comment-workshop operations
status: Done
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

## Part two landed — 2026-09-03

At `ce85d5d3`. A press now starts the agent turn that answers it.
`ask_comment_workshop` admits the operation through
`workshop_operation_for(workshop_comment, …)`, refuses a press while one is
outstanding ("A workshop turn is already running for this comment."),
records `pending_comment_workshop{ Draft, Operation, Session }`, and starts
one invocation through the existing `dispatch_agent_invocation` — which
gained the entry point and the operation on `agent_context_observation` —
so `AgentHost->dispatch(` keeps its single call site. The supplement
builder refuses whole (`invalid_observation`) when the entry point does not
admit the named operation. The session sink in
`composition_agent_invocation_projection::admit`, the one place a session
identity is allocated, binds the press to the turn it starts; the finding
outcome spends the operation only when the bound session equals the
outcome's and the draft still matches, so an unrelated outcome proposes
with no operation named and the press survives. If the dispatch is refused
the press is cleared. The six controls close, with the tooltip, until the
proposal arrives.

`application_agent_finding_workshop_press_contract` (the `workshop-press`
course) proves press → one invocation → the press bound to that turn and
unspent at delivery → the converted outcome proposes "Proposed by:
Clarify" and clears the press, with zero GitHub mutation. Two mutations:
recording the press but skipping the invocation fails that course and
`application_agent_chat_composition_contract` (the launch-path count);
deleting the editor's `WorkshopAsked` fails `qt_review_draft_editor` and
the course. Both restored byte-exact to the pinned digest.

Reproduced on the merged tree: build clean, 23/23 selectors in 297 s, boundaries, tidy
and format on the touched sources; both composition pins recomputed
(`full `bac0b7e5161e…`, manifest `06d72efb51e7…``). The three naming sites now describe the shipped behaviour,
and the MCP helper names the set once as requests it does not serve.

Left open, carded as [[BPR-0538]]: a turn that starts and finishes without
a revision leaves the press recorded and the six closed until the draft
changes, and the two-turn "unrelated outcome" case is proven at the
pairing's unit level, not end to end.

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| The typed set exists, is closed, and admits only under its entry point | `ctest --preset dev -R '^(core_agent_chat_scope\|core_agent_context_supplement)$'` | passed, in both parts' runs | this commit |
| Each operation's request shape reaches the one proposal path, refusals typed | `ctest --preset dev -R '^(application_agent_comment_workshop_host\|application_agent_launch\|qt_review_draft_editor)$'` | passed; the in-flight and no-draft refusals presented, never hidden | this commit |
| A press starts one turn and its answer names the operation, through the one shipped path | `ctest --preset dev -R '^application_agent_finding_workshop_press_contract$'` | `agent-finding-workshop-press:pass`, `AgentHost->dispatch(` pinned at one call site | this commit |
| Both halves of the seam are load-bearing | the two mutations above, restored | the course and the composition contract fail on the skipped invocation; the editor test and the course fail on the dropped `WorkshopAsked` | ce85d5d3 |
| The composed graph, boundaries, manifest, and pins hold | `ctest --preset dev -R` over the part-two set; `check_boundaries`; `test_network_isolation_manifest`; the round-trip contract | 23/23 selectors in 297 s; exit 0; passed; passed after the two-pass repin | this commit |
| The documents and the tree agree | `python3 scripts/verify_documentation.py --root .` after the three naming edits | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
