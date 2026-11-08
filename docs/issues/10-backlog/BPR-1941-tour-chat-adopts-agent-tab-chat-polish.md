---
schema: "implement-rfc.card.v1"
id: "BPR-1941"
title: "Tour chats show the Agent tab's live run status and adopt its chat polish"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-08, coordinator chat","related_cards":["BPR-1914","BPR-1925","BPR-1934","BPR-1876","BPR-1880"]}
acceptance: []
---

# BPR-1941: Tour chats show the Agent tab's live run status and adopt its chat polish

## Owner (2026-11-08)

"I want to bring the agent status updates from the agent tabs over to the
tour tab's side chat. the '<agent> is thing <elapsed time>' or whatever it
has is much better than the tour side chat's little status disclosure thingy
that has no useful info. There might be more better ux in the agent chat to
bring over, it just feels more polished overall."

## Change

Load `frontend-design:frontend-design` first.

- **Live status.** While a tour chat turn runs (any part's chat or the
  whole-tour chat, and Draft Tour too, where it shows status), show the Agent
  tab's live run status line: the agent's name, what it is doing now (e.g.
  reading or searching), and the elapsed time, updating live. Reuse the Agent
  tab's component and its source of run events. Don't re-implement it.
  Replace the tour chat's "Worked for Ns" disclosure, or keep a completed
  run's steps only if the Agent tab shows them the same way.
- **Audit and adopt.** Compare the Agent tab's chat with the tour chat, and
  adopt what fits. Candidates:
  - turn footers ("<agent> answered in 0:12");
  - failure turns with Resend, Delete and "Fixed since" (BPR-1876 and
    BPR-1880);
  - Stop while running;
  - the run's steps as the Agent tab shows them;
  - message layout and spacing;
  - copy affordances;
  - scrolling behaviour while a turn streams.

  Prefer sharing components over copying. List what you adopted and what you
  left out, with reasons, on the card.
- Keep the tour-specific pieces: "Update … from this chat", proposals in
  place, suggested prompts (BPR-1934), and refusal wording (BPR-1937).

## Acceptance

- AC1. A running tour chat turn shows the agent, its current activity and a
  live elapsed time, the same as the Agent tab (Qt test with a stub run).
- AC2. Each adopted Agent tab behaviour works in tour chats. The card lists
  what was adopted and what was skipped.
- AC3. Tour-specific features are unchanged (existing tests stay green).
- AC4. Owner look: Accept/Reject on a frozen build.
- AC5. The documentation gate passes.
