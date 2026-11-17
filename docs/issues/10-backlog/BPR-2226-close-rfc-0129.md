---
schema: "implement-rfc.card.v1"
id: "BPR-2226"
title: "Verify and close RFC-BPR-0129"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222", "BPR-2223", "BPR-2224", "BPR-2225"]
closeout_for: ["RFC-BPR-0129"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json"}
---

# BPR-2226: Verify and close RFC-BPR-0129

RFC base: `51e39545084ece348e80c4af10b7cc086fcb40ad`.

Evidence for each of AC01 to AC14 against the
[RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md), the owner's accepted look (AC09), the documentation gate, and
the RFC's status in its front matter and in the index.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** No product text, accessible name or setting calls a Result or Feature a skill; the editor, picker and Settings say Result and Feature, and `/skill-name` references are still called skills. | Not run | No producing revision |
| **AC02:** Every prompt has exactly one Result: saving without one is refused, a place or surface its Result cannot open in is not offered, changing the Result to one its placements do not allow names those placements and does not save, and a shipped flow prompt's Result is shown read-only and cannot be changed. | Not run | No producing revision |
| **AC03:** With the Fixture agent, Review this pull request whose text is replaced by one sentence with no format guidance still opens a report tab with a verdict, a summary with Keep summary, and a Draft comment on each finding with a place; likewise an Answer prompt with `/bpr:suggested-comment` and no format guidance still shows Draft comment. | Not run | No producing revision |
| **AC04:** For every shipped prompt and every flow, each instruction byte of the captured request (the system prompt and the message's instructions, excluding the listed material) appears in that prompt's editor as editable text or a read-only section, including the application contract, every contract and every inlined skill. | Not run | No producing revision |
| **AC05:** Typing `/` in the prompt editor and in a typed chat question lists Features beside the personal and repository skills, each marked by source; `/bpr:` lists Features only; a Feature the Result does not accept is listed disabled with its reason; choosing a row inserts its reference, and a typed question sent with one carries that Feature's contract. | Not run | No producing revision |
| **AC06:** A prompt naming `/bpr:suggested-change` and a repository `/repo:<name>` skill produces requests for a Claude agent and a Codex agent that each carry the Feature's contract and the skill's text in the same form, with no native skill loading; a fifth `/skill` is noted as today, and Features do not count toward the four. | Not run | No producing revision |
| **AC07:** Suggested comment yields Draft comment carrying the agent's wording; Suggested change yields a suggestion block kept at its lines; Proposed fix yields a diff that can be copied, with Apply disabled and naming its reason while no local-draft worktree exists; an omitted or malformed block leaves the rest of the Result presented, with a note that the agent gave none. | Not run | No producing revision |
| **AC08:** Stored macros of `prompt_ask::question` and `prompt_ask::review` load as Answer and Findings report with their text unchanged, and the shipped review prompts carry `/bpr:suggested-comment` and `/bpr:suggested-change`. | Not run | No producing revision |
| **AC09:** The editor's Result field, read-only sections, `/bpr:` completion, Compare and the dropped-overrides notice are reachable and operable by keyboard and named to assistive technology, and the owner has accepted the editor in a look at a frozen Fixture-agent build. | Not run | No producing revision |
| **AC10:** Every flow (each `core::writing_hook`, the report guidance, the suggested-comment format and the review skill's guidance) is a shipped prompt in Settings › Prompts with the Result the mapping gives it; editing its text changes what the next press of that flow sends; the writing-hooks panel, its Built-in prompts list and the review skill's panel are gone. | Not run | No producing revision |
| **AC11:** A profile holding writing-hook overrides and an edited review skill, started once on the new build, has neither left and is shown the notice naming each dropped prompt once, and not on the next start; a profile with neither sees no notice; an exchange file with overrides imports without them, and its summary names them. | Not run | No producing revision |
| **AC12:** Across a built-in revision, an untouched shipped prompt sends the new text with no flag; an edited one keeps and sends its own text and shows A newer built-in version exists, whose Compare shows both texts and whose Use built-in replaces the text only when pressed; a contract change reaches both with no flag. | Not run | No producing revision |
| **AC13:** Core decides which Results a place allows, which Features a Result accepts, the instructions a request carries and the actions an output unlocks; the host performs those actions, Qt presents them, and no widget composes instruction text. | Not run | No producing revision |
| **AC14:** A person's own prompt with Rewrite text appears in the Enhance menu of a comment being written, after the shipped prompt, and its output replaces that comment's text; the same prompt is offered in no chat, and a person's prompt with any flow Result is offered only in that flow. | Not run | No producing revision |
