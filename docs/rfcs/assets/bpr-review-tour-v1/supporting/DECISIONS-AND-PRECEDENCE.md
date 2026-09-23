# Locked decisions and discarded alternatives

This document captures the conversation's settled direction. It prevents an implementation agent from reverting to a superficially convenient design discussed earlier.

| Topic | Required decision | Do not reintroduce |
|---|---|---|
| Product | Author-prepared asynchronous guided code review, with problem-first context, meaningful order, decision points and reviewer-specific scrutiny. | Generic verbose diff summaries or a “better PR description” feature standing in for the tour. |
| Shared representation | Normal GitHub comments, real Markdown links, normal replies and author identity. | A BPR-owned shared tour with a website export, hosted portal, hidden metadata, bot dependency, or PR-body-only walkthrough replacing comments. |
| Website use | A website-only reviewer can start, follow the route, inspect the code and reply. | BPR installation as a prerequisite or navigation labels that are not working website links. |
| Preparation | Problem → Solution → stops → preview, fully functional without AI, revisitable and non-modal. | Auto-inference on load, a mandatory interview, modal questionnaire, or AI-required import. |
| Problem and solution | Independent Markdown editors and independent configurable skill hooks. | One giant system prompt that rewrites all fields or a solution-first description. |
| Magic button | Selected text if one contiguous nonempty selection exists; otherwise whole field; empty field drafts from context. | Rewriting the whole field when a selection exists, implicit sentence expansion, or broad writes justified only by prompt wording. |
| Replacement UX | In-place one-step undo/redo. Redo uses received bytes. | A required suggestion panel, Use/Apply/Dismiss, before/after acceptance, streamed partial edits, or clearing undo history. |
| Empty field | Recover existing PR explanation before cautious inference. Jira is deferred. | Invented rationale, impact, tests, alternatives, severity or company-wide ticket search. |
| Outline | Optional skeleton of titles/destinations/private prompts only. | Full comment bodies, auto-completion, bulk generate-all or mark-all-Ready. |
| Author attention | Each generated stop is visited and the current displayed content completed deliberately. | A pre-generation visit counting for unseen output, hidden-row visits, dwell-time policing, forced token edits, or claims that acknowledgement proves understanding. |
| Ordering | Free local insertion/reordering and supported post-publication maintenance. | Freezing tours forever after initial publication or deleting/recreating all comments just to reorder them. |
| Publication | Prefer one staged review and one COMMENT submission, through native API or admitted `gh api`. | Per-stop immediate publication presented as a batch, `gh pr review` used as the structured batch interface, global token extraction or auto-publish from AI. |
| Compatibility | Preserve exact code meaning with normal comment/permalink fallbacks; expose different visibility boundaries. | Claiming standalone comments or later edits are an atomic review submission, or using unrelated changed code as a fake anchor. |
| Reopen | Repopulate from current GitHub raw Markdown/IDs, with deterministic mapping and explicit conflicts. | Regenerating instead of loading, scraping rendered HTML, overwriting website edits, or fabricating lost private provenance/history. |

## Deliberate implementation decisions made by this pack

A root review-summary index is preferred to a separate PR comment so it can share the initial review submission. The route can later span several reviews. Normal comments remain the authoritative representation; light interpretation of headings/ordered links is a convenience, with manual mapping for nonstandard bodies.

The pack adds contiguous source-range anchors and a tour-specific PR-conversation-comment fallback for destinations beyond RFC-0022's current changed-file/right-side slice. These are new scope with explicit tests, not claims about existing native support. Where qualified GraphQL can stage file subjects in the same review, use it; the user's `gh api` concern does not prohibit fixed GraphQL operations.

A visible completion command such as Ready and next is retained for generated stops. It acknowledges the current stop, not acceptance of an AI proposal. Undo/redo is the only required rewrite-review mechanism. No extra AI Accept button appears.

Five new RFCs are additive. Do not edit the settled historical RFC texts merely to make these choices look preexisting. Update current product/architecture documentation and index integration narrowly during implementation, preserving the source archive and ID mapping.
