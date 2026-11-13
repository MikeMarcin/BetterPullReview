---
id: "RFC-BPR-0110"
title: "Prepare the description and Review Tour before a PR exists"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0004","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: ["RFC-BPR-0107", "RFC-BPR-0108"]
---

# Prepare the description and Review Tour before a PR exists

## Owner amendment: no evidence in 0.5.0 (2026-11-12)

The owner cut Evidence from 0.5.0 ("Mockups; cut Evidence") and
[RFC-BPR-0109](RFC-BPR-0109-author-evidence.md) is Deferred. In this RFC,
every clause about evidence (selected claims, checks or results in the
description or the tour, a stop citing evidence, rule X21, a conversation
proposing claims or planned checks into the evidence record, formal evidence
staying with its basis) is inactive until that RFC is reactivated. The
public preview carries the title, the accepted description and the tour;
notes, conversations and proposals stay private. The criteria are amended
below; their earlier wording is kept here:

- AC02 was: "Selected evidence enters the explanation with its origin,
  scope and limitations; private deliberation and unselected records do not
  appear in the exact public preview."
- AC07 was: "A whole-change conversation proposes a claim or planned check
  that reaches the evidence record only through the author's acceptance, as
  a proposal with its agent origin and never as a reported or observed
  result."
- AC09 ended: "; a stop citing evidence that needs recheck is Changed since
  accepted."

## Problem and grounded current behavior

