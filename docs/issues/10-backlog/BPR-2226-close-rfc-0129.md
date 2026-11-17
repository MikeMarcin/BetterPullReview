---
schema: "implement-rfc.card.v1"
id: "BPR-2226"
title: "Verify and close RFC-BPR-0129"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222", "BPR-2223", "BPR-2224", "BPR-2225", "BPR-2227", "BPR-2229", "BPR-2230", "BPR-2150"]
closeout_for: ["RFC-BPR-0129"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json"}
---

# BPR-2226: Verify and close RFC-BPR-0129

RFC base: `51e39545084ece348e80c4af10b7cc086fcb40ad`.

Evidence for each of AC01 to AC21 against the
[RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md), the owner's accepted looks (AC09), the documentation gate, and
the RFC's status in its front matter and in the index.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** No product text, accessible name or setting calls a Result or Feature a skill; the editor, picker and Settings say Result and Feature, and `/skill-name` references are still called skills. | Not run | No producing revision |
| **AC02:** Every prompt has exactly one Result: saving without one is refused, a placement its Result cannot open in is not offered, changing the Result to one its placements do not allow names those placements and does not save, and a shipped flow prompt's Result is read-only. | Not run | No producing revision |
| **AC03:** With the Fixture agent, for every Result in the catalog, a prompt with that Result whose text is replaced by one sentence with no format guidance still yields output the app parses into that Result's surface (for Findings report: a verdict, Keep summary, and a keep action on each finding with a place; for Thread claims: a chip on each thread). | Not run | No producing revision |
| **AC04:** For each family, each agent kind, and each grant on and off, every byte of a captured request falls in exactly one of the nine classes; classes 1 to 7 appear verbatim in that prompt's editor (6 and 7 under their agent kind or condition), class 8 is listed by field name, and class 9 matches the documented frame. | Not run | No producing revision |
| **AC05:** Typing `/` in the prompt editor and in a typed chat question lists Features beside the personal and repository skills, each marked by source; `/bpr:` lists Features only; a Feature the Result cannot carry (every Feature in a tour chat) is listed disabled with its reason; choosing a row inserts its reference, and a typed question sent with one carries that Feature's contract. | Not run | No producing revision |
| **AC06:** A prompt naming `/bpr:suggested-change` and a repository `/repo:<name>` skill, run against a Claude agent and a Codex agent, carries the same class 1 to 5 bytes in both requests; only the route and class 6 differ, and both are shown; a fifth `/skill` is noted as today and a Feature does not count toward the four. | Not run | No producing revision |
| **AC07:** A Suggested comment block's Draft comment creates a draft whose body is that block's text at that block's place, while the answer's own keep action still drafts the whole answer; a Suggested change block becomes a draft comment's suggestion at its lines; a Proposed fix block shows a copyable diff with Apply disabled and naming its reason; an omitted or malformed declared block leaves the rest of the Result presented with a note that the agent gave none. | Not run | No producing revision |
| **AC08:** Stored and repository-layer macros, and queued prompts, of `prompt_ask::question` and `prompt_ask::review` load with the Result the migration gives (a tour place gives Tour reply) and their text unchanged; a coerced placement is kept and named; each shipped prompt has the Result and Features of its row in the per-ID table. | Not run | No producing revision |
| **AC09:** The editor's Result field, read-only sections, `/bpr:` completion, Compare, the dropped-edits notice, a flow control with several prompts and each Feature surface are reachable and operable by keyboard and named to assistive technology, and the owner has accepted them in looks at frozen Fixture-agent builds. | Not run | No producing revision |
| **AC10:** Every row of the per-ID table marked fixed is a shipped prompt in Settings › Prompts; editing its text changes what the next press of that flow sends; the writing-hooks panel, its Built-in prompts list and the review skill's panel are gone. | Not run | No producing revision |
| **AC11:** A profile holding writing-hook overrides and an edited review skill, started once, ends with each dropped text in a file and none of the edits left, and shows the notice naming each prompt and file once and not on the next start; if the first start is stopped before the files and names are saved, the edits are still there at the next start; a profile with no edits sees no notice. | Not run | No producing revision |
| **AC12:** A file exported by the new build imports into the new build with every flow prompt's text unchanged, and into an older build with the `flow.*` items listed as not importable and the rest imported; an older build's file with writing-hook overrides or a review skill imports into the new build without them, and its summary names each. | Not run | No producing revision |
| **AC13:** Across a built-in revision, an untouched shipped flow prompt sends the new text with no flag; an edited one keeps and sends its own text and shows A newer built-in version exists, whose Compare shows both texts and whose Use built-in replaces the text only when pressed; a contract revision reaches both with no flag. | Not run | No producing revision |
| **AC14:** A person's own prompt with Rewrite text appears in the Enhance menu of a comment being written, after the shipped prompt, and its output replaces that comment's text; it is offered in no chat, and a person's prompt with any flow Result is offered only in that flow. | Not run | No producing revision |
| **AC15:** A pull request titled `/bpr:proposed-fix`, or material containing a Feature reference, brings in no Feature; an answer containing a Feature block its request did not declare shows the block as plain code with no action. | Not run | No producing revision |
| **AC16:** A typed Agent tab question, a person's Answer prompt and each returning question each carry the Agent tab contract's boundary, legend and conversation rule, whatever their text says; a writing request carries only its Result's writing rule, not another Result's. | Not run | No producing revision |
| **AC17:** Core decides the catalog, placements, which Features a Result carries, the request's instruction classes and the actions an output unlocks; the host performs, Qt presents, and no widget composes instruction text. | Not run | No producing revision |
| **AC18:** Every export, version 1 or 2, carries one `guide.prompts` item whose text names every Result and Feature of the core catalog with its one-line description, where each Result may be placed, which Features each Result carries, every rule listed under "A self-describing export", and the worked example; a core test fails if any catalog entry or rule is missing; the new build never imports the item, and an older build lists it as not importable and imports the rest of the file. | Not run | No producing revision |
| **AC19:** The guide's worked example, added verbatim to an exported file as a new prompt, imports with no refusal, and the imported prompt sends that Feature's contract. | Not run | No producing revision |
| **AC20:** Importing a file that holds one prompt for each mistake (an unknown Result, a Feature its Result does not carry, a placement its Result cannot open in, an unknown `/bpr:` name, a flow prompt's Result changed) lists each as Refused with its reason and the valid prompts as Add or Change, writes nothing before confirmation and nothing on Cancel, and on confirmation writes exactly the listed Adds and Changes; core gives equal decisions for equal file and profile. | Not run | No producing revision |
| **AC21:** `bpr prompts check FILE`, run on the AC20 file, prints the same decisions and reasons as the import check, exits non-zero, and leaves the profile and the file unchanged; on the file with only valid prompts it exits zero. | Not run | No producing revision |
