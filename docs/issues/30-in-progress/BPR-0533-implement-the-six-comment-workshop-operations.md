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
