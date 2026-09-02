---
id: BPR-0506
title: Simplification and closeout preparation
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0505, BPR-0513, BPR-0514, BPR-0517]
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
- `continue_against_old_revision` is old-revision in the session sense only:
  the launch path still composes context from the revision now under review
  and then admits a session bound to the old one ([[BPR-0517]]). Decide
  whether the context must also be composed from the old revision, and card
  it if so.
- [[BPR-0517]] added a test-only `LastChatTurn` evidence field to the
  composition so the continued course can read the dispatched turn; drop it
  if the counters suffice.

## Scouted worklist — 2026-09-02

A read-only scout over the delivered slices, with the decision each item
needs; the pass records the outcome of each in the ledger's
`## Simplification` section.

1. **Remove:** `mcp_context_helper.hpp:106` says "the four this helper
   implements"; it answers three (`initialize`, `tools/list`,
   `tools/call`) and never answered a fourth. Proven by
   `application_mcp_context_helper`.
2. **Keep, with the reason:** `LastChatTurn` (`github_application.cpp`,
   test-only). The launch path admits its own session, so the chat host's
   counters prove a turn happened but not what it carried or which binding
   it kept; `agent_chat_composition_public_path.cpp` asserts the message
   bytes and the kept binding through it. Not a counter's job.
3. **Doc consistency:** `docs/ux/ai-experience.md:27` still promises the six
   comment-workshop operations without the "specification only" caveat that
   `ai-agent-boundary.md` and [[BPR-0505]] already carry. One line; decide
   with the owner whether a card owns the six or the promise is withdrawn.
4. **Judgment:** eight construction-cut flags
   (`Disable…Composition` in `github_application_fixture_access.hpp:66-77`)
   share one identical declare/copy/guard shape at eight sites. Folding them
   into one mechanism is new structure, not a deletion; record it as a
   deliberate non-removal or split it to its own card.
5. **Settle two ledger labels:** the "Private notes and drafts" row reads
   `Core half verified` with a leftover "the composed course remains
   `Planned`" phrase, and the "Finding/chat/promotion decisions" row reads
   `Qt half verified` while its own text says the composed workbench is
   verified. Confirm `application_agent_invocation_composition_contract`
   asserts zero note and draft counts before relabelling the first.
6. **Confirmed, decision needed:** `continue_against_old_revision` keeps the
   session's old binding (`agent_chat_session_host.cpp`, `choose`) while
   `github_application.cpp` composes the frozen snapshot from the epoch
   host's current revision. Behaviour, so not this pass's to change; the
   owner decides whether old-revision context is required and a card
   follows.
7. **Presentation fix:** the finding queue's six buttons sit in one
   `QHBoxLayout` (`workbench_window.cpp:709-721`) and clip at 100 %; wrap or
   stack them. Proven by `qt_finding_queue_view` and
   `qt_composed_github_scale_100`'s capture.

Not read by the scout and therefore unconfirmed for duplicated policy:
`agent_chat_scope.cpp`, `agent_finding_promotion.cpp`, the Qt widgets'
bodies, and `agent_protocol_loop.cpp`; the pass reads them itself.
