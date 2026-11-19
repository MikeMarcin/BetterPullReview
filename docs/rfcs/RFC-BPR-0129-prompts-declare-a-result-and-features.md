---
id: "RFC-BPR-0129"
title: "Prompts declare a Result and Features; nothing the agent is told is hidden"
status: "Proposed"
metadata: {"origin":"owner grill-rfc interview, 2026-11-16, after BPR-2219 (the review report lost its format and Draft comment)","inspected_revision":"71dbb4fb3","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d at 671a497ca","mandate":"owner, 2026-11-16: the design confirmed in a grill-rfc interview; Q1, Q3, Q5 and Q6 answered by the owner, Q2 and Q4 defaults accepted by the coordinator; revised the same day for a critical review (B1 to B5, N1 to N10), its product choices O1 to O5 accepted by the owner as proposed; owner decided a self-describing export and import check the same day, then a version bump with no compatibility with older builds; cards BPR-2222 to BPR-2227, BPR-2229 and BPR-2230 in Backlog, none started"}
depends_on: ["RFC-BPR-0092", "RFC-BPR-0101", "RFC-BPR-0107", "RFC-BPR-0125", "RFC-BPR-0126", "RFC-BPR-0128"]
---

# Prompts declare a Result and Features; nothing the agent is told is hidden

## Problem and grounded current behavior

What a BetterPullReview agent run is told is assembled from many sources,
and the person can see and edit only some of them. The list below is the
full inventory at `71dbb4fb3`.

**Editable today**

- **Suggested prompts** (RFC-BPR-0125, `core::prompt_macro`). Each has an
  editable template and an *Ask as* of `prompt_ask::question` or
  `prompt_ask::review`. The shipped set is in
  `src/core/src/prompt_macro_shipped.cpp`. The kind decides what the answer
  becomes, but the rules that make the answer parse are elsewhere.
- **Writing-hook overrides** (`application/writing_hooks.hpp`,
  `core::writing_hook_settings`). These are sixteen of the files under
  `src/application/resources/review-preparation/`, each keyed by a purpose
  ID (`writing_hooks.cpp`: `tour.problem` … `local-draft.run-check`). They
  are edited in the writing-hooks panel, which [[BPR-2179]] placed in the
  Settings › Prompts library as its system prompts (`writing_hooks_panel.cpp`,
  headed "Built-in prompts"). No Settings page is named Writing hooks. An
  override replaces the whole file, guidance and output format together.
- **The review skill** `bpr-pr-review`
  (`packaging/agent-skills/bpr-pr-review/SKILL.md`, RFC-BPR-0092 D1,
  `application/review_skill_host.hpp`). Every Agent tab request carries it,
  except a returning question's first answer and a comment-workshop press
  (`window_agent_chat.cpp`, the brief). It holds the review method and also
  rules the app depends on:
  - the `brief.*` field legend;
  - the conversation rule;
  - "always answer in words";
  - the `bpr-findings` block;
  - the data-not-instructions boundary.

  Its edited copy has its own "Version N is available", and removing it
  writes a copy to Downloads first.

**Compiled in and not editable**

- **Resource files:**
  - `application-contract.md`, sent only with writing requests
    (`agent_launch.cpp`, the writing prompt). Its wording is for rewrites
    and outlines.
  - `report-templates.md` (`report_templates_guidance()`), sent with every
    Agent tab request after the first answer.
  - `suggested-comment.md`, as `brief.comment_format`. It is also overridable
    as `review.suggested-comment`.
- **Text in C++:**
  - `agent_launch.cpp`: the writing prompt's JSON schema, style and outline
    rules; `returning.answer_format`, the claim and `bpr-claims` rule for
    one thread or several; and the Codex-only helpers sentence
    (`codex_foreground_helpers`).
  - `core/agent_grant.cpp`: the grant sentences, `agent_scratch_instruction`
    and `agent_builds_instruction`.
  - `tour_agent_drafting.cpp`: the Draft tour prompt and the
    `bpr.tour-draft.v1` schema, with a tool-less or a read-only preamble
    depending on the agent.
  - `tour_agent_conversation.cpp`: the tour chats' prompts and the
    `bpr.tour-ask.v1` and `bpr.tour-revise.v1` schemas. Each chat requires
    exactly one JSON object.
  - `tour_agent_context.cpp`: `tour_agent_guide`.
  - `tour_writing_style.cpp`: the tour style.
  - `skill_guidance.cpp`: `inline_skill_directive`.
  - `core/local_draft_tour.cpp`: the local discussion's answer format and
    `bpr-proposals`.
  - `core/local_draft_check.cpp`: `bpr-check-plan` and `bpr-check-report`.

**Routes.** The same text reaches the agent differently by agent kind
(`agent_launch.cpp`, [[BPR-2140]]):

| Agent kind | Where the instructions go |
|---|---|
| Claude | `--append-system-prompt` |
| Codex `exec` | `developer_instructions` |
| Codex app-server | The message |
| Any, when the text exceeds `maximum_system_prompt_argument_bytes` (24 KiB) | The message |

Skills a prompt names (`/skill-name`, `/repo:name`, `/personal:name`;
[[BPR-1916]]) are inlined as `skill.N.*` fields for every agent kind.

