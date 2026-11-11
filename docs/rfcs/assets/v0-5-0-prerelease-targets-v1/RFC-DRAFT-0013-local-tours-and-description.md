---
id: "RFC-DRAFT-0013"
title: "Prepare the description and Review Tour before a PR exists"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0004","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: ["RFC-DRAFT-0010", "RFC-DRAFT-0011", "RFC-DRAFT-0012"]
---

# Prepare the description and Review Tour before a PR exists

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
S16](local-drafts/CURRENT-BEHAVIOR.md).

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
([S16](local-drafts/CURRENT-BEHAVIOR.md)) with one change of scope: a local
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
[evidence record](RFC-DRAFT-0012-author-evidence.md#claims-and-checks). Author
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
[reconciliation record](RECONCILIATION.md); its row numbers are given.

- **The Tour area (X04).** It has the three bands of [RFC-DRAFT-0002](RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md): a head, the
  scrolling route and the pinned agent bar. Its head states the local tour
  (its stops and how many are still to accept) and carries no Publish
  action; creating the draft pull request belongs to the workbench header.
- **Stops (X05).** [RFC-DRAFT-0009](RFC-DRAFT-0009-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md) governs them: arriving at a stop reveals its lines
  in the candidate's Changes view; a stop at rest offers Edit and its chat;
  the author accepts a stop, with the green or yellow ring. On an accepted
  candidate refresh, a stop whose correspondence is exact stays Accepted; a
  changed, ambiguous, missing or unavailable correspondence makes it Changed
  since accepted, with that word as its reason. No local-draft surface
  offers Suggest an outline.
- **Evidence a stop cites (X21; confirmed by the owner, 2026-11-10).** A
  stop that cites evidence which now needs recheck is Changed since
  accepted, with that as its reason.
- **Conversations (X07, X12, X13).** They look and behave as [RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md) says:
  the composer and one session line, the update link, a change request
  answered with the change, Enhance in the part's session. The rule that an
  accepted refresh starts a new session decides only when a session ends;
  the Session menu lists earlier candidates' conversations as history. A
  question's subject follows [RFC-DRAFT-0005](RFC-DRAFT-0005-agent-questions-say-what-they-point-at.md): the whole change, a field, a stop, or
  attached lines of the candidate, with no scope list. A running turn shows
  the block of [RFC-DRAFT-0006](RFC-DRAFT-0006-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md) and ends in one of its three ways.
- **Chat conventions (X09, X10).** [RFC-DRAFT-0004](RFC-DRAFT-0004-agent-chat-conventions.md) applies: Return sends in the
  chats, cost shows per turn and per session, and the agent accent marks
  only controls that start a run. A code reference resolves against the
  candidate and opens it there; its published permalink is one of the links
  that stay pending until the pull request's commits exist. The description
  and title editors save text and keep Ctrl or Cmd with Return.
- **Writing rules (X08).** The two local-authoring prompt purposes carry the
  citation rule and the moved-code rule of [RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md) from their first version.
- **No scratch folder or builds (X14).** Local-draft conversations stay
  read-only, as this RFC and [RFC-DRAFT-0007](RFC-DRAFT-0007-agent-scratch-folder-and-opt-in-builds.md) each say from their own side.
- **The description option (X19).** [RFC-DRAFT-0002](RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md) leaves "Update PR description
  after publishing" as it is, on by default. For a pull request associated
  with a local draft that option starts off, and turning it on goes through
  the composed preview, so the publication ownership rule above holds. For
  every other pull request the existing default stands.
- **Words (X25).** Accepting a proposal, a description or a refresh puts
  something in the draft. Accepting a stop is the author's sign-off under
  [RFC-DRAFT-0009](RFC-DRAFT-0009-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md). Text that could mean either says which.

## Acceptance

- **AC01:** A signed-out author writes and resumes a description and ordered code tour against an uncommitted local candidate without a fabricated PR/account/commit identity.
- **AC02:** Selected evidence enters the explanation with its origin, scope and limitations; private deliberation and unselected records do not appear in the exact public preview.
- **AC03:** Candidate changes and late proposals preserve original material and later author edits, with explicit stale/ambiguous/missing destinations rather than guessed code links. Authoring runs use captured candidate repository inputs through the candidate workspace or tool-free context; unselected live-helper edits cannot enter that context even when a proposed stop has valid candidate lines. Out-of-candidate source references cannot be accepted as written. An accepted refresh starts a new session on the successor candidate while the earlier conversation stays readable against its own.
- **AC04:** Local authoring, conversation history and accepted proposals survive restart with no live agent process and no fabricated account or pull-request scope, while bounded context selection and no-agent editing remain usable; the local-authoring prompts are editable in Settings and round-trip through the sharing format.
- **AC05:** Description and tour can be edited together deliberately but never overwrite one another implicitly; the prepared-tour publication projection preserves the accepted full PR description.
- **AC06:** Against an exact simulated publication result, matching stops acquire the actual PR anchors while mismatched stops and unresolved URLs remain local and visibly incomplete; existing PR-owned tour workflows retain their behavior.
- **AC07:** A whole-change conversation proposes a claim or planned check that reaches the evidence record only through the author's acceptance, as a proposal with its agent origin and never as a reported or observed result.
- **AC08:** A local tour shows the three bands with no Publish action; arriving at a stop reveals its candidate lines; a stop is accepted with Accept, Save or its own proposal Accept; after an accepted refresh only stops whose correspondence is not exact are Changed since accepted, each with its reason; and its chats show the session line, cost, reference links and running block as a pull request's tour chats do.
- **AC09:** For a pull request associated with a local draft, "Update PR description after publishing" starts off and cannot change the body without the composed preview, while a pull request with no local draft keeps the existing default; a stop citing evidence that needs recheck is Changed since accepted.

## Proposed delivery

[Local narrative and tour ownership](cards/DRAFT-0038-add-local-description-and-tour-ownership.md),
[interactive authoring and mapping](cards/DRAFT-0039-prepare-an-evidence-backed-tour-and-publication.md), and
[closeout](cards/DRAFT-0040-close-pre-pr-description-and-tour-preparation.md) form the fourth [milestone](milestone-plan.json).
