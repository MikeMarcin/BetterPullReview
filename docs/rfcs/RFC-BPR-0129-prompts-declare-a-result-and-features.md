---
id: "RFC-BPR-0129"
title: "Prompts declare a Result and Features; nothing the agent is told is hidden"
status: "Proposed"
metadata: {"origin":"owner grill-rfc interview, 2026-11-16, after BPR-2219 (the review report lost its format and Draft comment)","inspected_revision":"51e39545084ece348e80c4af10b7cc086fcb40ad","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d at 2485c52aa","mandate":"owner, 2026-11-16: the design below confirmed in a grill-rfc interview; cards BPR-2222 to BPR-2226 filed in Backlog, none started"}
depends_on: ["RFC-BPR-0092", "RFC-BPR-0101", "RFC-BPR-0107", "RFC-BPR-0125", "RFC-BPR-0128"]
---

# Prompts declare a Result and Features; nothing the agent is told is hidden

## Problem and grounded current behavior

What a BetterPullReview agent run is told comes from four places, and the
person can see and edit only some of them.

- **Suggested prompts** (RFC-BPR-0125, `core::prompt_macro`) have an
  editable template and an *Ask as*, either `prompt_ask::question` or
  `prompt_ask::review`. The shipped set is in
  `src/core/src/prompt_macro_shipped.cpp`. The kind decides what the answer
  becomes (a reply, or a review with findings), but the instructions that
  make the answer parse are not in the template.
- **Hidden instruction files.** Eighteen files under
  `src/application/resources/review-preparation/` are compiled in and sent
  with runs. They include the noncustomizable `application-contract.md`, the
  report shape `report-templates.md` (`report_templates_guidance()`, which
  nothing can edit), the `suggested-comment.md` format that every Agent tab
  request carries, and one file for each flow (`problem-statement`,
  `solution-summary`, `tour-outline`, `tour-stop-comment`, `enhance-*`,
  `local-*`, `review-*`).
- **Writing hooks** (`application/writing_hooks.hpp`,
  `core::writing_hook_settings`). A profile may override sixteen of those
  files. Each override is versioned (`*_hook_version`) and shown in the
  writing-hooks panel (`qt/writing_hooks_panel.cpp`, the "Built-in prompts"
  list that [[BPR-2179]] moved into Settings › Prompts). An override
  replaces the whole file, guidance and output format together.
- **The review skill** `bpr-pr-review` (RFC-BPR-0092 D1,
  `application/review_skill_host.hpp`). It is edited in its own panel and
  has its own "Version N is available" and Keep mine.

A prompt can also name an agent skill as `/skill-name`, `/repo:name` or
`/personal:name`. These are found in `~/.claude/skills`, `~/.agents/skills`,
`~/.codex/skills` and the reviewed repository at its base commit
(`core/skill_references.hpp`, [[BPR-1916]]). The host inlines each named
`SKILL.md` into the request's supplement, the same way for every agent kind
(`application/skill_guidance.hpp`).

Guidance and output format share one editable text, so an edit made for the
guidance can quietly break what the app does with the answer. [[BPR-2219]]
is the case: a review answer stopped reading as a report and lost Draft
comment. BPR-2219 fixes that one prompt by making the review guidance its
editable body and appending a read-only report-format contract. This RFC
makes that split the rule for every prompt and names its parts.

## Proposed behavior and boundaries

### Terms

- A **Result** is what a prompt produces: a fixed output contract and the
  app action it unlocks (where the answer opens, which buttons it carries).
- A **Feature** is an add-on the prompt asks for inline: a fixed output
  contract for one kind of block, and the action that block unlocks.
- A **skill** is still only an agent skill the person or the repository has
  (`/skill-name`). The product never calls a Result or a Feature a skill.

> Decision (owner, 2026-11-16): "It's confusing to call these skills." The
> main one is a **Result**; the inline add-ons are **Features**; real agent
> skills remain skills and keep working.

### A prompt is its text, one Result, and any Features

- **Guidance is the prompt's text.** What to look for, which sections, tone
  and length all live in the prompt's own editable text. A contract carries
  only the output shape the app parses: block names, fields, and any order
  a parser relies on.