The cost of this split shows in [[BPR-2219]]. Guidance and output format
often share one editable text, so an edit made for the guidance can
quietly break what the app reads. That is what happened there: a review
answer stopped reading as a report and lost Draft comment. Where the format
is not editable, it is also invisible. This RFC separates guidance from
format for every run the app starts, and shows both.

## Proposed behavior and boundaries

### Terms

- A **Result** is what a prompt produces. It is a fixed output contract
  plus the app action it unlocks: where the answer opens, and what it
  offers.
- A **Feature** is an add-on that a prompt asks for inline. It is a fixed
  output contract for one kind of block, plus the action that block unlocks.
- A **skill** is only an agent skill that the person or the repository has
  (`/skill-name`). The product never calls a Result or a Feature a skill.
- A **family contract** is the read-only rules shared by every run of one
  family: Agent tab, Writing, Tour, or Local draft. It carries the boundary
  and the request legend. It is appended to every run of that family,
  whatever the prompt says.

> Decision (owner, 2026-11-16): "It's confusing to call these skills." The
> main one is a **Result**; the inline add-ons are **Features**; real agent
> skills remain skills and keep working.

### A prompt is its text, one Result, and any Features

- **Guidance is the prompt's text.** What to look for, sections, tone and
  length all belong in the prompt's own editable text. A contract carries
  only what the app parses or must enforce.
- **Contracts are always appended.** Every request carries, after the
  prompt's text:
  1. its family contract;
  2. its Result's contract;
  3. each referenced Feature's contract.

  Deleting every word of format guidance from the text cannot stop the
  output from parsing.

> Decision (owner, 2026-11-16): a Result or Feature is a fixed output
> contract plus the app action it unlocks. All guidance lives in the
> prompt's editable text; contracts are appended and shown read-only.

### Nothing the agent is told is hidden

A captured request's bytes fall into nine classes. Every class is either
shown in the prompt's editor or documented once.

