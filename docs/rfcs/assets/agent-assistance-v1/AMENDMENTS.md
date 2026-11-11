# Proposed amendments to adopted RFCs

This pack changes one rule in an adopted RFC and adds to five others without
changing what they require. The sources were read on lane
`claude/v0-5-0-prerelease-targets-52265d` at
`90785ae34aeccf63724b9e5e7a70446f89341ec5`. Authoring leaves the live text
unchanged; each owning milestone applies its own through `implement-rfc`.

## A1. RFC-BPR-0012 and the AI experience: two entry points change their subject

Source: [RFC-BPR-0012](../../RFC-BPR-0012-ai-review-chat-findings-mcp.md),
`docs/ux/ai-experience.md`, and the rule in
[agent_chat_scope.hpp](../../../../src/core/include/bpr/core/agent_chat_scope.hpp)
that an entry point either acts on the reviewer's selection or takes its
subject from the session.

| Before | After |
|---|---|
| `evaluate_proposed_fix` acts on the selection: "a fix *to* something". | It acts on a conversation: a review thread, a private note or a defect. No selection is needed. |
| `analyze_current_file` acts on the selection, because "the selection is the only input that names a file". | It acts on the file open in Review. No selection is needed. |

Preserved: the closed set of eight entry points and the rule that every AI
action originates from an explicit visible command. Neither entry point has
a caller today, so no behavior a user has seen changes.

Reconciliation: `returning-pull-request`
([DRAFT-0002](cards/DRAFT-0002-was-this-addressed-on-a-thread.md)) and
`explain-file`
([DRAFT-0004](cards/DRAFT-0004-explain-this-file-in-review.md)).

## Additions that change no existing requirement

- **RFC-BPR-0102** gains a fifth question subject, a check, beside the pull
  request, a file, lines and a conversation
  ([DRAFT-0008](cards/DRAFT-0008-ask-why-a-check-failed.md)).
- **RFC-BPR-0068** keeps its boundary word for word: BPR downloads no log,
  embeds no website, reruns nothing and executes no repository code. A check
  row gains one agent control.
- **RFC-BPR-0090** gains a second way into its in-pane suggestion edit, with
  the agent's replacement as the starting text
  ([DRAFT-0006](cards/DRAFT-0006-suggest-the-change-from-a-draft-comment.md)).
  The comment workshop gains a seventh operation.
- **RFC-BPR-0109** said it "adds no runner". An agent run now performs a
  planned check, and its result is recorded in that RFC's existing reported
  class. The observed class stays reserved and unused
  ([DRAFT-0010](cards/DRAFT-0010-run-a-planned-check-with-the-agent.md)).
- **RFC-BPR-0086** gains an Agents section under its existing counting,
  store, purge and export rules
  ([DRAFT-0012](cards/DRAFT-0012-insights-agents-section.md)).
- **RFC-BPR-0107** allows that "a later RFC may add recognized purposes".
  This pack proposes six: `review.latest-upload`, `review.proposed-fix`,
  `review.explain-file`, `review.suggest-change`, `review.check-failure`
  and `local-draft.run-check`.
