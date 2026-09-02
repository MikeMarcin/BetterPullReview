---
id: BPR-0505
title: "Documentation: extend the AI boundary, UX, fixture catalog, and matrix"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0504]
---

# Documentation: extend the AI boundary, UX, fixture catalog, and matrix

`docs/architecture/ai-agent-boundary.md` already states the RFC-BPR-0011
allocation, protocols, workspaces, context supplement, and session semantics
in 380 lines. This card extends that document rather than writing a second one
beside it, and brings `docs/ux/ai-experience.md`,
`docs/verification/fixture-catalog.md`, and
`docs/verification/master-verification-matrix.md` up to what the tree actually
does. The RFC-BPR-0012 ledger's documentation ledger is filled in the same
slice.

## Required final state

- `ai-agent-boundary.md` gains the finding schema, the anchor policy and its
  three states, chat scope, the two stale continuations, the promotion
  boundary, full-agent behaviour, the MCP tool list, privacy, and the stated
  limitations — as new sections in the existing document.
- Its documentation ledger identifies functional-core, application-host, and
  Qt-presentation-adapter ownership; the typed intents, events, observations,
  requests, and deltas; authority; concurrency and generation; and any
  evidence-backed no-impact region.
- `ai-experience.md` gains implementation truth: what shipped, what the anchor
  states are called, what the two continuations are, and what surface the
  workshop actually uses.
- The fixture catalog's `## AI` section describes the finding-payload canary
  and the MCP helper canary beside the existing entries.
- `master-verification-matrix.md` rows AI-3 and AI-4 name the delivered
  surfaces, and new rows cover MCP read-only enforcement and side-chat
  staleness.

## Non-goals

- A new AI document beside the boundary document — refused here, for the
  reason BPR-0178 gave: a reader would have to work out which was current.
- The simplification pass and the closeout sections — BPR-0506.
- Any RFC status change — closure order is the index's, and BPR-0507's.
- New behaviour of any kind.

## Required direct evidence

- `python3 scripts/verify_documentation.py --root .` passes, run without a
  pipe.
- `python3 tests/scripts/verify_documentation_tests.py` passes.
- Every selector named in a new or edited matrix row exists: each is confirmed
  against `ctest --preset dev -N` and `ctest --preset core-only -N`.
- Every claim added to `ai-agent-boundary.md` names the card and the check
  that proves it; anything not delivered is named as not delivered rather than
  left to silence.
- No sentence asserts a behaviour the tree does not have — each is traceable
  to a selector recorded in `docs/verification/RFC-BPR-0012.md`.