- **Contracts are always appended and never hidden.** Whatever the text
  says, the request carries the Result's contract and each referenced
  Feature's contract after the text. The editor shows each contract,
  read-only, below the text. If someone deletes every word of format
  guidance from the text, the output still parses.

> Decision (owner, 2026-11-16): a Result or Feature is a fixed output
> contract plus the app action it unlocks. All guidance lives in the
> prompt's editable text; contracts are appended and shown read-only.

- **Nothing the agent is told is hidden.** Every instruction a run sends
  appears in that prompt's editor, as editable text or in a read-only
  section. This includes the application contract (`application-contract.md`),
  the Result and Feature contracts, the inlined skills (by name, with their
  text one press away) and any guidance a flow adds. The run's *material*
  (the diff, the thread, the description) is untrusted evidence, not
  instruction. A read-only section lists it by what it is, without printing
  it.

### Exactly one Result

- Each prompt has one required **Result**. It is a field of its own in the
  editor, not part of the text, and a prompt without one cannot be saved.
- The Result decides where the output opens and limits where the prompt can
  be placed (RFC-BPR-0125's places and [[BPR-2179]]'s surfaces). The editor
  does not offer a place or surface the Result cannot open in. Changing the
  Result to one that its current placements do not allow names those
  placements, and the change does not save until they are removed.

> Decision (owner, 2026-11-16): "I'm okay for the 'main skill' to be
> separate and required."

### Features, inline as `/bpr:<name>`

- A Feature is referenced in the text as `/bpr:<name>`, for example
  `/bpr:suggested-comment`. The completion picker that offers the person's
  and the repository's `/skills` offers Features too, in the prompt editor
  and in any composer with `/` completion. Each row is marked
  BetterPullReview, personal or repository, and typing `/bpr:` narrows the
  list to Features. A Feature that the prompt's Result does not accept is
  listed disabled, with the reason as its tooltip and accessible
  description.
- `bpr:` becomes a third reserved qualifier, beside `repo:` and `personal:`
  (`core/skill_references.hpp`). A person's or repository's skill named
  `bpr` is still reached as `/personal:bpr` or `/repo:bpr`.
- The app expands each `/bpr:<name>` before sending. The reference stays in
  the text as written, and the Feature's contract rides after the text. The
  request is the same for Claude Code and for Codex, and it does not depend
  on either agent loading skills natively.
- A real `/skill-name` in the same text keeps working as [[BPR-1916]] has
  it, up to `maximum_referenced_skills` (four). Features do not count toward
  that limit, and a prompt may name each Feature once.
- The editor marks an unknown `/bpr:<name>`, and saving is refused with the
  name. In a composer, an unknown `/bpr:<name>` is sent as typed text, as an
  unknown `/skill` is today.

> Decision (owner, 2026-11-16): "Our built-ins should yes read like inline
> references with a picker like the skills picker / command currently has";
> "choose something that will clearly work across claude and codex". Users
> must still be able to invoke real skills alongside built-ins.

### The catalog (built-in only)

The application defines every Result and Feature, and people cannot author
their own. People write prompts and choose from the catalog.

> Decision (owner, 2026-11-16): built-in only, no user-authored Results or
> Features.

| Result | Opens as / unlocks | Placeable by a person's prompt |
|---|---|---|
| **Answer** | A reply in the chat where it was asked. A long answer becomes a report card and a report tab, decided by its shape (RFC-BPR-0128 Q7). | Any chat place and Ask scope |
| **Findings report** | A report card and a report tab. The verdict and summary carry Keep summary, and each finding carries its keep action (Draft comment at its lines, otherwise Keep local; RFC-BPR-0127). This is BPR-2219's format contract. | Agent tab and Ask, at the pull request or a file |
| **Tour outline** | The tour's outline: stops with anchors, reasons and questions | Its flow only |
| **Tour stop comment** | One stop's text | Its flow only |
| **Rewrite text** | Replaces the text being written: a comment, reply, note, description field or stop (Enhance) | Its flows only |
| **Description** | A local draft's title and description | Its flow only |
| **Commit message** | A local draft's commit message | Its flow only |
| **Check plan & report** | A local draft's planned check and its report (RFC-BPR-0120) | Its flow only |

