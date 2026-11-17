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

Evidence for each of AC01 to AC13 against the
[RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md), the owner's accepted look (AC09), the documentation gate, and
the RFC's status in its front matter and in the index.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** No product text, accessible name or setting calls a Result or Feature a skill; the editor, picker and Settings say Result and Feature, and `/skill-name` references are still called skills. | Not run | No producing revision |
| **AC02:** Every prompt has exactly one Result: saving without one is refused, a place or surface its Result cannot open in is not offered, and changing the Result to one its placements do not allow names those placements and does not save. | Not run | No producing revision |
| **AC03:** With the Fixture agent, Review this pull request whose text is replaced by one sentence with no format guidance still opens a report tab with a verdict, a summary with Keep summary, and a Draft comment on each finding with a place; likewise an Answer prompt with `/bpr:suggested-comment` and no format guidance still shows Draft comment. | Not run | No producing revision |
| **AC04:** For every shipped prompt and every flow, each instruction byte of the captured request (the system prompt and the message's instructions, excluding the listed material) appears in that prompt's editor as editable text or a read-only section, including the application contract, every contract and every inlined skill. | Not run | No producing revision |
| **AC05:** Typing `/` in the prompt editor lists Features beside the personal and repository skills, each marked by source; `/bpr:` lists Features only; a Feature the Result does not accept is listed disabled with its reason; choosing a row inserts its reference. | Not run | No producing revision |
| **AC06:** A prompt naming `/bpr:suggested-change` and a repository `/repo:<name>` skill produces requests for a Claude agent and a Codex agent that each carry the Feature's contract and the skill's text in the same form, with no native skill loading; a fifth `/skill` is noted as today, and Features do not count toward the four. | Not run | No producing revision |
| **AC07:** Suggested comment yields Draft comment carrying the agent's wording; Suggested change yields a suggestion block kept at its lines; Proposed fix yields a diff with Apply, or Apply disabled with its reason where nothing can be written; an omitted or malformed block leaves the rest of the Result presented, with a note that the agent gave none. | Not run | No producing revision |
| **AC08:** Stored macros of `prompt_ask::question` and `prompt_ask::review` load as Answer and Findings report with their text unchanged, and the shipped review prompts carry `/bpr:suggested-comment` and `/bpr:suggested-change`. | Not run | No producing revision |
| **AC09:** The editor's Result field, read-only sections, `/bpr:` completion, Compare and the dropped-overrides notice are reachable and operable by keyboard and named to assistive technology, and the owner has accepted the editor in a look at a frozen Fixture-agent build. | Not run | No producing revision |
| **AC10:** Every flow (each `core::writing_hook`, the report guidance and the suggested-comment format) is a shipped prompt in Settings › Prompts with its Result; editing its text changes what the next press of that flow sends; the writing-hooks panel and its Built-in prompts list are gone. | Not run | No producing revision |
| **AC11:** A profile holding writing-hook overrides, started once on the new build, has none left and is shown the notice naming each dropped prompt once, and not on the next start; a profile without overrides sees no notice; an exchange file with overrides imports without them, and its summary names them. | Not run | No producing revision |
| **AC12:** Across a built-in revision, an untouched shipped prompt sends the new text with no flag; an edited one keeps and sends its own text and shows A newer built-in version exists, whose Compare shows both texts and whose Use built-in replaces the text only when pressed; a contract change reaches both with no flag. | Not run | No producing revision |
| **AC13:** Core decides which Results a place allows, which Features a Result accepts, the instructions a request carries and the actions an output unlocks; the host performs those actions, Qt presents them, and no widget composes instruction text. | Not run | No producing revision |
