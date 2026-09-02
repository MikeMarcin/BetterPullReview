---
id: BPR-0505
title: "Documentation: extend the AI boundary, UX, fixture catalog, and matrix"
status: Done
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

## What this slice delivered — 2026-09-02

Documentation only. No source, test, build, script, or tracker file was
touched, so nothing here changes what the tree does. Five documents changed:
`docs/architecture/ai-agent-boundary.md`, `docs/ux/ai-experience.md`,
`docs/verification/fixture-catalog.md`,
`docs/verification/master-verification-matrix.md`, and the documentation-ledger
section of `docs/verification/RFC-BPR-0012.md`.

### Decisions worth knowing

- **Extended in place, with the RFC-BPR-0012 material as one new top-level
  section.** `ai-agent-boundary.md` already told RFC-BPR-0011's story and ended
  with its own "What is not claimed". Interleaving RFC-BPR-0012's sections
  among them would have left that closing statement stranded mid-document, so
  the new work is one section — **Explicit AI review: findings, chat, workshop,
  and the MCP helper** — carrying ten subsections: the eight this card names,
  a claim-to-test table, and a closing section that holds the stated
  limitations beside what was not delivered. Still one document, as
  [[BPR-0178]] required.
- **Every added claim names its card and its check.** The subsections cite
  inline, and a 38-row claim-to-test table sits under them, in the shape the
  RFC-BPR-0011 half of the document already uses.
- **Four things RFC-BPR-0012 specified and did not build are named rather than
  left to silence**: the panels are not in the workbench drawer ([[BPR-0513]]),
  there is no composed side-chat session host so a send is refused by name
  ([[BPR-0514]]), the frozen MCP snapshot carries alignment facts rather than
  file bytes so `list_snapshot_files` answers with an empty list, and the six
  comment-workshop operations `ai-experience.md` lists appear nowhere in the
  tree: a case-insensitive search of `src` for those wordings returns one
  unrelated line. The last of these was found while writing and is new to the
  record.
- **Two new matrix rows rather than widened old ones.** AI-5 is MCP read-only
  enforcement and AI-6 is side-chat staleness; folding either into AI-3 or AI-4
  would have made one row's failure ambiguous about which claim broke.

