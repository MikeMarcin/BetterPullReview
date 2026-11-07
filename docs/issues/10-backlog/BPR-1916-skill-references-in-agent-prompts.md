---
schema: "implement-rfc.card.v1"
id: "BPR-1916"
title: "Agent prompts can reference a skill from the person's own skills or the reviewed repository"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1900","BPR-1914","BPR-1915","BPR-1917"]}
acceptance: []
---

# BPR-1916: Agent prompts can reference a skill from the person's own skills or the reviewed repository

## Owner (2026-11-06)

"having used it for a while I want to do more than just enhance the tour text with my agent. I want to ask questions, get back and forth and have it update it. For example right now I have Solution section where the draft said something I don't understand, but I don't really have a way to ask for clarification. It also feels like the output of the draft is entirely devoid of any formatting. It feels just like big walls of text. I have a skill here for /technical-book-humanizer but I also have a skill elsewhere for technical writing that is slightly more tuned to those purposes. I wonder if I can't have a hook to let me reference a custom skill in my prompts if the skill is available in as a personal user skill or a skill in the repo being reviewed. I also wonder if we can improve the settings agent tab where we're editing the built-in prompts its a bit weak maybe some /frontend-design:frontend-design could improve that."

## Change

- In any prompt the person writes or edits (Tour tab asks and drafting, Agent tab chat, and the built-in prompts in Agent settings), `/skill-name` references a skill. Offer completion of the available names as you type.
- **Sources:** the person's own skills (for Claude, `~/.claude/skills/*/SKILL.md`; record the equivalents for other agents) and skills in the reviewed repository (`.claude/skills/`, `.agents/skills/`, and similar; record the list).
- **Trust:** a repository skill is content the pull request can change. Read repository skills from the base side (the PR's base commit, or the linked or kept clone's default branch), never from the PR head, and show which source a skill came from. A personal skill wins over a repository skill with the same name, unless the person picks otherwise. Record this in the threat model.
- **Mechanism:** today's writing and read-only runs disable skills and settings (`--setting-sources ""`, `--disable-slash-commands`). Choose and record one mechanism:
  1. Inline the referenced SKILL.md (bounded, with its supporting files listed but not loaded) into the prompt as clearly delimited guidance.
  2. Let the agent load that one skill through its own skill tool.
  Recommend (1): it keeps runs tool-bounded and works for every agent kind.
- **Skill not found:** an unknown `/name` is left as text, with a quiet note.

## Acceptance

- AC1. `/technical-book-humanizer` in a tour drafting prompt makes the run receive that skill's guidance, checked through the stub's received prompt.
- AC2. A repository skill is read from the base side. A PR that changes it doesn't change what the run receives (a fixture test).
- AC3. Completion lists personal and repository skills with their source.
- AC4. Writing and read-only runs still pass every BPR-1375, BPR-1891, BPR-1896, BPR-1899 and BPR-1912 check.
- AC5. `ai-agent-boundary.md`, `security-threat-model.md` and an RFC-BPR-0092 amendment record the sources, the trust rule and the mechanism, and the documentation gate passes.
