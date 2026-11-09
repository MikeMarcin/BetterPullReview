# Native agent sessions and tour context: proposed pack

This is an unadopted authoring pack for the owner's 2026-11-08 request.
It proposes native sessions in the Agent tab and tour side chats, New session
with retained history, explicit replay fallback and adequate tour-state
context. No product implementation, live-ID allocation or board adoption
has been authorized by the authoring request.

Read the [RFC](RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md), then
the [proposed amendment](proposed-session-amendment.md) to existing contracts.
[DECISIONS.md](DECISIONS.md) is the single record of actual owner answers,
open choices, draft checks, review coverage and unevaluated execution
eligibility. The [glossary](GLOSSARY.md) distinguishes the overloaded session
and confirmation terms.

The [implementation entrypoint](IMPLEMENTATION_PROMPT.md) points to the
canonical [milestone-plan.json](milestone-plan.json) and its generated
[MILESTONES.md](MILESTONES.md). The proposed work is:

| Card | Outcome |
|---|---|
| [DRAFT-0001](cards/DRAFT-0001-native-agent-tab-sessions.md) | Native runtime sessions, durable identities and workspaces in the Agent tab |
| [DRAFT-0002](cards/DRAFT-0002-tour-sessions-and-history.md) | Per-agent tour sessions and scoped New session/history behavior |
| [DRAFT-0003](cards/DRAFT-0003-tour-status-and-context.md) | Authoritative tour snapshots, product guide and correct source-status reasons |
| [DRAFT-0004](cards/DRAFT-0004-replay-and-history-migration.md) | Explicit fallback and migration of legacy histories |
| [DRAFT-0005](cards/DRAFT-0005-qualify-and-document-sessions.md) | Installed-agent/public-path qualification, usage comparison and documentation |
| [DRAFT-0006](cards/DRAFT-0006-close-native-sessions-rfc.md) | Closeout of the full RFC against actual evidence |

The [portable schema](milestone-plan.schema.json) is copied unchanged from
the discovered implement-rfc skill's offline kit. The schema, validator
provenance and commands are recorded in the decision record. This pack is
the original draft, revised by the requested Astra max review; the decision
record preserves the initial version's provenance, review coverage and actual
checks without manufacturing an implementation baseline or owner approval.