| Feature | Its block unlocks | Accepted by |
|---|---|---|
| **Suggested comment** | A Draft comment button carrying the agent's wording | Answer, Findings report |
| **Suggested change** | A GitHub suggestion block at its lines, kept as a draft comment's suggestion | Answer, Findings report, Rewrite text |
| **Proposed fix** | A patch, shown as a diff, with Apply | Answer, Findings report |

If the output leaves out a Feature's block, or the block does not parse,
the Result still presents. Only that block's action is missing, and the
item says the agent gave none. Nothing the agent wrote is lost.

**Mapping today's kinds.** `prompt_ask::question` becomes **Answer**, and
`prompt_ask::review` becomes **Findings report**. Each stored macro keeps
its text and gains the matching Result. The shipped review prompts gain
`/bpr:suggested-comment` and `/bpr:suggested-change`, because their answers
carry both blocks today.

### Every app flow is a shipped prompt (M2)

- Every flow's instructions become a shipped prompt in Settings › Prompts,
  with its own editable text, Result and Features. This covers each writing
  hook in `core::writing_hook`, the report guidance in `report-templates.md`,
  and the suggested-comment format. A format rule moves into a contract.
  Guidance (what to look for, tone) moves into the prompt's text, or into
  the text of each shipped prompt that references the Feature. Pressing a
  flow sends its prompt, so an edit changes what the next press sends.
