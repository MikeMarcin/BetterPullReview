---
id: BPR-0496
title: Plan RFC-BPR-0012 verification and allocate its cards
status: Done
rfc: RFC-BPR-0012
depends_on: []
---

# Plan RFC-BPR-0012 verification and allocate its cards

RFC-BPR-0012 adds explicit AI assistance on top of RFC-BPR-0011's launch
boundary: structured findings, a before/after comment workshop, a
revision-aware side chat, a full-agent session, and a read-only MCP context
helper. It is the first RFC in the pack with no implementation at all — zero
cards linked it before this one — so this slice records the allocation, the
claim matrix, and the card set before any source moves.

## Outcome

`docs/verification/RFC-BPR-0012.md` records the allocation, the claim matrix,
and cards BPR-0497 through BPR-0507.

## The rule the whole plan serves

**AI output is a proposal until a person promotes it, and nothing it says can
publish, mutate, or move the UI on its own.** A finding, a workshop revision,
a chat answer, and a full agent's actions all re-enter as typed events bound
to the exact revision they were computed against; the only way any of them
reaches GitHub is a person converting it into a local draft and publishing
through the ordinary RFC-BPR-0008 controls, or a full agent using its own
GitHub tools under its own approvals, whose results come back through
ordinary synchronization.

The convenience that would erode this is obvious and would look helpful: the
agent returned a good comment, so post it. RFC-BPR-0012's non-goals say the
opposite three times over — no automatic invocation, no automatic publication,
no treating confidence as proof — and the product contract forbids
automatically publishing AI output. Structural expression: a finding converts
into exactly one `review_draft_command` or `defect_command` (the existing
RFC-BPR-0008 and RFC-BPR-0009 vocabularies), never into a
`review_publication_proposal`, and the host has no path from agent output to
the mutation coordinator.

## The second rule

**Anchors are semantic identities or nothing.** A finding's path/line/OID
validates against RFC-BPR-0004's `source_line_ref`, `source_boundary_ref`,
`alignment_segment`, `change_group`, and `move_relation` identities and the
session's frozen `agent_revision_binding`. An anchor that does not validate is
shown as unanchored; one whose revision moved is shown as stale. Nothing
guesses a line from a y-coordinate or a ribbon path, and nothing silently
re-attaches a finding to a newer head.

## Why this RFC can be implemented now, and what it reuses

`depends_on` names RFC-BPR-0008 and RFC-BPR-0011. Both are substantially
implemented on available targets, and the RFC index's product-owner exception
lets implementation through RFC-BPR-0014 proceed while earlier RFCs stay
`Proposed`, ending at a closeout card in `10-backlog/`.

The surfaces it extends already exist and are named on each card so nothing
is built twice: `core::agent_profile`, `agent_session` (with
`agent_revision_binding` and `agent_session_staleness`), and
`agent_context_supplement`; the `agent_invocation_host`, the portable
protocol loop, and `agent_launch_request`; the `review_draft_editor`,
`review_mutation_application_host`, and `review_draft_command`; the review
ledger's private notes and the defect workflow's `defect_command`; and the
composition root's `AgentHost` construction beside the review-mutation host.
`docs/ux/ai-experience.md` already states the seven entry points, the six
finding actions, and the MCP helper's limits, and
`docs/verification/master-verification-matrix.md` already carries rows AI-3
and AI-4 for this RFC. What the tree has nothing of is a finding type, a
structured-output decoder, an MCP helper, a side chat, or a workshop surface.

## What is genuinely deferred

Nothing on a platform, a person, or a live service. Checked against the RFC
text: every criterion is proven with fixture processes (the RFC-BPR-0011
canary pattern), the recorded RFC-BPR-0008 fixture path, and hermetic Qt and
core tests. A smoke run against an installed Codex or Claude harness stays
optional extra validation, as it is for RFC-BPR-0011. One decision is left
explicit on BPR-0503 rather than pre-empted here: the workshop's before/after
surface, since no Markdown renderer exists in the tree.

## Cards

| Card | Slice |
|---|---|
| BPR-0497 | Core: structured finding schema and anchor validation |
| BPR-0498 | Core: finding promotion into a local draft or defect, never a publication |
| BPR-0499 | Core: explicit AI actions, side-chat scope, and stale continuation |
| BPR-0500 | Host: structured-finding decode, limits, and typed revision-bound re-entry |
| BPR-0501 | Host: read-only MCP context helper over stdio |
| BPR-0502 | Qt: side-chat drawer with context chips and stale state |
| BPR-0503 | Qt: finding queue and before/after comment workshop |
| BPR-0504 | Composition: wire findings, chat, workshop, and MCP into the shipped workbench |
| BPR-0505 | Documentation: extend the AI boundary, UX, fixture catalog, and matrix |
| BPR-0506 | Simplification and closeout preparation |
| BPR-0507 | Verify and close RFC-BPR-0012 |