| # | Class | Shown how |
|---|---|---|
| 1 | The prompt's text | Editable |
| 2 | The family contract | Read-only, verbatim |
| 3 | The Result's contract | Read-only, verbatim |
| 4 | Each Feature's contract | Read-only, verbatim |
| 5 | The skills directive (`inline_skill_directive`) and each inlined skill | The directive verbatim; each skill by name, with its text one press away |
| 6 | **Agent addenda**: text added for one agent kind (the Codex helpers sentence; Draft tour's tool-less or read-only preamble) | Read-only, verbatim, under the agent kind it applies to |
| 7 | **Grant addenda**: text added while a grant is on (scratch folder, building and testing) | Read-only, verbatim, with the condition that adds it; paths shown as placeholders |
| 8 | The run's **material**: the brief's values, the diff, a thread, the typed words, evidence JSON | Listed by field name, not printed; untrusted evidence, not instruction |
| 9 | The **frame**: field names, separators, the JSON envelope, and the route (system prompt, `developer_instructions` or message) | Documented once, in `docs/architecture/ai-agent-boundary.md`, not per prompt |

### Exactly one Result

- Each prompt has one required **Result**. It is a field of its own, not
  part of the text, and a prompt without one cannot be saved.
- The Result decides where the output opens, and it limits where the prompt
  can be placed (RFC-BPR-0125's places and [[BPR-2179]]'s surfaces). The
  editor does not offer a placement the Result cannot open in. Changing the
  Result to one its current placements do not allow names those placements,
  and the change does not save until they are removed.
- A shipped flow prompt's Result is fixed, because its flow reads that
  output. The editor shows it read-only.
- A person's own prompt may choose any Result, and is placed only where that
  Result works. For example, a second Rewrite text prompt ("Make it formal")
  appears in the Enhance menu, and its output replaces the text being
  written. It is never offered in a chat. When a flow has more than one
  prompt, its control offers each by name, the shipped one first.

> Decision (owner, 2026-11-16): "I'm okay for the 'main skill' to be
> separate and required." Answering Q3 and Q5 the same day: a shipped flow
> prompt's Result is fixed and shown read-only; the user's own prompts may
> choose any Result, placed only where that Result works.

### Features, inline as `/bpr:<name>`

- **Referencing a Feature.** A Feature is referenced in the text as
  `/bpr:<name>`: `/bpr:suggested-comment`, `/bpr:suggested-change` or
  `/bpr:proposed-fix`.
- **The picker.** The completion picker that offers the person's and the
  repository's `/skills` offers Features too. It does so in the prompt
  editor and in every composer with `/` completion, including a typed chat
  question (owner, Q6). Each row is marked BetterPullReview, personal or
  repository, and `/bpr:` narrows the list to Features.
- **Features the Result cannot carry.** A Feature whose block the Result's
  format cannot carry is listed disabled, with the reason as its tooltip and
  accessible description. In a tour chat, whose Result is a JSON object,
  every Feature is disabled.
- **The `bpr:` qualifier.** `bpr:` becomes a third reserved qualifier,
  beside `repo:` and `personal:` (`core/skill_references.hpp`). A skill
  named `bpr` is still reached as `/personal:bpr` or `/repo:bpr`. Feature
  names and flow prompt IDs are separate namespaces, and neither is ever
  the other.
- **Expansion before sending.** The app expands each `/bpr:<name>` before
  sending. The reference stays in the text as written, and the Feature's
  contract rides after the text, in class 4. Claude and Codex get the same
  instruction bytes in classes 1 to 5. Only the route (class 9) and the
  agent addenda (class 6) differ, and both are shown. Nothing depends on
  either agent loading skills natively.
- **Feature trust.**
  - A Feature is brought in only by the prompt's own text or by the words
    the person types. A substituted value (`{title}`, `{file}`, a branch, a
    login) or any material never brings one in. This extends the rule that
    a pull request's title cannot bring in a skill (RFC-BPR-0125).
  - An answer's Feature block is honoured only when the request declared
    that Feature. An undeclared block is shown as plain code and unlocks
    nothing.
- **Limits.** Real `/skill-name` references keep working as [[BPR-1916]]
  has them, up to four; Features do not count toward that limit. Naming a
  Feature a second time adds nothing: its contract is appended once. The
  editor marks the repeat.
- **Unknown names.** In the editor, an unknown `/bpr:<name>` is marked and
  saving is refused, with the name. In a composer, it is sent as typed text.

> Decision (owner, 2026-11-16): "Our built-ins should yes read like inline
> references with a picker like the skills picker / command currently has";
> "choose something that will clearly work across claude and codex". Users
> must still be able to invoke real skills alongside built-ins.

### The catalog (built-in only)

The application defines every Result and Feature, and people cannot author
their own.

> Decision (owner, 2026-11-16): built-in only, no user-authored Results or
> Features.

The owner named eight Results. The owner added six more, marked †, by
accepting O1. They cover the shapes the app already parses that
none of the eight can carry:

- a report without findings;
- thread claims;
- tour chat replies;
- the tour draft;
- the local discussion's proposals;
- Suggest the change's line replacement.

| Result | Family | Its contract (read-only) | Opens as / unlocks | Features it carries |
|---|---|---|---|---|
| **Answer** | Agent tab | None beyond the family contract | A reply where it was asked; a long answer becomes a report card and tab by its shape (RFC-BPR-0128 Q7) | All three |
| **Findings report** | Agent tab | Report shape, the optional verdict line and `bpr-findings` (BPR-2219's contract) | Report card and tab; Keep summary; each finding's keep action | All three |
| **Report** † | Agent tab | Report shape: title, summary, sections, items with places | Report card and tab | All three |
| **Thread claims** † | Agent tab | Claim words and `bpr-claims`, for one thread or several | A claim chip on each thread (RFC-BPR-0128) | Suggested comment |
| **Line suggestion** † | Agent tab | `{first_line,last_line,current,replacement}` | A suggestion block at the comment's lines (RFC-BPR-0118) | None (JSON) |
| **Tour reply** † | Tour | `bpr.tour-ask.v1` at a stop or part, `bpr.tour-revise.v1` for the whole tour | The tour chat's reply and proposals | None (JSON) |
| **Tour draft** † | Tour | `bpr.tour-draft.v1` | Draft tour's problem, solution and stops | None (JSON) |
| **Tour outline** | Writing | The writing result's stops | The tour's outline | None (JSON) |
| **Tour stop comment** | Writing | The writing result's replacement | One stop's text | None (JSON) |
| **Rewrite text** | Writing | The writing result's replacement | Replaces the text being written | None (JSON) |
| **Description** | Writing | The writing result's replacement | A local draft's title and description | None (JSON) |
| **Commit message** | Writing | The writing result's replacement | A local draft's commit message | None (JSON) |
| **Draft proposals** † | Local draft | Prose, then `bpr-proposals` | The local discussion's reply and proposals to accept | None |
| **Check plan & report** | Local draft | `bpr-check-plan` and `bpr-check-report` | A local draft's planned check and its report (RFC-BPR-0120) | None |

| Feature | Its block unlocks |
|---|---|
| **Suggested comment** | Draft comment on that block. The draft's body is the block's text, not the whole answer, and it sits at the block's place. This is beside the answer's own keep action (RFC-BPR-0127), not in place of it. |
| **Suggested change** | A GitHub suggestion block, kept as a draft comment's suggestion at its lines. |
| **Proposed fix** | A patch shown as a diff that can be copied. Apply waits for a folder the app may write to: until RFC-BPR-0115's local-draft worktree exists, Apply is shown disabled with that reason (Q4). |

If the output leaves out a declared block, or the block does not parse, the
Result still presents. Only that block's action is missing, and the item
says that the agent gave none.

**Family contracts.** Each is read-only:

- **Agent tab:** split from the review skill (owner, O2):
  - the data-not-instructions boundary;
  - the `brief.*` legend;
  - the conversation rule;
  - "always answer in words";
  - the no-publishing rule.
- **Writing:** `application-contract.md`, reworded per Result so that a
  rewrite and an outline are each told only their own rule.
- **Tour:** `tour_agent_guide`.
- **Local draft:** the local draft's rules from `local_draft_tour.cpp` and
  `local_draft_check.cpp`.

### Every prompt, by ID

This one table replaces the per-flow mapping (Q2, as revised by the
review). Flow prompts get new IDs (`flow.*`) because their texts no longer
mean what the old purpose IDs meant (see Sharing). In the table, SC is
Suggested comment, SCh is Suggested change, and "Shipped" means a shipped
flow prompt.

| ID (old ID) | Shown as | Result | Features | What happens to stored data |
|---|---|---|---|---|
| `agent-tab.review-pull-request`, `agent-tab.review-open-file` | Review this pull request; Review {file} | Findings report | SC, SCh | Shipped Result replaces a stored `review` kind |
| `agent-tab.what-could-break` | What could this change break? | Report | SCh | Shipped Result replaces a stored `question` kind; its template from `report-templates.md` moves into its text |
| `agent-tab.explain-line`, `agent-tab.explain-block`, `conversation.summarize`, `conversation.concern-right`, `conversation.explain-note-code` | As today | Answer | None | Kind `question` maps to Answer |
| `conversation.draft-reply` | Draft a reply… | Answer | SC | As above |
| `agent-tab.what-changed` | What changed since my review | Report (runs `flow.review.latest-upload`) | None | Action row; nothing stored to map |
| `agent-tab.explain-file` | Explain this file | Report (runs `flow.review.explain-file`) | None | As above |
| `conversation.was-addressed` | Was this addressed? | Thread claims (runs `flow.review.was-addressed`) | None | As above |
| `tour-stop.*`, `tour-part.*`, `tour.*` | As today | Tour reply | None | Kind `question` at a tour place maps to Tour reply, not Answer |
| `flow.agent-tab.question` (part of `review.skill`, `review.suggested-comment`) | Typed question | Answer, fixed | SC | Shipped; text is the review skill's method and the comment style (owner, O2) |
| `flow.tour.problem`, `flow.tour.solution` (`tour.problem`, `tour.solution`) | Problem; Solution | Rewrite text, fixed | None | Shipped; override dropped |
| `flow.tour.outline` (`tour.outline`) | Tour outline | Tour outline, fixed | None | As above |
| `flow.tour.stop-comment` (`tour.stop-comment`) | Stop comment | Tour stop comment, fixed | None | As above |
| `flow.tour.draft` (none: `tour_agent_drafting.cpp`) | Draft tour | Tour draft, fixed | None | Shipped; tour style moves into its text |
| `flow.tour.chat-stop`, `flow.tour.chat-part`, `flow.tour.chat-whole` (none: `tour_agent_conversation.cpp`) | Tour chat | Tour reply, fixed | None | Shipped; tour style moves into their text |
| `flow.enhance.tour-stop`, `flow.enhance.writing` (`enhance.tour-stop`, `enhance.writing`) | Enhance | Rewrite text, fixed | None | Shipped; override dropped |
| `flow.workshop`, `flow.workshop.labels` (`review.feedback`) | Workshop a comment; …with labels | Answer, fixed | SC | Shipped. The label helper chooses which one runs, so label guidance is still sent only for workshop presses while the helper is on (owner, O3). Override dropped |
| `review.suggested-comment` | Retired | None | None | Its format becomes the Suggested comment contract; its style moves into `flow.agent-tab.question` and `flow.workshop*`. Override dropped |
| `flow.local.description` (`local.description`) | Description | Description, fixed | None | Override dropped |
| `flow.local.discussion` (`local.discussion`) | Discuss the change | Draft proposals, fixed | None | As above |
| `flow.local.commit-message` (`local.commit-message`) | Commit message | Commit message, fixed | None | As above |
| `flow.local-draft.run-check` (`local-draft.run-check`) | Check the change | Check plan & report, fixed | None | As above |
| `flow.review.latest-upload` (`review.latest-upload`) | What changed since my review | Report, fixed | None | As above |
| `flow.review.was-addressed` (`review.proposed-fix`) | Was this addressed? | Thread claims, fixed | None | As above; the old ID's "proposed fix" is not the Feature |
| `flow.review.explain-file` (`review.explain-file`) | Explain this file | Report, fixed | None | As above |
| `flow.review.suggest-change` (`review.suggest-change`) | Suggest the change | Line suggestion, fixed | None | As above |
| `review.skill` | Retired | None | None | Split: method into `flow.agent-tab.question` and the review prompts' text (BPR-2219), rules into the Agent tab contract. An edited copy is dropped (Q1) |

**The `report-templates.md` file is split.** Its shared rules become the
Report and Findings report contracts. Its "what could break" template
becomes `agent-tab.what-could-break`'s text, and its review template becomes
the review prompts' text.

**Migrating stored suggested prompts** (RFC-BPR-0125's stored differences,
[[BPR-2179]]'s repository layers, and queued prompts):

- A person's own macro maps by kind and place:
  - `review` becomes Findings report.
  - `question` becomes Answer, or Tour reply at a tour place.
- A changed shipped macro takes its Result from the table. If the person
  changed the stored kind from the shipped one, it maps by kind instead.
- A repository layer's `prompt_override.Kind` maps the same way.
- A placement the new Result cannot open in is kept and coerced, as
  `prompt_surfaces.cpp` already coerces `review` to `question` off the Agent
  tab. The editor names the coercion. Nothing is deleted.
- **Queued prompts** (RFC-BPR-0126, `core/agent_queue.cpp`):
  - The queued encoding gains a version. It stores the Result by stable name
    and keeps `/bpr:` references unexpanded.
  - An older entry decodes by kind, as above.
  - Contracts are taken at dispatch, from the running build.
  - No older build is required to read a new entry.

### Every app flow is a shipped prompt (M2)

- Every row marked "fixed" in the table above is a shipped prompt in
  Settings › Prompts, with its text, its fixed Result and its Features.
  Pressing the flow sends that prompt, so editing it changes what the next
  press sends.
- Flows are documented once, here. Two kinds of text are not instructions
  to a run the app starts, so they are out of scope:
  - text that the person's own harness reads through a CLI: `bpr tour`
    (`src/app/src/tour_assist.cpp`) and `bpr cycle`'s round Markdown;
  - the MCP helper's tool descriptions.
- **Removed:**
  - the writing-hooks panel, with its "Built-in prompts" list;
  - `writing_hook_settings.Overrides`;
  - the review skill's panel.
- **Dropped edits.** On the first start after the upgrade, any
  writing-hook override and any edited review skill copy is dropped:
  1. Before anything is deleted, the app writes each dropped text to a file
     in Downloads, as RFC-BPR-0092 does today before removing an edited
     skill (owner, O4), and stores the dropped names.
  2. Only then are the edits deleted.
  3. The one-time notice names each dropped prompt and its file.

  If saving the files or the names fails, nothing is deleted, and the next
  start tries again. A profile with no edits sees nothing.

> Decision (owner, 2026-11-16): every app flow's agent instructions become
> shipped prompts in Settings › Prompts; the writing-hooks editor is
> removed; existing overrides are dropped with a one-time notice (owner
> chose "Drop them"). Answering Q1 the same day: an edited `bpr-pr-review`
> copy is dropped too, with the same notice.

### Sharing (RFC-BPR-0107 exchange)

The `bpr.profile-settings` file moves to version 2 for every export. There
is no compatibility with other versions in either direction:

- An older build refuses a version 2 file, as it refuses any newer version
  today.
- The new build reads only version 2. It refuses a version 1 file as an
  older format, naming the version it found.

> Decision (owner, 2026-11-16, overriding O5's compatibility, relayed by the
> coordinator): "Naw just tick the version and dont support older builds.
> We havent even released prompt export properly yet." The `flow.*` IDs stay,
> for clarity. This follows the owner's standing rule of no backward
> compatibility for unreleased features.

- **Export.** Flow prompts are exported as Prompts items under their
  `flow.*` IDs, each with its text.
- **A person's own prompts.** These go in the same version 2 file, carried
  by RFC-BPR-0125's [[BPR-2149]] with each prompt's Result, Features and
  placements. There is no separate version for files with or without them.
  Until this RFC's prompt export is released, version 2 may gain fields as
  its cards land.

#### A self-describing export, and an import check

The owner asked: "when I export the prompts and ask another agent to modify
them so I can say integrate some local workflows and then import them back.
How will that agent understand results and features?"

- **The guide travels in the file.** Every export carries one guide that
  the app writes. Core generates it from the catalog, so it cannot drift
  from what the build accepts. It lists:
  - every Result and Feature, each with a one-line description;
  - where each Result may be placed;
  - which Features each Result carries;
  - the rules: guidance goes in a prompt's text; contracts are appended by
    the app and cannot be edited in the file; `/skill-name`, `/repo:name`
    and `/personal:name` are real skills, while `/bpr:<name>` is a Feature;
    a flow prompt's Result is fixed; the guide itself is not imported;
  - one worked example: a person's prompt with the Answer Result that uses
    `/bpr:suggested-comment`.
- **Where the guide sits.** The guide is a top-level `guide` section of
  the version 2 file, written by export. Import reads past it and acts on
  nothing in it: editing or removing the guide changes nothing an import
  does.
- **Import checks before it writes.** Before anything is written, import
  lists every prompt in the file as Add, Change or Refused. A refused
  prompt names its reason:
  - an unknown Result;
  - a Feature its Result does not carry;
  - a placement its Result cannot open in;
  - an unknown `/bpr:` name;
  - a flow prompt's Result changed;
  - and the reasons RFC-BPR-0107 and RFC-BPR-0125 already give.

  Nothing is written until the person confirms. A confirmed import writes
  exactly the listed Adds and Changes, and Cancel writes nothing. Core makes
  every decision, and equal file and profile give equal decisions.
- **A check an editing agent can run.** `bpr prompts check FILE` prints the
  same per-prompt decisions and reasons as the import check, and changes
  nothing. Given `--profile NAME`, it also says Add or Change against that
  profile; without one it checks against the catalog alone. It exits
  non-zero when any prompt would be refused.

> Decision (owner, 2026-11-16, relayed by the coordinator): the export is
> self-describing, with an app-written guide ignored on import; import shows what will be added or changed, and why
> any prompt would be refused, and writes nothing until the person
> confirms, with the decisions deterministic in core; and an optional
> `bpr prompts check FILE` goes on its own card, after the import check.

### A newer built-in version (M3)

RFC-BPR-0125 already flags a changed suggested prompt written against an
older shipped revision (`prompt_set_entry::Outdated`, with Compare).

- That flag extends to the shipped flow prompts and reads **A newer
  built-in version exists**, with **Compare** and **Use built-in**.
- An untouched prompt takes each release's text silently.
- An edited prompt keeps the person's text until they choose otherwise.
- A contract is not text, so a contract change reaches every prompt at once
  and raises no flag.

> Decision (owner, 2026-11-16): edited shipped prompts keep the user's text;
> a newer built-in shows the flag with Compare and Use built-in; untouched
> prompts update silently.

### Sizes

- **Text bounds.** A prompt's text keeps the bound its source had:
  - 8 KiB for a macro template or a former writing hook;
  - 64 KiB for `flow.agent-tab.question`, the bound the review skill had.
- **Contracts and addenda** do not count toward the text bound. They count
  toward the request.
- **Over the system-prompt bound.** When the assembled instructions exceed
  `maximum_system_prompt_argument_bytes`, they go in the message, as today.
- **Over the input limit.** A request over the agent input limit is refused
  before it starts, with its reason.
- **The editor** shows the assembled size against both bounds.

### The editor and the surfaces

The owner's look covers these surfaces:

- the Prompts editor's Result field;
- the read-only class 2 to 7 sections;
- `/bpr:` completion;
- Compare;
- the dropped-edits notice;
- a flow control that offers several prompts;
- the blocks the Features unlock.

How each surface reads is the frontend designer's call. Every card that
draws one loads `frontend-design:frontend-design`, and the owner judges the
result in a look at a frozen Fixture-agent build.

> Decision (owner, 2026-11-16): "Have the frontend designer come up with
> whatever it thinks is the best workflow."

### Boundary with BPR-2219

[[BPR-2219]] proceeds now, unchanged. Its read-only report-format contract
becomes the Findings report Result's contract. The review guidance it moves
into the prompt body stays that prompt's text. Its card records that
visible-prompt criterion as its AC-f.

> Decision (owner, 2026-11-16): BPR-2219 proceeds now unchanged.

### Boundaries

- Results, Features and family contracts change what a run is told about
  its output and what the app offers on that output. They do not change
  what an agent may read or do: RFC-BPR-0101's chat rules, RFC-BPR-0102's
  subjects and the grants hold.
- Nothing acts by itself. Draft comment, Keep local, Apply and Use built-in
  each wait for a press, and nothing is published until the reviewer
  publishes (RFC-BPR-0127).

### Architectural allocation

- **Core** decides:
  - the catalog: each contract's identity, revision and family;
  - which Results a placement allows, and which Features a Result carries;
  - `/bpr:` scanning, beside `skill_references`, with the trust rule;
  - assembling a request's instruction classes 1 to 7 into one value that
    the editor also renders;
  - parsing each declared block and deciding its action;
  - every migration above, and the queued encoding;
  - whether a newer built-in exists.
- **Host** performs:
  - reading the shipped prompts and contracts from resources;
  - routing the instructions for each agent kind;
  - Draft comment, Keep local and Apply;
  - saving dropped edits, then deleting them;
  - the exchange items.
- **Qt** presents:
  - the editor and its read-only sections;
  - the picker rows;
  - the flow controls;
  - Compare and Use built-in;
  - the notice;
  - the Feature surfaces.

## Proposed amendments to adopted RFCs

The milestone named in each row records its amendment on that RFC as an
owner amendment, and the amendment keeps every obligation it does not name.

| RFC | What changes | Recorded by |
|---|---|---|
| RFC-BPR-0092 | `bpr-pr-review` splits into `flow.agent-tab.question`'s text and the Agent tab contract; an edited copy is dropped with its file saved (Q1). | M2 ([[BPR-2224]]) |
| RFC-BPR-0107 | The file moves to version 2 with no compatibility in either direction; flow prompts are carried under `flow.*` IDs. | M2 ([[BPR-2224]]) |
| RFC-BPR-0107 | Every export carries the top-level `guide` section; import lists Add, Change or Refused with reasons before writing, and writes only on confirmation. | M2 ([[BPR-2229]]) |
| RFC-BPR-0118 | Suggest the change's prompt is `flow.review.suggest-change` with the Line suggestion Result. | M2 ([[BPR-2224]]) |
| RFC-BPR-0125 | *Ask as* becomes the Result field, and stored kinds migrate as above. Under Q6, `/` lists skills and Features. Queued prompts store a Result. [[BPR-2149]] carries Result, Features and placements in this RFC's version 2 file, with no version 1 and version 2 split. | M1 ([[BPR-2222]]). If [[BPR-2150]] closes RFC-BPR-0125 first, it is recorded as a post-closure amendment. |
| RFC-BPR-0125 | The Outdated flag and Compare extend to flow prompts. | M3 ([[BPR-2225]]) |
| RFC-BPR-0126 | Queued entries store a Result and take contracts at dispatch. | M1 ([[BPR-2222]]) |
| RFC-BPR-0128 | `report-templates.md` splits into the Report and Findings report contracts and the prompts' text. Was this addressed? is the Thread claims Result. | M2 ([[BPR-2224]]) |

### Amendment (owner, 2026-11-18): Writing style ships first as an editable Feature

[[BPR-2264]] ships one Feature ahead of this RFC, in 0.5.0, and this RFC
adopts it. The owner: "build the writing style hook at a Feature I think.
That way all the skills can reference it easily ... there should be mostly
immutable features but we can have some editable features using the prompt
library to edit them."

- Features are immutable by default. A registry (`core/prompt_features.hpp`)
  marks a Feature editable; an editable Feature is edited in Settings ›
  Prompts, under Features, like a built-in prompt, and travels in the
  settings exchange under `feature.<name>`.
- `writing-style` is the first Feature and the only editable one. Unlike the
  Features above, it is appended to every request whose answer a person
  reads, after that request's own instructions and format, which win where
  they disagree; a reference to it in a prompt or skill sends it once.
- `bpr:` is already reserved: `/bpr:<name>` is never a skill reference.
- This RFC's other Features (`suggested-comment`, `suggested-change`,
  `proposed-fix`) join the same registry, immutable, when M1 builds them.

### Amendment (owner, 2026-11-18): Comment labels is the second editable Feature

[[BPR-2270]]. The owner: "Maybe the comment tag is another editable feature
defaulted to our conventional comments", then "I don't think we need that
safety net, could be a user just blanks that out because they want all plain
comments". `comment-labels` (`/bpr:comment-labels`, `feature.comment-labels`)
ships the Conventional Comments convention with [[BPR-2267]]'s decoration
rule. The same helper attaches it, before the Writing style, to every request
that may write a comment to post (Agent tab turns, the comment workshop, the
returning questions; not Explain this file or Suggest the change). A blank
override turns it off. The app adds no label of its own: a drafted comment
is the agent's text, its catalog label read into the label control. This
narrows O3: the workshop's label helper still chooses its own guidance.

## Questions

### Resolved (2026-11-16)

| # | Question | Answer |
|---|---|---|
| Q1 | An edited `bpr-pr-review` copy? | **Owner:** drop it, with the one-time notice |
| Q2 | The per-flow Result mapping? | **Default, accepted by the coordinator:** each flow's output opens where it does today. Revised for the critical review into the table "Every prompt, by ID", which still keeps every output where it opens today |
| Q3 | Is a shipped flow prompt's Result fixed? | **Owner:** fixed, shown read-only |
| Q4 | Proposed fix's Apply before RFC-BPR-0115? | **Default, accepted by the coordinator:** disabled with its reason; the patch can be copied |
| Q5 | May a person's prompt use any Result? | **Owner:** yes, placed only where that Result works |
| Q6 | Features in the typed chat `/` picker? | **Owner:** yes |

### Accepted by the owner (2026-11-16)

The critical review's five product choices were proposed by the
coordinator. The owner accepted all five as proposed, relayed by the
coordinator.

| # | Decision |
|---|---|
| O1 | The six † Results (Report, Thread claims, Line suggestion, Tour reply, Tour draft, Draft proposals) join the owner's eight, so every shape the app parses has a contract. |
| O2 | The review skill is split. Its method and comment style become the editable Typed question prompt, and its boundary, legend and conversation rule become the read-only Agent tab contract. |
| O3 | Comment labels are two shipped workshop prompts, and the label helper chooses which runs. Labels are not folded into Enhance. |
| O4 | Each dropped edit is saved to a file in Downloads before it is deleted, and the notice names the file. |
| O5 | Flow prompts are shared under new `flow.*` IDs. *Overridden later the same day:* the file version is bumped, and there is no compatibility with older builds (see Sharing). |

> Decision (owner, 2026-11-16): O1 to O5 accepted as proposed.

## Grill confirmation (2026-11-16)

- **First round:** the owner confirmed the design in a grill-rfc interview
  on 2026-11-16. Scope and version: this RFC's first draft. Covered:
  - terminology (1);
  - the contract model and that nothing is hidden (2);
  - one required Result and inline `/bpr:` Features across Claude and Codex
    beside real skills (3);
  - the owner's eight Results, three Features and the `prompt_ask` mapping
    (4);
  - built-in only (5);
  - flows into Prompts with edits dropped (6);
  - the newer-built-in flag (7);
  - the editor left to the designer (8);
  - the BPR-2219 boundary (9);
  - the three milestones.
- **Second round, the same day, relayed by the coordinator:**
  - the owner answered Q1, Q3, Q5 and Q6;
  - Q2 and Q4 are defaults accepted by the coordinator.
- **Critical-review revision, the same day:**
  - correctness fixes: the full inventory, the byte classes, family
    contracts, the per-ID table, the exchange wire rules, Feature trust,
    the queue, sizes, and saving dropped edits;
  - product choices O1 to O5, proposed by the coordinator.
- **Third round, the same day, relayed by the coordinator:**
  - the owner accepted O1 to O5 as proposed;
  - this confirms the revised draft at this version, including the
    critical-review fixes it carries.
- **Fourth round, the same day, relayed by the coordinator:**
  - the owner decided on a self-describing export, an import check, and an
    optional `bpr prompts check`;
  - the guide was first placed as a Prompts item, to stay readable by older
    builds.
- **Fifth round, the same day, relayed by the coordinator:**
  - the owner overrode O5's compatibility: the file version is bumped, older
    builds are not supported, and the new reader reads only version 2;
  - the guide is a top-level section;
  - the `flow.*` IDs stay.

## Milestones

- **M1: Results and Features in prompts.**
  - [[BPR-2222]] (after [[BPR-2219]]): the model, contracts, family
    contracts, expansion and trust, migration and the queue.
  - [[BPR-2223]]: the editor and the picker.
  - [[BPR-2227]]: the Feature surfaces and flow controls with several
    prompts.
- **M2: Every flow is a shipped prompt.**
  - [[BPR-2224]]: the per-ID table, removal, dropped edits with the notice,
    and the exchange.
  - [[BPR-2229]]: the self-describing export and the import check (after
    [[BPR-2224]] and RFC-BPR-0125's [[BPR-2149]]).
  - [[BPR-2230]]: `bpr prompts check` (after [[BPR-2229]]).
- **M3: A newer built-in version.** [[BPR-2225]].
- **Closeout:** [[BPR-2226]], after RFC-BPR-0125's closeout [[BPR-2150]].

## Acceptance criteria

- **AC01:** No product text, accessible name or setting calls a Result or Feature a skill; the editor, picker and Settings say Result and Feature, and `/skill-name` references are still called skills.
- **AC02:** Every prompt has exactly one Result: saving without one is refused, a placement its Result cannot open in is not offered, changing the Result to one its placements do not allow names those placements and does not save, and a shipped flow prompt's Result is read-only.
- **AC03:** With the Fixture agent, for every Result in the catalog, a prompt with that Result whose text is replaced by one sentence with no format guidance still yields output the app parses into that Result's surface (for Findings report: a verdict, Keep summary, and a keep action on each finding with a place; for Thread claims: a chip on each thread).
- **AC04:** For each family, each agent kind, and each grant on and off, every byte of a captured request falls in exactly one of the nine classes; classes 1 to 7 appear verbatim in that prompt's editor (6 and 7 under their agent kind or condition), class 8 is listed by field name, and class 9 matches the documented frame.
- **AC05:** Typing `/` in the prompt editor and in a typed chat question lists Features beside the personal and repository skills, each marked by source; `/bpr:` lists Features only; a Feature the Result cannot carry (every Feature in a tour chat) is listed disabled with its reason; choosing a row inserts its reference, and a typed question sent with one carries that Feature's contract.
- **AC06:** A prompt naming `/bpr:suggested-change` and a repository `/repo:<name>` skill, run against a Claude agent and a Codex agent, carries the same class 1 to 5 bytes in both requests; only the route and class 6 differ, and both are shown; a fifth `/skill` is noted as today and a Feature does not count toward the four.
- **AC07:** A Suggested comment block's Draft comment creates a draft whose body is that block's text at that block's place, while the answer's own keep action still drafts the whole answer; a Suggested change block becomes a draft comment's suggestion at its lines; a Proposed fix block shows a copyable diff with Apply disabled and naming its reason; an omitted or malformed declared block leaves the rest of the Result presented with a note that the agent gave none.
- **AC08:** Stored and repository-layer macros, and queued prompts, of `prompt_ask::question` and `prompt_ask::review` load with the Result the migration gives (a tour place gives Tour reply) and their text unchanged; a coerced placement is kept and named; each shipped prompt has the Result and Features of its row in the per-ID table.
- **AC09:** The editor's Result field, read-only sections, `/bpr:` completion, Compare, the dropped-edits notice, a flow control with several prompts and each Feature surface are reachable and operable by keyboard and named to assistive technology, and the owner has accepted them in looks at frozen Fixture-agent builds.
- **AC10:** Every row of the per-ID table marked fixed is a shipped prompt in Settings › Prompts; editing its text changes what the next press of that flow sends; the writing-hooks panel, its Built-in prompts list and the review skill's panel are gone.
- **AC11:** A profile holding writing-hook overrides and an edited review skill, started once, ends with each dropped text in a file and none of the edits left, and shows the notice naming each prompt and file once and not on the next start; if the first start is stopped before the files and names are saved, the edits are still there at the next start; a profile with no edits sees no notice.
- **AC12:** Every export is `bpr.profile-settings` version 2 and imports into the new build with every flow prompt's text unchanged; a version 1 file is refused before anything is shown, naming the version it found.
- **AC13:** Across a built-in revision, an untouched shipped flow prompt sends the new text with no flag; an edited one keeps and sends its own text and shows A newer built-in version exists, whose Compare shows both texts and whose Use built-in replaces the text only when pressed; a contract revision reaches both with no flag.
- **AC14:** A person's own prompt with Rewrite text appears in the Enhance menu of a comment being written, after the shipped prompt, and its output replaces that comment's text; it is offered in no chat, and a person's prompt with any flow Result is offered only in that flow.
- **AC15:** A pull request titled `/bpr:proposed-fix`, or material containing a Feature reference, brings in no Feature; an answer containing a Feature block its request did not declare shows the block as plain code with no action.
- **AC16:** A typed Agent tab question, a person's Answer prompt and each returning question each carry the Agent tab contract's boundary, legend and conversation rule, whatever their text says; a writing request carries only its Result's writing rule, not another Result's.
- **AC17:** Core decides the catalog, placements, which Features a Result carries, the request's instruction classes and the actions an output unlocks; the host performs, Qt presents, and no widget composes instruction text.
- **AC18:** Every export carries a top-level `guide` section that names every Result and Feature of the core catalog with its one-line description, where each Result may be placed, which Features each Result carries, every rule listed under "A self-describing export", and the worked example; a core test fails if any catalog entry or rule is missing; the same file with its guide edited or removed imports with identical decisions.
- **AC19:** The guide's worked example, added verbatim to an exported file as a new prompt, imports with no refusal, and the imported prompt sends that Feature's contract.
- **AC20:** Importing a file that holds one prompt for each mistake (an unknown Result, a Feature its Result does not carry, a placement its Result cannot open in, an unknown `/bpr:` name, a flow prompt's Result changed) lists each as Refused with its reason and the valid prompts as Add or Change, writes nothing before confirmation and nothing on Cancel, and on confirmation writes exactly the listed Adds and Changes; core gives equal decisions for equal file and profile.
- **AC21:** `bpr prompts check FILE`, run on the AC20 file, prints the same decisions and reasons as the import check, exits non-zero, and leaves the profile and the file unchanged; on the file with only valid prompts it exits zero.