- The proposed mapping keeps each flow's output opening where it opens
  today:

  | Flow | Result |
  |---|---|
  | Problem statement, solution summary, Enhance stop, Enhance writing | Rewrite text |
  | Tour outline | Tour outline |
  | Tour stop comment | Tour stop comment |
  | Review feedback (the comment workshop's label guidance) | Becomes part of the Enhance writing prompt's text |
  | Local description | Description |
  | Local discussion | Answer |
  | Local commit message | Commit message |
  | Local run check | Check plan & report |
  | What changed since my review, What could this change break? | Findings report |
  | Was this addressed?, Explain this file | Answer |
  | Suggest the change | Rewrite text, with `/bpr:suggested-change` |

- A flow prompt keeps its stable purpose ID (`writing_hook_purpose`), so
  RFC-BPR-0107's exchange still names it.
- **Settings › Writing hooks is removed.** That means the writing-hooks
  panel, its "Built-in prompts" list, and `writing_hook_settings.Overrides`.
- **Existing overrides are dropped.** On the first start after the upgrade,
  a profile that held any override discards them all and shows a one-time
  notice naming each prompt whose edit was dropped. A profile without
  overrides sees no notice. An exchange file that carries writing-hook
  overrides imports without them, and the import's summary names what was
  dropped.

> Decision (owner, 2026-11-16): every app flow's agent instructions become
> shipped prompts in Settings › Prompts; Writing hooks is removed; existing
> overrides are dropped with a one-time notice (owner chose "Drop them").

### A newer built-in version (M3)

- An untouched shipped prompt takes each release's text silently.
- An edited shipped prompt keeps the person's text. When a newer built-in
  text ships, the prompt shows **A newer built-in version exists**, with
  **Compare** (the person's text beside the new built-in) and **Use
  built-in**. The app makes neither choice for the person.
  `prompt_builtin_change.WrittenAgainst` (RFC-BPR-0125) records which
  revision the text was written against.
- A contract is not part of the text, so a contract change applies to every
  prompt at once, edited or not, and raises no flag.

> Decision (owner, 2026-11-16): edited shipped prompts keep the user's text;
> a newer built-in shows the flag with Compare and Use built-in; untouched
> prompts update silently.

### The editor

The Prompts editor gains the Result field, the read-only contract and
material sections, and `/bpr:` completion. The rest of the workflow is the
frontend designer's call: how contracts are laid out, how Compare reads,
where the notice sits. The work loads `frontend-design:frontend-design`,
and the owner judges the result in a look at a frozen Fixture-agent build.

> Decision (owner, 2026-11-16): "Have the frontend designer come up with
> whatever it thinks is the best workflow."

### Boundary with BPR-2219

[[BPR-2219]] proceeds now, unchanged. Its read-only report-format contract
becomes the **Findings report** Result's contract. The review guidance it
moves into the prompt body stays that prompt's text.

> Decision (owner, 2026-11-16): BPR-2219 proceeds now unchanged.

### Boundaries

- A Result or Feature changes what a run is told about the shape of its
  output, and what the app offers on that output. It does not change what
  an agent may read or do: RFC-BPR-0101's chat rules, RFC-BPR-0102's
  subjects and the application contract's limits all hold.
- No Result or Feature acts by itself. Draft comment, Keep local, Apply and
  Use built-in each wait for a press, and nothing is published until the
  reviewer publishes (RFC-BPR-0127).
- RFC-BPR-0125's `/p ` prompt references, conditions, places and sharing
  stay as they are, except as amended below.

### Architectural allocation

- **Core** decides:
  - the Result and Feature catalog, with each contract's identity and
    revision;
  - which Results a place or surface allows, and which Features a Result
    accepts;
  - `/bpr:` reference scanning, beside `skill_references`;
  - what a request is told (text, then contracts), assembled as one value
    that the editor also renders;
  - parsing each contract's blocks and deciding the actions they unlock;
  - migration of `prompt_ask` and of writing-hook overrides;
  - whether a newer built-in exists.
- **Host** performs:
  - reading the shipped contracts and prompts from resources;
  - sending the assembled instructions the same way for each agent kind;
  - Draft comment, Keep local and Apply;
  - dropping the overrides in the store and recording that the notice was
    shown.
- **Qt** presents:
  - the editor's Result field and read-only sections;
  - the `/bpr:` rows in the completion popup;
  - Compare and Use built-in;
  - the one-time notice;
  - the buttons a Result or Feature unlocks.

## Proposed amendments to adopted RFCs

The milestone that builds each amendment applies it and records it on that
RFC as an owner amendment. Each amendment keeps every obligation it does
not name.

| RFC | What changes |
|---|---|
| RFC-BPR-0092 | The review skill `bpr-pr-review` stops being separate instructions; its guidance becomes the review prompts' text. Q1 covers an edited copy. |
| RFC-BPR-0107 | Writing-hook overrides in an exchange file are dropped on import and named in its summary. Flow prompts are exchanged as prompts under their purpose IDs. |
| RFC-BPR-0125 | *Ask as* becomes the **Result** field. Under Q6, `/` now lists skills and Features, not skills only. Built-in revisions gain the newer-built-in flag. |
| RFC-BPR-0128 | The report-templates guidance becomes prompt text, and the report shape becomes the Findings report contract. |

## Open questions

These go to the owner. Each affected part waits for its answer, as the
last column says. A default is a recommendation, not an answer.

| # | Question | Default | Affects |
|---|---|---|---|
| Q1 | An edited copy of the review skill `bpr-pr-review`: keep it as the edited text of Review this pull request (flagged under M3), or drop it with the Writing hooks overrides? | Keep it as the edited text | M2's review-skill part |
| Q2 | Is the per-flow Result mapping above right? | As listed: each flow's output opens where it opens today | M2, per flow |
| Q3 | Is a flow prompt's Result fixed and shown read-only, since its flow reads that output? | Fixed | M1 editor, M2 |
| Q4 | Proposed fix's Apply needs a folder the app may write to. Until RFC-BPR-0115's local-draft worktree exists, should Apply show disabled with its reason, leaving the patch to be copied? | Yes | M1's Proposed fix action |
| Q5 | May a person's own prompt use a flow-only Result (for example a second Rewrite text, "Make it formal", offered beside Enhance)? | Not in this RFC | The catalog's "Placeable" column |
| Q6 | Is a typed question's `/` picker offered Features too (a typed question is an Answer), not only the prompt editor? | Yes | M1 picker |

## Grill confirmation (2026-11-16)

The owner confirmed the design in a grill-rfc interview on 2026-11-16.

- **Scope and version:** this RFC's first draft, written from that
  interview.
- **Covered:**
  - terminology (decision 1);
  - the contract model, and that nothing is hidden (2);
  - one required Result, and inline `/bpr:` Features that work across
    Claude and Codex beside real skills (3);
  - the catalog and the `prompt_ask` mapping (4);
  - built-in only (5);
  - flows moving into Prompts, with Writing hooks removed and overrides
    dropped (6);
  - the newer-built-in flag (7);
  - the editor left to the designer (8);
  - the BPR-2219 boundary (9);
  - the three milestones.
- **Not covered:** these were drafted after the interview and are open or
  proposed above:
  - the per-flow mapping beyond `prompt_ask`;
  - the read-only material section;
  - which Features each Result accepts;
  - the unknown-`/bpr:` rule;
  - the exchange amendment;
  - Q1 to Q6.

## Milestones

- **M1: Results and Features in prompts.**
  - The catalog and its contracts.
  - The Answer and Findings report Results, with the `prompt_ask`
    migration.
  - The three Features, inline as `/bpr:`, with their actions.
  - Expansion for Claude and Codex.
  - The editor and the picker, after an owner look.
  - Cards: [[BPR-2222]] (model, expansion and actions; after
    [[BPR-2219]]) and [[BPR-2223]] (editor and picker).
- **M2: App flows are shipped prompts.** Every flow appears in Settings ›
  Prompts, Writing hooks is removed, and overrides are dropped with the
  notice. Card [[BPR-2224]].
- **M3: A newer built-in version.** The flag, Compare and Use built-in.
  Card [[BPR-2225]].
- **Closeout:** [[BPR-2226]].

## Acceptance criteria

- **AC01:** No product text, accessible name or setting calls a Result or Feature a skill; the editor, picker and Settings say Result and Feature, and `/skill-name` references are still called skills.
- **AC02:** Every prompt has exactly one Result: saving without one is refused, a place or surface its Result cannot open in is not offered, and changing the Result to one its placements do not allow names those placements and does not save.
- **AC03:** With the Fixture agent, Review this pull request whose text is replaced by one sentence with no format guidance still opens a report tab with a verdict, a summary with Keep summary, and a Draft comment on each finding with a place; likewise an Answer prompt with `/bpr:suggested-comment` and no format guidance still shows Draft comment.
- **AC04:** For every shipped prompt and every flow, each instruction byte of the captured request (the system prompt and the message's instructions, excluding the listed material) appears in that prompt's editor as editable text or a read-only section, including the application contract, every contract and every inlined skill.
- **AC05:** Typing `/` in the prompt editor lists Features beside the personal and repository skills, each marked by source; `/bpr:` lists Features only; a Feature the Result does not accept is listed disabled with its reason; choosing a row inserts its reference.
- **AC06:** A prompt naming `/bpr:suggested-change` and a repository `/repo:<name>` skill produces requests for a Claude agent and a Codex agent that each carry the Feature's contract and the skill's text in the same form, with no native skill loading; a fifth `/skill` is noted as today, and Features do not count toward the four.
- **AC07:** Suggested comment yields Draft comment carrying the agent's wording; Suggested change yields a suggestion block kept at its lines; Proposed fix yields a diff with Apply, or Apply disabled with its reason where nothing can be written; an omitted or malformed block leaves the rest of the Result presented, with a note that the agent gave none.
- **AC08:** Stored macros of `prompt_ask::question` and `prompt_ask::review` load as Answer and Findings report with their text unchanged, and the shipped review prompts carry `/bpr:suggested-comment` and `/bpr:suggested-change`.
- **AC09:** The editor's Result field, read-only sections, `/bpr:` completion, Compare and the dropped-overrides notice are reachable and operable by keyboard and named to assistive technology, and the owner has accepted the editor in a look at a frozen Fixture-agent build.
- **AC10:** Every flow (each `core::writing_hook`, the report guidance and the suggested-comment format) is a shipped prompt in Settings › Prompts with its Result; editing its text changes what the next press of that flow sends; the writing-hooks panel and its Built-in prompts list are gone.
- **AC11:** A profile holding writing-hook overrides, started once on the new build, has none left and is shown the notice naming each dropped prompt once, and not on the next start; a profile without overrides sees no notice; an exchange file with overrides imports without them, and its summary names them.
- **AC12:** Across a built-in revision, an untouched shipped prompt sends the new text with no flag; an edited one keeps and sends its own text and shows A newer built-in version exists, whose Compare shows both texts and whose Use built-in replaces the text only when pressed; a contract change reaches both with no flag.
- **AC13:** Core decides which Results a place allows, which Features a Result accepts, the instructions a request carries and the actions an output unlocks; the host performs those actions, Qt presents them, and no widget composes instruction text.
