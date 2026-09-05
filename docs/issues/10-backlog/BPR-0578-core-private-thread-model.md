---
id: BPR-0578
title: Implement the Qt-free private thread, subject, turn, revision, actor, and lifecycle model
status: Backlog
rfc: RFC-BPR-0023
milestone: Post-Pack v2
depends_on: [BPR-0577]
---

# Implement the Qt-free private thread, subject, turn, revision, actor, and lifecycle model

## Objective

Define deterministic private-conversation semantics without persistence, process, provider, or presentation dependencies.

## Required final state

- Strong IDs exist for threads, turns, and revisions, with validation and stable names where appropriate.
- Thread subjects cover PR-wide and existing exact semantic identities without presentation coordinates or null-as-PR ambiguity.
- Thread lifecycle is open/resolved-locally/archived; staleness/composition/publication are derived facts, not destructive lifecycle states.
- Every revision has exactly one typed actor: reviewer, exact agent run, or legacy unknown.
- Editing creates an immutable successor; original actor/body/digest remain unchanged.
- Complete/incomplete/failed/cancelled revision eligibility is explicit.
- Core replay/example/property tests cover creation, reply, edit, archive, stale observation, invalid identity, and no silent reanchor.

## Non-goals

- SQLite schema
- Agent protocol parsing
- Qt labels

## Completion evidence

- Focused core test command/output
- Forbidden-dependency/include check
- Transition table and rejection coverage

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