| Claim | Command | Result | Commit |
|---|---|---|---|
| `ai-agent-boundary.md` gains the finding schema, the anchor policy and its three states, chat scope, the two stale continuations, the promotion boundary, full-agent behaviour, the exact MCP tool list, privacy, and the stated limitations, as new sections in the existing document | `awk '/^## Explicit AI review/{f=1} f&&/^### /' docs/architecture/ai-agent-boundary.md` | Ten subsections: the eight topics this card names, **Where to check each RFC-BPR-0012 claim**, and **What RFC-BPR-0012 does not claim**, which holds the stated limitations. The document went from 374 to 934 lines; no second AI document was created | this commit |
| Every claim added names the card and the check that proves it, and anything not delivered is named as not delivered | reading the added sections | Each subsection cites its card and selector inline; the claim table has 38 data rows; **Stated limitations** lists seven, and **Not delivered** names five items with their reasons | this commit |
| No sentence asserts a behaviour the tree does not have | public headers and delivered cards read before writing: `agent_finding.hpp`, `agent_finding_promotion.hpp`, `agent_chat_scope.hpp`, `agent_session.hpp`, `agent_finding_delivery.hpp`, `agent_finding_host.hpp`, `agent_comment_workshop_host.hpp`, `mcp_context_helper_supervisor.hpp`, `mcp_context_helper.hpp`, `side_chat_panel.hpp`, `finding_queue_view.hpp`, and the shipped `.cpp` for the queue's anchor-state text, the MCP method dispatch, and the canary's finding modes | Every stated bound, enumerator, count, refusal name, and rendered string was read out of the tree rather than from a card summary. Three drafted sentences were corrected against the source: the MCP helper answers **three** request methods (`initialize`, `tools/list`, `tools/call`) and not four; the canary attaches a payload only under `--emit-findings` or `--findings-transcript`, not by default; and `store_promoted_finding`'s check is `application_agent_invocation_host`, not `application_agent_finding_protocol` | this commit |
| The RFC-BPR-0012 documentation ledger identifies functional-core, application-host, and Qt-presentation-adapter ownership, the typed intents/events/observations/requests/deltas, authority, concurrency and generation, and evidence-backed no-impact regions | reading `## Documentation ledger` in `docs/verification/RFC-BPR-0012.md` | Filled: a five-region ownership table, a five-kind typed-value table, an authority section naming the single durable write and the single anchoring authority, a concurrency section recording that **no new generation counter was added** and why, and six evidence-backed no-impact regions. The claim-matrix rows and card table were not touched | this commit |
| `ai-experience.md` gains implementation truth: what shipped, the anchor state names, the two continuations, and the workshop's surface | reading `## Implementation truth` | Five subsections: what shipped, the three anchor states as the queue prints them (`state (reason)`, e.g. `anchored (none)`), `start_new_session` and `continue_against_old_revision`, the two-`QPlainTextEdit` plain-text workshop with the refused-renderer reason, and what is specified but not built | this commit |
| The fixture catalog's `## AI` section describes the finding-payload canary and the MCP helper canary beside the existing entries | reading `## AI` in `docs/verification/fixture-catalog.md` | Two new bullets and two new subsections. The finding-payload half covers both fixtures — the extended `tests/fixtures/agent/agent_canary_main.cpp` and the composed `tests/end_to_end/agent_finding_canary_main.cpp` — because the first deliberately anchors nowhere and only the second proves a correctly placed finding validates | this commit |
| Matrix rows AI-3 and AI-4 name the delivered surfaces, and new rows cover MCP read-only enforcement and side-chat staleness | reading `docs/verification/master-verification-matrix.md` | AI-3 and AI-4 rewritten with named surfaces and selectors; AI-5 (MCP read-only) and AI-6 (side-chat staleness) added | this commit |
| Every selector named in a new or edited matrix row exists | `ctest --preset dev -N` and `ctest --preset core-only -N`, then a script extracting each selector from rows AI-3 through AI-6 and matching it against both listings | 20 selector mentions, 16 distinct, **0 missing** from `dev` (237 tests). The six that are core-only suites — `core_agent_finding`, `core_agent_finding_promotion`, `core_agent_session`, `core_agent_chat_scope`, `application_agent_finding_protocol`, `application_agent_invocation_host` — plus `live_mutation_isolation` are also present in `core-only` (105 tests). Neither preset was built or run; `build/dev` and `build/core-only` were both already configured | this commit |
| No selector-shaped identifier anywhere in the four edited verification and architecture documents names a test that does not exist | a script matching every `` `core_*` ``, `` `application_*` ``, `` `qt_*` ``, `` `architecture_*` ``, `` `composed_*` ``, and `` `live_*` `` code span against the `dev` listing | 55 distinct identifiers, all registered | this commit |
| The documentation gate passes, run without a pipe | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK`, exit 0 | this commit |
| The documentation gate's own tests pass | `python3 tests/scripts/verify_documentation_tests.py` | `Ran 54 tests`, `OK`, exit 0 (the printed `ERROR:` lines are the suite's own negative-control fixtures) | this commit |
| Whitespace | `git diff --check` | exit 0 | this commit |
| No behaviour change | `git status --porcelain` | Only the five documents and this card are modified by this slice. `src/adapters/qt/**`, `src/app/src/github_application.cpp`, and `tests/end_to_end/application_review_round_trip_composition_contract.cmake` are also dirty in this working tree, from [[BPR-0513]] running concurrently; none was touched here | this commit |

Not in this slice: the simplification pass and the ledger's closeout sections
([[BPR-0506]]), and any RFC status change ([[BPR-0507]]).

## Integrator reproduction — 2026-09-02

`python3 scripts/verify_documentation.py --root .` → `DOCUMENTATION_OK`;
`python3 tests/scripts/verify_documentation_tests.py` → 54 tests OK;
`git diff --check` clean; every selector named in matrix rows AI-3 through
AI-6 is present in `ctest --preset dev -N`. The two nits the slice surfaced
— the MCP helper header's comment counting four request methods where the
helper answers three, and the RFC-0012 privacy row whose composed half is
already asserted by `application_agent_invocation_composition_contract` —
are recorded on [[BPR-0506]] for the simplification pass.
