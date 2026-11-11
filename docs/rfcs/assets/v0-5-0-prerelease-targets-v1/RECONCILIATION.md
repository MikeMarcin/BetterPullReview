# How the local PR drafts pack was absorbed

The owner first asked, on 2026-11-10, that this pack be reconciled with the
local PR drafts pack so that implementing it would keep every decision made
here, and then chose to make this the one pack: "I think I would be more
comfortable with the 0.5.0 pack being the one pack we import and implement
here, just take the good from the other pack". This record says what was
brought in, what was left out, and the rule that holds at each place the two
met.

## What was brought in

"Local PR drafts and shareable author configuration", version 1, authored
2026-11-07 and last revised 2026-11-10 after its third review. It was read
on 2026-11-10 at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1`,
46 files, combined SHA-256 of its per-file SHA-256 list
`5111cff27b915e8d4cc4946323e7fe871f1602da6ad1106c13f28b3cf07acbd2`. That
directory was not changed and remains the original.

| There | Here |
|---|---|
| RFC-DRAFT-0001 to 0006 | [RFC-DRAFT-0010](RFC-DRAFT-0010-agent-and-prompt-sharing.md), [0011](RFC-DRAFT-0011-local-pr-drafts.md), [0012](RFC-DRAFT-0012-author-evidence.md), [0013](RFC-DRAFT-0013-local-tours-and-description.md), [0014](RFC-DRAFT-0014-draft-publication-and-continuity.md), [0015](RFC-DRAFT-0015-optional-workflow-extensions.md) |
| DRAFT-0001 to 0020 | DRAFT-0028 to 0047 in [cards/](cards/), each card's old number in its metadata |
| Seven milestones | The same seven, with the same names and internal order, in [the plan](milestone-plan.json) |
| Its decision record, with the owner's answers D01 to D14 and P04 to P06 | [local-drafts/DECISIONS.md](local-drafts/DECISIONS.md), numbers mapped; summarized in this pack's [decision record](DECISIONS.md) |
| UX flow, current-behaviour observations and their anchors, owner intent, provenance | [local-drafts/](local-drafts/UX-FLOW.md) |
| Its glossary | Merged into this pack's [glossary](GLOSSARY.md) |

All six RFCs came across whole: each rests on a direction or answer the
owner gave, and none was dropped as weak. Their text is as it was, with
numbers and links mapped, one inserted note in RFC-DRAFT-0015, a section
"Fitted to this pack's other decisions" in each, and seven added criteria
(one each in RFC-DRAFT-0010, 0011, 0012, 0014 and 0015, two in
RFC-DRAFT-0013), each assigned to an implementation card and its RFC's
closeout.

## What was left out

- **The enterprise overview and the compatibility analysis built on it.**
  They describe an employer's internal tooling. They stay outside the
  repository, at the original location with the digest recorded in
  [provenance](local-drafts/PROVENANCE.json). RFC-DRAFT-0015 does not need
  them to be implemented; it specifies a generic contract and a synthetic
  adapter. The owner can add them if they are fit to commit.
- **Its hold on import.** The owner's instruction for that pack was not to
  import it yet. The owner's decision to make this the one pack to import
  and implement replaces that for the absorbed content, which now has this
  pack's status: a draft, implemented only under a later mandate.
- **Its own overview, entrypoint, reviewer guide, manifest, generated
  milestone view, card index and schema copy.** This pack's equivalents
  replace them.
- **Its pack checker, coverage map and check receipt.** They are tied to
  that pack's layout and numbers. The checker's one distinctive feature,
  failing when a recorded source anchor has moved, is not carried; the
  anchors themselves are, in
  [repository-observations.json](local-drafts/repository-observations.json),
  and were last confirmed at the inspected revision by that checker.

## Whose rule holds

- **Shared surfaces follow RFC-DRAFT-0001 to 0009.** Home's sections and
  rows, the Tour tab, a tour stop, every agent chat, publication reporting,
  review submission and the review surface's opening state behave as those
  RFCs say, for a pull request and for a local draft alike, unless a row
  below says otherwise.
- **Local-draft matters follow RFC-DRAFT-0010 to 0015.** What a local
  draft, a candidate, evidence, the description and draft creation are, and
  the owner's answers D01 to D14 and P04 to P06, stand as written.
- **Authority boundaries of both stand.** Neither group widens what the
  other allows an agent, a publisher or an import to do.

## Where they meet

"Inherits" means the absorbed RFC needed no new decision: it reuses the
surface and gets the behaviour already decided. "Resolved" means the two
said different things and the rule shown settles it. "Confirmed" means
neither said, the authoring session proposed the rule, and the owner
confirmed it on 2026-11-10. Each rule is also written into the absorbed RFC it
governs, under "Fitted to this pack's other decisions".

| # | Surface | Decided in RFC-DRAFT-0001 to 0009 | The absorbed RFCs | Rule | Kind |
|---|---|---|---|---|---|
| X01 | Home sections | [RFC-DRAFT-0001](RFC-DRAFT-0001-home-says-whose-move-my-pull-request-is.md): sections unchanged; each authored row carries one next move; no Returned section | RFC-DRAFT-0011 adds Prepare pull request… and an In preparation list and removes the Review Cycles card | Both hold. In preparation sits with the author entry above the existing sections; it adds no section among Needs me, My pull requests, Waiting on others and Following and changes no row in them | Inherits |
| X02 | A local draft that has become a pull request | An authored pull request is one row in My pull requests or Waiting on others, with its next move; a GitHub draft reads Draft unless it was Returned | RFC-DRAFT-0014: "Home can show it as linked to a GitHub draft with the real remote state" | One row. Once a local draft is associated with a pull request, Home shows that pull request's row with its next move and a mark that it has local preparation, which opens the preparation. In preparation lists only local drafts with no pull request | Confirmed |
| X03 | The word "draft" on Home | The next-move word Draft means a GitHub draft pull request | The absorbed glossary: avoid a standalone "draft" where local preparation and GitHub state could be confused | The reason word stays Draft inside My pull requests, whose rows are all pull requests, and its sentence says "A draft pull request". In preparation labels its rows Local draft | Resolved |
| X04 | The Tour tab | [RFC-DRAFT-0002](RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md): head, scrolling route, pinned agent bar | RFC-DRAFT-0013: a local tour in the draft workbench's Tour area | The local tour uses the same three bands. Its head states the local draft's tour (stop count and how many to accept); it carries no Publish action, because creating the draft PR belongs to the workbench header | Inherits |
| X05 | A tour stop | [RFC-DRAFT-0009](RFC-DRAFT-0009-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md): arrival shows the code; a stop at rest offers Edit and its chat; Accept in the files' language with a green or yellow ring; no Ready, next | RFC-DRAFT-0013: local stops bind to exact candidate material; a changed candidate marks each stop exact, changed, ambiguous, missing or unavailable and "cannot … silently mark it reviewed" | Local stops are accepted the same way. On an accepted candidate refresh, a stop whose correspondence is exact stays Accepted; changed, ambiguous, missing or unavailable makes it Changed since accepted with that word as its reason. Arrival reveals the stop's lines in the candidate's Changes view | Inherits |
| X06 | Suggest an outline | RFC-DRAFT-0009: removed from the application; the outline hook and `bpr tour outline` stay | RFC-DRAFT-0010 lists the outline writing hook among six exportable prompts | The outline prompt remains a recognized purpose and stays exportable, because its hook remains. No local-draft surface offers Suggest an outline | Resolved |
| X07 | Tour conversations | [RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md): composer and one session line under the transcript; the update link; a change request answered with the change; Enhance in the part's session | RFC-DRAFT-0013: sessions scoped to the local draft; a refreshed candidate starts a new session (P04); proposals returned beside what they change | Local-draft conversations look and behave as RFC-DRAFT-0003 says. P04 decides only when a session ends. The Session menu lists earlier candidates' conversations as history | Inherits |
| X08 | Writing rules | RFC-DRAFT-0003: cite code as `path:first-last`; do not describe behaviour that was only moved behind a condition; versions advance | RFC-DRAFT-0013 adds two prompt purposes, description writing and whole-change discussion | Both rules are part of the two new purposes' built-in guidance from their first version | Inherits |
| X09 | Code references | [RFC-DRAFT-0004](RFC-DRAFT-0004-agent-chat-conventions.md): one recogniser; links only for references that will open; published as commit-pinned permalinks | RFC-DRAFT-0013: no guessed future commit links; links that need a pull request stay visibly pending | In a local draft a reference resolves against the candidate and opens it there. Its published permalink is one of the links that stay pending until the pull request's commits exist | Inherits |
| X10 | Send key, cost, agent accent | RFC-DRAFT-0004: Return sends in chats; cost per turn and per session in dollars and cents; indigo exactly where a control starts an agent run | The absorbed UX flow: conversations beside the material; the description editor saves text | Local-draft chats follow all three. The description and title editors are save composers and keep Ctrl or Cmd with Return | Inherits |
| X11 | Accent on extension actions | The accent test is whether pressing the control starts or continues an agent run | RFC-DRAFT-0015: extension actions such as Gather ticket context are handed to an interactive harness | An extension action bound to an agent wears the accent; one bound to a script or tool does not | Confirmed |
| X12 | What a question is about | [RFC-DRAFT-0005](RFC-DRAFT-0005-agent-questions-say-what-they-point-at.md): each request has a subject; selected lines attach as a chip; no scope list | RFC-DRAFT-0013: ask about the whole change, a field or a stop | The same subject model: the whole change, a field, a stop, or attached lines of the candidate. No scope list appears in a local draft | Inherits |
| X13 | A running turn | [RFC-DRAFT-0006](RFC-DRAFT-0006-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md): activity, helpers, the quiet notice, and no silent ending | RFC-DRAFT-0013: read-only runs in a candidate workspace | Local-draft runs show the same block and end the same three ways. Read-only runs start no helpers today, so the helper rows are simply absent | Inherits |
| X14 | Scratch folder and builds | [RFC-DRAFT-0007](RFC-DRAFT-0007-agent-scratch-folder-and-opt-in-builds.md): Agent tab sessions for a pull request; "Tour runs stay read-only" | RFC-DRAFT-0013 and its deferrals: local-draft conversations are read-only; no effect-capable session for a local draft | Neither the scratch folder nor builds is available in a local-draft conversation. Both packs already say so from their own side; extending them to local drafts would be a new decision | Inherits |
| X15 | What the Agent tab may do | RFC-DRAFT-0007 changes it: a scratch folder on to begin with, and builds in the disposable checkout when turned on | RFC-DRAFT-0015 describes Agent-tab sessions as acting "in a private workspace or, only by an explicit choice for each invocation, in an existing checkout" when explaining why no supervised action route exists | That description is out of date once RFC-DRAFT-0007 lands and is re-read then. Its conclusion is unaffected: a scratch script or a build is not a BPR-scoped route for an arbitrary configured action in the author's worktree, and ACTIONS-01 stands | Resolved |
| X16 | Builds and evidence | RFC-DRAFT-0007: an allowed agent may build and run tests in its disposable checkout | RFC-DRAFT-0012: every result is planned or reported; observed is reserved for a later BPR-supervised action | A build or test run by an Agent tab agent is the agent's report. If it reaches a local draft's evidence it arrives by import, as reported, bound to the material actually tested, which is the pull request's checkout and not a candidate | Resolved |
| X17 | New per-agent settings | RFC-DRAFT-0007 adds Scratch folder starts and Build and run tests to an agent's settings, and a remembered per-pull-request scratch choice | RFC-DRAFT-0010 lists the fields of an exported agent definition and holds that "Imported settings never silently relax a check" | Both settings join the exported definition as requested values. Build and run tests imports off unless the user accepts it for that entry in the preview, as relaxed run checks do. Scratch folder starts imports as requested when Off and as the recipient's own default otherwise. Neither changes an entry's identity. The per-pull-request scratch choice is local state and is never exported | Confirmed |
| X18 | Tour publication | RFC-DRAFT-0002: the plan chooses the form; steps advance; failures are error-role banners | RFC-DRAFT-0014: the tour is an explicitly selected follow-up after the pull request exists | The follow-up tour publication chooses its form and reports steps and failures the same way. Creating the draft pull request and its other follow-ups show their steps and failures in the same presentation | Inherits |
| X19 | Description when a tour publishes | RFC-DRAFT-0002 leaves "Update PR description after publishing" and its meaning unchanged; it is on by default today | RFC-DRAFT-0013 (P03): for a pull request created from a local draft, the accepted full description is authoritative and publishing its tour "defaults to leaving that body intact" | For a pull request associated with a local draft, the option starts off and turning it on goes through the composed preview. For every other pull request the existing default stands | Resolved |
| X20 | Unaccepted stops and publication | RFC-DRAFT-0009: publishing a tour requires every stop Accepted | RFC-DRAFT-0014: tests and proofs are not universal gates; unresolved items are shown honestly | Unaccepted stops block publishing the tour. They do not block Create draft PR…, whose preview lists them as unresolved | Confirmed |
| X21 | Evidence a stop cites | RFC-DRAFT-0009: a stop is accepted again when its lines or a link in its text change | RFC-DRAFT-0012 and RFC-0004: a stop may cite selected evidence, and evidence can come to need recheck | A stop that cites evidence which now needs recheck is Changed since accepted, with that as its reason | Confirmed |
| X22 | Submissions and other publication work | [RFC-DRAFT-0008](RFC-DRAFT-0008-a-review-opens-fresh-and-a-submission-always-ends.md): a submission always shows a stated condition, waits a bounded time, and is not blocked by another pull request's effects | RFC-DRAFT-0014 adds draft creation and follow-up effects | The new effects cannot hold up a review submission, and each shows waiting, sending, done or failed with its reason. Its read-first journal and no-blind-retry rules are unaffected | Inherits |
| X23 | Opening state | RFC-DRAFT-0008: every opened pull request starts on Comments with no side chat | RFC-DRAFT-0011: a local draft is durable and resumable | Durable preparation resumes; view state does not. A local draft opens on Changes with no chat open. A pull request created from it opens like any other | Confirmed |
| X24 | Imported prompt overrides | RFC-DRAFT-0003 and RFC-DRAFT-0005 change built-in guidance and what a request tells the agent, and advance the versions | RFC-DRAFT-0010: a prompt item carries its built-in version and digest provenance | An override written against an earlier built-in version imports as before, and the preview says which version it was written against | Inherits |
| X25 | Words | "Accepted" of a stop is the author's sign-off | That pack speaks of accepted proposals, an accepted description and an accepted refresh | Accepting a proposal, a description or a refresh puts something in the draft. Accepting a stop is the sign-off of RFC-DRAFT-0009. Text that could mean either names which | Resolved |

## Rules the owner confirmed

Six rows were the authoring session's proposals, drawn from both parts'
stated intent. They were put to the owner as a list on 2026-11-10, and the
owner answered "Confirm all six". They now bind the cards shown.

| Row | Governs |
|---|---|
| X02 | DRAFT-0033 and DRAFT-0042, Home after association |
| X11 | DRAFT-0044 to DRAFT-0046, action presentation |
| X17 | DRAFT-0028 and DRAFT-0029, the exchange format and its Settings flow |
| X20 | DRAFT-0041, the creation preview |
| X21 | DRAFT-0039, evidence in a tour |
| X23 | DRAFT-0033, opening and resuming |

## Order of delivery

The absorbed RFCs reuse the tour surface, the chats and the agent settings
that RFC-DRAFT-0001 to 0009 change, so those changes come first. The plan
encodes this as hard prerequisites:

- `local-tour-preparation` waits for `tour-publishing`, `tour-stops`,
  `tour-conversation` and `agent-tab-asks`, besides its own `author-evidence`
  and `share-agent-prompts`.
- `publish-prepared-draft` waits for `dependable-session`, so the new
  publication effects are built on the repaired lane.

Two things are not hard prerequisites:

- `share-agent-prompts` is independent, as the owner asked for it to be. Its
  format includes the two settings of X17 from its first version, whether or
  not `agent-scratch` has landed.
- `local-draft-entry` and `home-next-move` both change Home and may land in
  either order; X01 to X03 hold in both.

The seven absorbed milestones follow the nine RFCs' milestones in the plan's
default order. All seven are part of the 0.5.0 release: asked on 2026-11-10
whether the local PR draft milestones belong in it, with sharing alone
offered as the smaller choice, the owner answered "All of them".

Once RFC-DRAFT-0001 to 0009 land, the source anchors recorded for the
absorbed RFCs will have moved, and their current-behaviour notes are re-read
then.

## Checks

Every decision in this pack's [decision record](DECISIONS.md) was compared
against the other pack's six RFCs, its decision record, UX flow, glossary,
card index and entrypoint, and that pack was searched for the terms this
pack changes. No decision made for RFC-DRAFT-0001 to 0009 is overridden by a
row above; X19 narrows one unchanged existing default for one class of pull
request. After absorption the plan validates, every link in the pack
resolves and every criterion label is defined once in its RFC. No product
behaviour was run.
