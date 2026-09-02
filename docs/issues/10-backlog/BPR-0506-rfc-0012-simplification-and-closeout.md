---
id: BPR-0506
title: Simplification and closeout preparation
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0505, BPR-0513, BPR-0514]
---

# Simplification and closeout preparation

The behaviour-preserving pass over everything BPR-0497 through BPR-0505
delivered, then the ledger. Removals are recorded with the reasoning that made
them unnecessary, and anything deliberately kept is recorded with why, so the
next person choosing between two forms does not rediscover the argument.
`docs/verification/RFC-BPR-0012.md` gains its `## Simplification` and
`## Closeout preparation` sections in the shape
`docs/verification/RFC-BPR-0011.md` already uses.

## Required final state

- Each behaviour-preserving removal is named with what the delivered design
  made unnecessary. Each deliberate non-removal is named with why it stays —
  a state a reviewer can reach, or a mandatory RFC surface, not "unreached".
- The ledger's claim matrix has no row reading `Planned`. Every row names its
  card and the command that proves it, and any row weaker than the plan
  imagined says so in its own words.
- `## Closeout preparation` names what is not delivered rather than leaving a
  reader to infer it from silence.
- The ledger states that RFC-BPR-0012 stays `Proposed` and that closure stays
  ordered behind RFC-BPR-0008 and RFC-BPR-0011.
- The focused selectors each acceptance criterion names are rerun after the
  simplification, and their results are recorded with the commit.

## Non-goals

- Closing the RFC or changing its status — BPR-0507, under the index's order.
- New behaviour, new types, or a new surface of any kind.
- The documentation content itself — BPR-0505 delivered it.
- The full `dev` and `core-only` matrices, which are not a closeout gate.

## Required direct evidence

- Every removal appears in the ledger's `## Simplification` section with its
  reasoning, and every non-removal with its own.
- No claim-matrix row reads `Planned`; a row without a command and a card is a
  defect in this slice.
- The rerun after simplification is recorded with results and commit:
  `ctest --preset core-only -R '^core_agent_' --output-on-failure`, and
  `ctest --preset dev -R
  '^(application_agent_|application_mcp_|qt_side_chat_panel|qt_finding_queue_view|qt_review_draft_editor|composed_application_hosts)'
  --output-on-failure`.
- `python3 scripts/verify_documentation.py --root .` passes, run without a
  pipe.
- `git diff --check` is clean over the slice.
- RFC-BPR-0012 remains `Proposed`, and no sentence in the ledger makes a
  closure claim.

## Inputs from the landed slices — 2026-09-02

- `src/adapters/mcp/include/bpr/adapters/mcp/mcp_context_helper.hpp`'s
  comment counts four request methods; the helper answers three
  (`initialize`, `tools/list`, `tools/call`). Found by [[BPR-0505]].
- The ledger row "Private notes and drafts are never included without
  explicit selection" still reads core-half verified with its composed half
  planned, while `application_agent_invocation_composition_contract` already
  asserts zero note and draft counts; settle the row against that evidence.
- The six comment-workshop operations `docs/ux/ai-experience.md` names
  (clarify, shorten, make actionable, phrase as a question, make more direct,
  check the premise) exist nowhere in the tree; [[BPR-0505]] records them as
  not delivered. Decide whether they are a card or a removed promise.
- The panels are composed and, after [[BPR-0513]], placed; the side-chat
  session host is [[BPR-0514]]. Neither is this card's.
- At 100 % the finding queue's six action buttons overflow the conversations
  drawer's width and the last two are clipped ([[BPR-0513]]'s capture);
  wrap or stack them.
