---
schema: "implement-rfc.card.v1"
id: "BPR-2370"
title: "The report's side chat reads the pull request's workspace"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2370--BPR-2370--e10664743876.json","origin":"owner, 2026-11-24","related_cards":["BPR-2286","BPR-2322","BPR-1900"],"related_rfcs":["RFC-BPR-0096","RFC-BPR-0101"]}
---

# BPR-2370: The report's side chat reads the pull request's workspace

## Owner (2026-11-24)

"Why does the report side chat not have access to the code? How am I supposed
to ask serious follow-ups if it cant read the workspace?"

## Why it could not

The item side chat ([[BPR-2322]]; Ask follow-up on a report's finding or a
finding card, Ask about this on a draft or a thread) kept [[BPR-2286]]'s
rounds: "each round is one fresh, tool-free run of the window's agent on its
own lane", on the local draft's tool-free runner (`WritingOnly`, an empty
folder, no tools), told "You have no tools". That was the implementer's
recorded design decision on BPR-2286 ("How rounds run. As Draft Description
does"), carried into BPR-2322 ("fresh tool-free rounds"). No owner words chose
it: the owner asked on 2026-11-19 to ask follow-ups and update comments, and
on 2026-11-20 for "something akin to the side chat from the tour". A tour
part's chat (BPR-1900) reads the workspace with read-only tools.

## Change

- A round launches as a tour part's chat turn does: `ReadOnlyTools` in the
  pull request's workspace at the revision under review, through the same
  `agent_invocation_host` workspace port, history clone and read-only launch
  policy (MCP off, no grant, no edits, no network), path `follow-up` (fresh;
  the message still carries the item and the earlier rounds).
- It runs on the side chat's own `agent_invocation_host` (`FollowUpSession`,
  its own effect lane, launcher and projection), so a round never waits on or
  reaches the tour's or the Agent tab's session. One round at a time across a
  window's item chats; a second item's question meanwhile says why and sends
  nothing.
- `application::follow_up_host` gains a launcher (`set_launcher`): a round
  starts on the decision lane and answers later, settling exactly as the
  runner's answers do (a stop, a failure, a refusal before start, shutdown).
- The skills a question names still ride inline with the round (the
  supplement's skill fields and the prompt's closing directive), and the
  round keeps the Writing style it was asked with, as on the tool-free
  runner before.
- The answer is the run's last message (what it said while reading is its
  working). `core::follow_up_instructions` tells the agent its working folder
  is a private read-only copy of the pull request and that a rewrite's last
  message must be only the text.
- The agent it needs is one with a read-only mode (Claude or Codex), as a
  tour part's chat needs; otherwise the chat says so and sends nothing.
- No workspace setup step is added: as for a tour part's chat and the Agent
  tab, the invocation host makes the workspace when a round starts.

## Acceptance

- AC1. Host: a launcher's round is given the round's agent, its Writing
  style and its question's skills; it starts at once, not on the agent lane; its
  answer settles on the decision lane as a round; Stop reaches the launched
  run's token and its late answer counts as stopped; a round refused before
  it starts says why; after shutdown nothing is published and the host is
  quiescent. The runner path is unchanged.
- AC2. Public path with the fixture agent: the report's Ask follow-up (two
  rounds) and the draft composer's Ask about this each run in the pull
  request's workspace (the fixture agent finds `src/engine/renderer.cpp` in
  its working folder), the round with a named skill still carries only
  that skill's guidance and a later round none, and every earlier check of the course still holds
  (proposals, Use, Draft comment, logging with no words).
- AC3. Documentation: `review-tours.md` and `ai-experience.md` say the side
  chat reads the workspace read-only.

## Verification

macOS, dev preset, `d90638de9` (rebased on main `fc4a7f6cc`). `coding-format`
on every changed C++ file; `tidy.py` on the changed translation units reports
nothing in this change's lines (its findings are in code this change did not
touch). The production app manifest digest is resealed against the merged
tree, keeping the BPR-2365 and BPR-2366 notes.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `application_follow_up_host` (launcher receives the round's style and skills; start, stop, refusal, shutdown) | pass |
| AC1, AC2 | `core_agent_follow_up` | pass |
| AC2 | `application_follow_up_public_path` (the report's two rounds, one with a named skill, and the draft's round ran in the pull request's workspace; skill guidance only on the round that named it) | pass |
| AC2 | `application_agent_chat_composition_contract` (launch call sites now nine) | pass |
| all | `architecture_boundaries`, `coding_format_check` | pass |

## Owner judgement

- Rounds are fresh runs that make their own workspace copy each time (as a
  tour Enhance does), not the pull request's retained native workspace the
  Agent tab and a tour part's chat resume in; each round pays the copy.
- Codex is now eligible for the side chat (read-only), where before only a
  Claude stream-JSON agent was.
- One round at a time per window across item chats.
- No attended look was prepared.
