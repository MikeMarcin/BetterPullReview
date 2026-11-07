---
schema: "implement-rfc.card.v1"
id: "BPR-1916"
title: "Agent prompts can reference a skill from the person's own skills or the reviewed repository"
status: "In Progress"
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

## Owner decisions (2026-11-06)

- **Trust.** First "repository skills come from the base side only, never the
  PR head" ("you could be reviewing a PR you dont trust"), then revised:
  "i guess head is fine. We will only run skills you configure right?" Head
  is allowed on the condition that only skills the person names in a prompt
  reach a run. Recorded in the threat model.
- **Mechanism.** Inline, for every run and agent kind: "unfortunately my
  custom wrapper right now seems to throw away all my settings on load its
  own, so we can't rely on that" (BPR-1896 found the same). No run gains the
  `Skill` tool, a plugin, a setting source or a permission rule.

## Native route, checked on installed Claude Code 2.1.310 (2026-11-06)

In a scratch folder with a fixture project skill, print mode, stream JSON,
BPR-1900's read-only flags:

- `--setting-sources ""`: the init frame's `skills` lists only Claude's
  bundled skills; neither a personal nor a project skill loads, so a
  `.claude/skills` in the working folder (the head) is not picked up.
- `--setting-sources project`: the folder's skill loads, and so does its
  `CLAUDE.md` (the run obeyed an instruction in it); project
  `permissions.allow` was ignored in an untrusted folder.
- `--setting-sources user`: every user plugin, agent and setting comes too.
  No narrow switch for skills alone; `--add-dir` does not load skills.
- `--plugin-dir` with a staged plugin holding one skill loads only that
  skill (`bpr-skills:NAME`, plugin listed in the init frame).
  `Skill(bpr-skills:NAME)` in `--allowedTools` denied another skill the
  model tried in print mode. `/name` at the start of a prompt expands with
  no Skill tool; named mid-prompt it loads only if the model calls the tool.
- The init frame reports `tools`, `skills`, `slash_commands`, `plugins`
  (name, path, source) and `agents`.

Rejected for the owner's reason above; recorded as evidence in the
[AI agent boundary](../../architecture/ai-agent-boundary.md#skills-a-prompt-names-bpr-1916).

## Implementation (2026-11-06)

- Core `skill_references`: finding `/name` (`/repo:` and `/personal:`
  qualifiers), resolution (personal first), the quiet note, completion rows
  with source, the merged catalog. `read_skill_front_matter` beside the
  review skill's reader.
- Application: `skill_guidance` (documents, library, inline fields, the
  directive, the BPR-1917 preview), `skill_catalog` (personal folders
  `~/.claude/skills`, `~/.agents/skills`, `~/.codex/skills`; the loader on
  the effect lane), `repository_skill_port`; supplement `skill.N.*` fields
  (`core::supplement_skill`, bounded, refused whole outside bounds).
- Git: `make_installed_git_skill_reader` reads `.claude/skills`,
  `.agents/skills`, `.codex/skills` at the base commit from the linked
  clone's objects through the publication catalog's private store.
- App: the catalog loads on start, review change and clone link change; a
  writing hook's skills ride on Draft from PR context, Suggest an outline,
  Enhance, Generate comment and the Settings test; an Agent tab question's
  skills ride with it. A fixture composition reads no personal skills unless
  `--attended-skill-home` names a fixture home.
- Qt: `skill_completer` on the Settings prompt editor, the Agent tab's
  question field and the Tour conversation's ask box ([[BPR-1914]]); an ask
  turn carries the skills its question names inline, as every run does
  (`qt_tour_agent_session_view` checks completion and Return in the ask box).
- Tests: `core_skill_references`, `application_agent_launch`
  (`test_skill_references_reach_the_run`: inline fields for writing,
  read-only and Codex runs, read-only argv unchanged, a `Skill` tool or
  tool use refused), `repository_skill_reader` (base not head, a head-added
  and a working-tree skill never reach a run, a lure skill never sent unless
  named), `qt_settings_window` (completion rows with source).