Current tour ownership includes a GitHub host, account, repository and pull
request. Source destinations include commits. A local draft containing dirty
material cannot honestly satisfy those identities. Tour publication normally
writes Problem and Solution into the PR description, although a no-description
route exists. Tour chats are native agent sessions: their history and resume
handles are durable, they are scoped to that same account and pull request, and
they read a private workspace pinned to the pull request's base and head, never
the author's worktree. A local draft has no such authority, and its candidate
may hold files no commit contains. See [S6–S7 and
S16](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

The owner wants the explanation and tour ready before GitHub draft creation.
Enterprise descriptions may have strict templates and permanent-history meaning.
This requires local authoring and an explicit publication mapping, not fabricated
PR IDs or competing automatic writers of the same body.

## Proposed behavior and boundaries

### Author the public material deliberately

A local PR draft has an editable title and complete Markdown description. The
built-in initial guidance offers Problem and Solution and selected verification
information. It does not require an enterprise tag, ticket, risk scale or
template. An author or configured assistant can propose other text; accepting
a proposal is an explicit versioned edit to this one publication body.

The local Review Tour retains its Problem/Solution brief and ordered code stops,
but does not silently synchronize or overwrite the description. Authors can
deliberately transfer selected passages or accept a proposed coordinated edit
after seeing the affected fields. This gives the full publication body a single
authority while keeping tour prose useful. Existing PR-based tour authoring
outside this local-draft flow retains its established behavior.

The author can compose a tour, attach selected claim/evidence references, reorder
stops and discuss a particular stop before any account or remote PR exists.
Evidence is selected rather than copied wholesale: a stop may summarize a
reproducer and its result while retaining the method's limits and a reference to
the detailed artifact. A question can explicitly request reviewer attention.
Tests, formal models and ordinary author judgments keep their qualifications.

### Source identity and continuity

Local tour stops bind to exact candidate/source material and semantic source
ranges. They have local stable identities, not guessed future commit links.
Introduce a local tour ownership/material seam and a deliberate conversion to
the existing PR-owned tour; do not relax GitHub authority by nullable account or
PR fields across all existing tours. Reuse ordered-stop, Markdown, diff identity
and proposal primitives where their contracts fit.

Preserve the existing limits: at most 256 stops, 64 KiB per tour text field,
32 related sources per stop and a 4 MiB authored-tour budget. Local description
and title follow the actual selected publisher's bounded fields, with an early
clear limitation and a final exact preview. No silent truncation is permitted.

Changing the candidate retains original stop material and indicates exact,
changed, ambiguous, missing or unavailable correspondence. It cannot move a stop
to a convenient nearby line or silently mark it reviewed. Stale agent proposals
remain inspectable and cannot overwrite later author edits. Formal evidence
stays attached to its original basis unless separately reconciled.

On publication, the association process verifies the actual outgoing material
and maps admitted stops to the resulting PR's exact commits and comparison.
Unmappable stops remain local and need attention; they are not dropped or
published elsewhere. PR-dependent links can remain visibly pending until actual
identities exist. Preparation does not pretend that locally held images or
unpublished artifact references are publicly reachable URLs.

### Resumable interaction

Description, tour, accepted proposals and author questions are durable local
preparation. Agent conversations follow the adopted native-session contract
([S16](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)) with one change of scope: a local
draft's sessions belong to the profile, the local draft, the tour thread and the
agent entry, with no fabricated account, host or pull request. History survives
restart, an idle session needs no live process, and New session, returning to
history and the labelled transcript-replay fallback behave as they do for a
pull request's tour. Replay keeps its current limits of 12 turns within 48 KiB
of context and a 160 KiB request; persisting history is not a requirement to
send all of it to an agent. Explicit source selection and existing privacy
boundaries apply. Local text/history remain private unless selected for public
output; a collapsed GitHub details section is still publication.

Allow asking about the whole change, a field or a stop; return proposed edits
next to the material they change. A whole-change conversation can also propose
claims and planned checks, which the author accepts or rejects in the
[evidence record](RFC-BPR-0109-author-evidence.md#claims-and-checks). Author
edits and discussion remain usable without invoking a model.

An agent asked about a local draft reads the candidate, not whatever the worktree
holds now. A pull request's tour runs already read a private workspace
materialized from pinned commits and isolated from the active worktree. A local
draft's runs get the same kind of workspace, materialized from the captured
candidate: its committed head through the existing path, and selected
uncommitted files from their captured bytes, never by copying the live worktree.
Runs in it are read-only, and this pack adds no effect-capable session that
shares it.

The candidate is the session's source binding in the adopted sense. An accepted
refresh makes a successor candidate with its own workspace and a new session;
the earlier conversation stays readable against the candidate it discussed, and
its unaccepted proposals fall under the stale-proposal rules above. If the
runtime cannot use the candidate workspace, use the existing tool-free writing
lane with explicitly selected captured candidate context and disclose its
limited coverage. If neither route can keep repository inputs bound to the
candidate, leave that agent operation unavailable; manual authoring remains
usable. Reading the live worktree is not a fallback.

Validate proposed source references against the candidate and retain the
stale-proposal rules above. Matching paths or line ranges cannot establish which
code informed a natural-language explanation; this check does not replace the
input boundary. An unselected helper edit must not enter the run's repository
context merely because the proposed stop points to unchanged candidate lines.

A configured writing skill gives guidance only; it
does not acquire test-running or publication authority. This RFC adds the
local-authoring prompt purposes (description writing and whole-change
discussion) as recognized, editable Settings prompts and as items of the sharing
format, under that format's rule for purposes an older importer does not
recognize. Tour prompts keep their existing purposes.

### Publication ownership

For a PR created from this flow, its accepted full description is authoritative.
Publishing its prepared tour defaults to leaving that body intact; altering it
requires another explicit composed preview. Enterprise tools may format selected
material into their body, but a later tour publish cannot erase their template,
hidden markers or author checklist. The publication RFC owns remote operations,
partial results and recovery. This RFC proves local authoring and truthful
mapping requirements; it does not claim a live remote result.

### Fitted to this pack's other decisions

A local tour is built from the same parts as a pull request's tour, so it
takes their behaviour from this pack's RFCs. These rules come from the
[reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md); its row numbers are given.

- **The Tour area (X04).** It has the three bands of [RFC-BPR-0099](RFC-BPR-0099-tour-publishing-in-reach-and-reported.md): a head, the
  scrolling route and the pinned agent bar. Its head states the local tour
  (its stops and how many are still to accept) and carries no Publish
  action; creating the draft pull request belongs to the workbench header.
- **Stops (X05).** [RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md) governs them: arriving at a stop reveals its lines
  in the candidate's Changes view; a stop at rest offers Edit and its chat;
  the author accepts a stop, with the green or yellow ring. On an accepted
  candidate refresh, a stop whose correspondence is exact stays Accepted; a
  changed, ambiguous, missing or unavailable correspondence makes it Changed
  since accepted, with that word as its reason. No local-draft surface
  offers Suggest an outline.
- **Evidence a stop cites (X21; confirmed by the owner, 2026-11-10;
  inactive since the 2026-11-12 amendment).** A stop that cites evidence
  which now needs recheck is Changed since accepted, with that as its
  reason.
- **Conversations (X07, X12, X13).** They look and behave as [RFC-BPR-0100](RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md) says:
  the composer and one session line, the update link, a change request
  answered with the change, Enhance in the part's session. The rule that an
  accepted refresh starts a new session decides only when a session ends;
  the Session menu lists earlier candidates' conversations as history. A
  question's subject follows [RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md): the whole change, a field, a stop, or
  attached lines of the candidate, with no scope list. A running turn shows
  the block of [RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md) and ends in one of its three ways.
- **Chat conventions (X09, X10).** [RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md) applies: Return sends in the
  chats, cost shows per turn and per session, and the agent accent marks
  only controls that start a run. A code reference resolves against the
  candidate and opens it there; its published permalink is one of the links
  that stay pending until the pull request's commits exist. The description
  and title editors save text and keep Ctrl or Cmd with Return.
- **Writing rules (X08).** The two local-authoring prompt purposes carry the
  citation rule and the moved-code rule of [RFC-BPR-0100](RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md) from their first version.
- **No scratch folder or builds (X14).** Local-draft conversations stay
  read-only, as this RFC and [RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md) each say from their own side.
- **The description option (X19).** [RFC-BPR-0099](RFC-BPR-0099-tour-publishing-in-reach-and-reported.md) leaves "Update PR description
  after publishing" as it is, on by default. For a pull request associated
  with a local draft that option starts off, and turning it on goes through
  the composed preview, so the publication ownership rule above holds. For
  every other pull request the existing default stands.
- **Words (X25).** Accepting a proposal, a description or a refresh puts
  something in the draft. Accepting a stop is the author's sign-off under
  [RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md). Text that could mean either says which.

## Acceptance criteria

- **AC01:** A signed-out author writes and resumes a description and ordered code tour against an uncommitted local candidate without a fabricated PR/account/commit identity.
- **AC02:** Private deliberation (notes, conversations and proposals) does not appear in the exact public preview, which carries the title, the accepted description and the tour. (Amended 2026-11-12; see above.)
- **AC03:** Candidate changes and late proposals preserve original material and later author edits, with explicit stale/ambiguous/missing destinations rather than guessed code links. Authoring runs use captured candidate repository inputs through the candidate workspace or tool-free context; unselected live-helper edits cannot enter that context even when a proposed stop has valid candidate lines. Out-of-candidate source references cannot be accepted as written. An accepted refresh starts a new session on the successor candidate while the earlier conversation stays readable against its own.
- **AC04:** Local authoring, conversation history and accepted proposals survive restart with no live agent process and no fabricated account or pull-request scope, while bounded context selection and no-agent editing remain usable; the local-authoring prompts are editable in Settings and round-trip through the sharing format.
- **AC05:** Description and tour can be edited together deliberately but never overwrite one another implicitly; the prepared-tour publication projection preserves the accepted full PR description.
- **AC06:** Against an exact simulated publication result, matching stops acquire the actual PR anchors while mismatched stops and unresolved URLs remain local and visibly incomplete; existing PR-owned tour workflows retain their behavior.
- **AC07:** Withdrawn by the owner's 2026-11-12 amendment: with RFC-BPR-0109 deferred there is no evidence record for a conversation to propose into.
- **AC08:** A local tour shows the three bands with no Publish action; arriving at a stop reveals its candidate lines; a stop is accepted with Accept, Save or its own proposal Accept; after an accepted refresh only stops whose correspondence is not exact are Changed since accepted, each with its reason; and its chats show the session line, cost, reference links and running block as a pull request's tour chats do.
- **AC09:** For a pull request associated with a local draft, "Update PR description after publishing" starts off and cannot change the body without the composed preview, while a pull request with no local draft keeps the existing default. (Amended 2026-11-12; see above.)

## Implementation record

Milestone `local-tour-preparation`; evidence in the
[verification ledger](../verification/RFC-BPR-0110.md). Decisions made within
the behaviour above, for the owner to see. The evidence parts of this record
(citations, `evidence_recheck`, claim and check proposals, selected evidence
in the request and the preview) were removed with the 2026-11-12 amendment;
the record below describes what remains.

- **Where it lives.** The description, the tour, proposals and conversations
  are the local draft's `Preparation` (`core/local_draft_tour.hpp`), stored in
  the draft's record in `local-drafts.sqlite3`, so they are versioned,
  resumed, discarded and reset with the draft. Nothing in it names an
  account, host, repository identity or pull request.
- **Binding.** A stop binds a candidate's ordinal, path, side, lines and the
  exact bytes of those lines. On a successor candidate the same bytes at the
  same lines, or at exactly one other place in the file, are `exact`; at
  several places `ambiguous`; not there `changed`; a deleted file `missing`;
  a file the candidate does not keep as text `unavailable`. The stop's own
  binding never moves; Accept on a later candidate rebinds it to the exact
  lines and keeps the earlier binding.
- **Acceptance.** A stop is Accepted on a fingerprint of its title, text
  and lines. After an accepted refresh only a stop whose
  correspondence is not exact is Changed since accepted, with that word.
  Accept needs exact lines; Save with lines chosen in Changes rebinds and
  accepts; a stop's own proposal Accept accepts it when its lines are exact.
- **Description authority.** Title and body are saved together as one edit;
  the body's bound is 65,536 bytes, refused rather than cut. The tour's
  Problem and Solution never write it; Add from Tour copies one of them to its
  end on request. Each field has a revision.
- **Proposals.** An answer's one `bpr-proposals` block (JSON) proposes a
  title, description, Problem, Solution, stop text or new stop. Each records the field revision and the candidate it read;
  a later edit or refresh makes it stale, readable and refused. A new stop
  outside the candidate's lines is recorded as such and never accepted.
- **Conversations.** One conversation per candidate, thread (`change` or
  `stop:N`) and agent entry identity; an accepted refresh opens a new one and
  the Session menu lists earlier ones read-only. History is the stored turns.
- **The agent route.** This delivery uses the tool-free writing lane only:
  the draft's host composes one request from the instructions, the draft's
  preparation, the replayed turns (12 within 48 KiB)
  and the candidate's changed files read from the private store, the
  subject's file first, within 160 KiB, and says how many files it sent. The
  agent starts in an empty folder with no tools and no saved session, so it
  cannot read the worktree; every turn replays rather than resumes. The
  candidate workspace (read-only tools over a materialized candidate) and
  native resume are not built; agents other than Claude's stream JSON are
  unavailable here and manual authoring is unaffected.
- **Prompts.** `local.description` and `local.discussion` are writing hooks
  (profile schema 28), listed under Built-in prompts › Local drafts,
  exported and imported like the others (RFC-BPR-0107).
- **Surface.** The draft page has Changes, Description and Tour.
  Description is the body, Add from Tour, Preview… (the exact title, body and
  tour text, the unaccepted stops and the pending links) and Save, with the
  agent bar. Tour is the head (its stops and how many to accept, Add Stop, no
  Publish), the route (Problem, Solution, the stops with their rings and
  reason, the selected stop's editor with Show, Use Selection, More, Remove,
  Accept and Save) and the pinned agent bar (transcript with candidate links,
  proposals with Accept and Reject, session line with cost, Session menu, Ask
  in the agent accent).
- **Mapping.** `map_local_publication` takes an exact publication result
  (its head, body and files) and anchors only exact stops, with a
  commit-pinned permalink; the rest stay local with their reason. Creating
  the pull request is RFC-BPR-0111's; no publication route calls it yet.
- **X19.** A pull request whose Home row carries a local draft's mark opens
  its tour with "Update PR description after publishing" off; changing it
  asks for a new Preview, as every change of that option does.

### In the pull request window (2026-11-13)

The owner's redesign ("I thought we chose to stick with the PR view and just
augment it"; accepted design `docs/design/local-draft-pr-window-v1`) moved the
local draft into the pull request window and rejected the draft's own
Description and Tour areas. They are deleted. What replaced them:

- **The Tour tab is the window's own Tour panel** (RFC-BPR-0099's head, route
  and agent bar; RFC-BPR-0106's stops at rest and editing, rings, Accept;
  RFC-BPR-0100's side chats and Enhance). `core::view_local_tour` projects the
  draft's preparation into the shapes the panel reads (a `review_tour`, a
  `tour_agent_session`, a `tour_proposal`), and `translate_local_tour_command`
  turns the panel's commands back into the draft's own edits. The draft's
  record is unchanged in form; nothing projected is stored, published or sent.
  The projected owner and commit identities are derived from the draft and a
  candidate's ordinal in a domain of their own, as the viewer's comparison
  identities are (`local_presentation`); where the panel would show a commit,
  it shows "candidate N".
- **Local-only differences through the same panel.** No Publish in the head
  and no publication page for a local tour (X04). A stop's ring comes from
  `decide_local_stop`, with the draft's own reason word ("Changed since
  accepted: changed.", X05). Typing saves a stop's text without accepting it
  (`edit_local_stop`); Save accepts; Mark not accepted withdraws
  (`unaccept_local_stop`). Use Selected Lines rebinds to the latest
  candidate's lines. Proposals on the Problem, Solution or a stop wait in the
  panel; a stale one shows refused ("the target changed"); a stop outside the
  candidate is left out. Problem and Solution have their own chats (threads
  `problem`, `solution`). Enhance and a field's draft ask the part's chat for
  a proposed rewrite. Draft the tour asks the whole change. Ask more is not
  offered: every local turn replays or resumes its conversation.
  Split, duplicate, merge, undo and outlines do nothing for a local tour.
- **The description is Overview's.** Overview's description editor saves the
  draft's body (with its title); Rename… in the candidate pop-up changes the
  title. The exact public preview is `compose_local_publication` (title, body,
  tour; no conversation, note or proposal).
- **The carry (On publication).** `carry_local_tour_to_pull_request` maps the
  draft's tour onto its pull request's empty private tour draft at the pull
  request's head: an exact stop is anchored there and keeps its acceptance; any
  other comes across with its text and no lines, needing attention, never
  dropped. The window offers it once, when the pull request's tour draft loads
  empty and Home marks it as made from the draft. `map_local_publication`'s
  anchors now count lines from zero, as every tour destination does (they were
  one-based and unused).

## Proposed delivery

[Local narrative and tour ownership](../issues/60-done/BPR-2026-add-local-description-and-tour-ownership.md),
[interactive authoring and mapping](../issues/20-ready/BPR-2027-prepare-an-evidence-backed-tour-and-publication.md), and
[closeout](../issues/10-backlog/BPR-2028-close-rfc-0110.md) form the fourth [milestone](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
