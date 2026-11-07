---
schema: "implement-rfc.card.v1"
id: "BPR-1914"
title: "The tour session is a conversation: ask about the tour, get answers, and have it propose edits"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900","BPR-1913"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1900","BPR-1912","BPR-1913","BPR-1915","BPR-1916"]}
acceptance: []
---

# BPR-1914: The tour session is a conversation: ask about the tour, get answers, and have it propose edits

## Owner (2026-11-06)

"having used it for a while I want to do more than just enhance the tour text with my agent. I want to ask questions, get back and forth and have it update it. For example right now I have Solution section where the draft said something I don't understand, but I don't really have a way to ask for clarification. It also feels like the output of the draft is entirely devoid of any formatting. It feels just like big walls of text. I have a skill here for /technical-book-humanizer but I also have a skill elsewhere for technical writing that is slightly more tuned to those purposes. I wonder if I can't have a hook to let me reference a custom skill in my prompts if the skill is available in as a personal user skill or a skill in the repo being reviewed. I also wonder if we can improve the settings agent tab where we're editing the built-in prompts its a bit weak maybe some /frontend-design:frontend-design could improve that."

## Change

Load `frontend-design:frontend-design` first.

1. **Ask the tour agent.** The Tour tab gets a way to talk to the tour session about the tour. You can ask about a selected passage of Problem, Solution or a stop ("What does this mean?"), or ask a free question about the tour or the change. The agent answers in the same read-only workspace session (BPR-1900 tools, BPR-1912 rules). The answer reads as a reply, not as an edit.
2. **Back and forth.** Follow-ups keep the conversation. This supersedes RFC-BPR-0092 Amendment BPR-1900's T1 (fresh run per request): the tour session keeps one conversation per tour, with the tour so far given fresh on each turn. Record how the session resumes for each agent kind (Claude `--resume` with saved session state, Codex equivalents, or a replayed transcript) and what is stored where.
3. **Have it update the tour.** From the conversation you can ask it to change the tour ("rewrite that sentence so it says X", "add a stop on the retry loop"). Changes come back as BPR-1900 proposals: per field and per stop, accept/edit/discard, with the hunk placement and drop rules. They never silently overwrite.
4. **Where it lives.** It's next to the tour, not in the editing column's way, and coherent with BPR-1913's transcript window: either the transcript window becomes the conversation, or a compact ask box sits next to the selected field. Choose with the skill, and keep the transcript copyable.
5. Asking about a selected passage is a first-class action: select text, then "Ask about this".

## Acceptance

- AC1. Selecting a passage in Solution and asking "what does this mean?" gets an answer in the conversation that cites the code it read.
- AC2. A follow-up question keeps context from the earlier turn (test with a stub that checks the resumed context).
- AC3. "Rewrite that so it says X" returns a proposal for that field, and accepting it updates the field. Nothing changes without acceptance.
- AC4. The conversation is copyable as a whole (BPR-1913's rules).
- AC5. Owner look: attended Accept/Reject on a frozen build that shows ask, answer, follow-up and an accepted rewrite.
- AC6. The RFC amendment supersedes T1 and records the resume mechanism, and the documentation gate passes.
