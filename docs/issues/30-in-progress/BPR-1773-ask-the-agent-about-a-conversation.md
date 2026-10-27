---
schema: "implement-rfc.card.v1"
id: "BPR-1773"
title: "Ask the agent about a conversation: review threads and private notes, with the code they are on"
status: "In Progress"
rfcs: ["RFC-BPR-0094"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1772--BPR-1773--b27f9d2285df.json","kind":"feature","found_by":"owner chat, 2026-10-26","authoring":"frontend-design:frontend-design loaded for the Qt surfaces","related_cards":["BPR-1594","BPR-1753","BPR-1245"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d7322f02675655a3cd04dc58cf7a52c6b12498b6","evidence":{"kind":"command","detail":"core_agent_chat_scope (eighth entry point, no selection needed) and core_agent_context_supplement (test_a_conversation_travels_bounded_or_refuses_the_request: bounds, opening and newest comments, excerpt window, refusals, carried unchanged or refused whole)."}},{"criterion":"AC2","outcome":"pass","revision":"d7322f02675655a3cd04dc58cf7a52c6b12498b6","evidence":{"kind":"command","detail":"application_agent_launch, test_a_conversation_is_read_and_crosses: read_conversation (current and original lines, deleted comments left out) and every conversation.* field after the version-two fields; none without one."}},{"criterion":"AC3","outcome":"pass","revision":"d7322f02675655a3cd04dc58cf7a52c6b12498b6","evidence":{"kind":"command","detail":"qt_conversation_cards_view (header button and menu name the thread once each), qt_workbench_window_scale_100 (chip in the field, prompts, About line, Use as reply only for thread turns, chip close, note prompts), qt_private_notes_panel."}},{"criterion":"AC4","outcome":"pass","revision":"d7322f02675655a3cd04dc58cf7a52c6b12498b6","evidence":{"kind":"command","detail":"application_agent_chat_conversation_contract: Ask agent from a fixture thread's menu through the shipped composition, then a send: the turn is ask_about_conversation, carries that thread (path, comments, valid bounds), runs once and mutates nothing on GitHub; the one launch path is unchanged. application_private_review_host note_press: the note's text becomes one reviewer revision and the run's only private source; note.count stays 0."}}]
---

# BPR-1773: Ask the agent about a conversation

Implements [RFC-BPR-0094](../../rfcs/RFC-BPR-0094-ask-the-agent-about-a-conversation.md),
whose problem, owner decisions and criteria this card does not repeat.

## Slices

- Core: the entry point and the bounded conversation (`686c8c448`).
- Application, composition and Qt: threads, notes, the chip, Use as reply.

## Evidence

AC1 to AC4 pass at the recorded revision (front matter). Thirteen focused
ctest entries pass on macOS dev, including the composition courses
`application_agent_chat_composition_contract` (unchanged) and
`application_agent_invocation_composition_contract`.

## Pending

- AC5, the owner's look on a frozen build (attended review): not prepared.
  The disk filled during this card's full build; a frozen root needs its own
  build tree.
- Not run: the full suite, `coding-tidy`, Windows and Linux.
