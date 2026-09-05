---
id: "RFC-BPR-0023"
title: "Private Review Threads, Provenance, and Comment Composition"
status: "Deferred"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0007","RFC-BPR-0008","RFC-BPR-0011","RFC-BPR-0012"]
pack_version: "post-v2"
---

# RFC-BPR-0023: Private Review Threads, Provenance, and Comment Composition

## Status and scope

This is a deferred post-pack RFC. It does not alter the canonical fourteen-RFC
pack-v2 sequence and cannot become `Auto-Verified` before every dependency is
`Auto-Verified`.

The product owner's 2026-09-04 request authorizes reconciliation and import
only and explicitly defers implementation. All imported cards remain in backlog
until a later implementation request. This supersedes the retained pack's claim
of implementation authority. Import waives no dependency ordering, target-native
evidence, owner-attended gate, or designated GitHub mutation-target policy.
The source pack called this RFC-BPR-0022; import assigned RFC-BPR-0023 because
the companion off-hunk publication pack takes RFC-BPR-0022 in the requested order.

The feature crosses four existing authorities:

- RFC-BPR-0007 owns durable private review state;
- RFC-BPR-0008 owns ordinary GitHub drafts, publication, acting identity, and
  server reconciliation;
- RFC-BPR-0011 owns agent profiles, sessions, process isolation, and explicitly
  selected private context;
- RFC-BPR-0012 owns explicit AI actions, findings, side chat, workshop results,
  and the rule that AI output remains local until the reviewer acts.

This RFC joins those authorities. It does not replace or duplicate any of them.

## Summary

Add durable **private review threads** containing reviewer and agent turns,
retain exact authorship and derivation provenance, and let the reviewer select
one or more private revisions or findings and **Squash to Draft**.

Squashing is a non-destructive local composition operation. It creates or
updates a local RFC-BPR-0008 draft and records lineage; it never publishes. A
separate explicit publish gesture uses the existing GitHub mutation path. The
public result is an ordinary GitHub comment under the authenticated reviewer's
identity, with no hidden BetterPullReview metadata.

## Problem

A reviewer often wants AI help while deciding what they believe, not an agent
that speaks directly to the pull request. The useful intermediate work is a
private deliberation:

- a reviewer notices something and writes a local note;
- an agent analyzes the same code;
- the reviewer asks a follow-up;
- another agent challenges or refines the claim;
- the reviewer combines only the defensible pieces into a concise public
  comment.

BetterPullReview already enforces the important publication boundary, but the
intermediate deliberation has no durable first-class representation. A local
note is one flat text value. A side-chat session owns current interaction state
but not a durable transcript. A structured finding carries rich data while it
is in the finding queue, yet current promotion can flatten it into an anonymous
note or draft. A draft records only its anchor, body, and state.

The result is a false choice: keep useful reasoning outside the product, or
flatten it early and lose who contributed what. This RFC adds the missing local
layer without weakening the GitHub boundary.

## Findings established from the 2026-09-04 source baseline

These findings were read from the source dump identified by the pack manifest.
The implementation card must re-audit them against the live tree before editing.

1. `local_note_record` in
   `src/application/include/bpr/application/review_ledger_port.hpp:75-81`
   stores an ID, optional file/comparison attachment, and note text. It carries
   no author, agent session, finding identity, model, effort, or derivation.
2. `private_ledger_observation` at the same path (`:99-105`) loads all retained
   notes into one bounded aggregate. A transcript feature must not extend that
   aggregate into an all-turn load.
3. `review_ledger_schema_version` is `5` in
   `src/adapters/sqlite/src/review_ledger_database.hpp:26`. The canonical schema
   contains `local_note` and retained agent-session metadata, but no private
   thread, turn, agent-run provenance, composition, or publication-lineage
   authority.
4. `review_comment_draft` in
   `src/core/include/bpr/core/review_round_trip.hpp:60-72` stores ID, exact
   `source_line_ref`, body, and state. It has no provenance or composition
   identity. In the application composition, the current draft is process-local
   and allocated from `NextReviewDraft`.
5. `agent_profile` in
   `src/core/include/bpr/core/agent_profile.hpp:93-106` names the executable,
   protocol, workspace policy, and environment-name allowlist. It does not say
   which model or reasoning effort a particular run actually used.
6. `agent_session` in
   `src/core/include/bpr/core/agent_session.hpp:69-76` records only session ID,
   profile ID, and exact revision binding. One session may carry multiple
   turns, so session identity alone is not run identity.
7. `agent_protocol_decoder` retains remote session/thread/turn strings
   internally at
   `src/application/include/bpr/application/agent_launch.hpp:346-348`, but
   `agent_run_result` at `:381-398` exposes neither those values nor requested or
   reported model/effort/tool-version facts.
8. `agent_chat_session_host` in
   `src/application/include/bpr/application/agent_chat_session_host.hpp:267-296`
   deliberately owns current side-chat session policy and dispatches through
   the one invocation path. It does not own a durable transcript.
9. `agent_finding` in
   `src/core/include/bpr/core/agent_finding.hpp:98-117` retains provider,
   analyzed revisions, structured claim/evidence/consequence, and optional
   suggested comment. The current finding host retains the latest delivery and
   can promote one finding to one local artifact; the durable note form does not
   retain that source lineage.
10. `review_mutation_receipt` in
    `src/application/include/bpr/application/review_mutation_port.hpp:204-210`
    returns the authoritative server comment ID. That is sufficient to correlate
    a local composition with the ordinary GitHub comment after successful
    publication without modifying the public body.

## Design principles

### Private is an authority boundary, not a visual style

Private threads, turns, provenance, compositions, and unpublished drafts are
local-ledger state. They are not GitHub state and must not be represented as
though another reviewer can see them. Every private surface must state that it
is local and not visible on GitHub.

### Authorship and derivation are different facts

Authorship says who produced the exact bytes of one revision. Derivation says
which prior revisions, findings, or compositions influenced it. Editing an
agent response does not make the agent disappear; it creates a new
reviewer-authored revision derived from the agent revision.

### Provenance must state its basis

BetterPullReview must distinguish what the reviewer requested, what a profile
configured, what the protocol reported, what the agent merely stated in text,
and what is unknown. It must not collapse those into one confidently worded
model label.

### History is immutable; current state is projected

A turn may have successor revisions, but an accepted revision is never edited
in place. The UI may show only the latest revision by default while retaining
history and lineage. Composition and publication add records; they do not
consume or erase their sources.

### Anchors are semantic and exact

A private thread may be attached only through stable review and RFC-BPR-0004
identities. Staleness and unplaceability are visible states. There is no
nearest-line fallback, no measured-coordinate persistence, and no silent
retargeting after a head move.

### Composition and publication are separate commands

`Squash to Draft` creates local state. `Publish` is a second explicit command
through RFC-BPR-0008 after target/head/identity revalidation. No UI gesture,
keyboard shortcut, host callback, or agent result may combine those operations.

## Terminology

**Private review thread** — A durable local conversation bound to one review
scope and an optional exact semantic subject.

**Turn** — One conversational contribution. A turn has stable identity and one
or more immutable revisions.

**Turn revision** — The exact body, author, completion state, and digest for one
version of a turn.

**Review actor** — The typed origin of a revision: the local reviewer, one exact
agent run, or `legacy_unknown` for data whose origin was not retained.

**Agent run** — One explicit invocation or side-chat turn execution under an
agent session. It has its own identity because one session may execute multiple
runs.

**Runtime provenance** — The bounded facts about the profile, protocol,
revision binding, requested settings, protocol-reported settings, tool version,
remote run identifiers, and terminal outcome of one agent run.

**Derivation edge** — A typed immutable relationship from a newer local object
to an exact source revision or finding snapshot.

**Review composition** — A local workspace that snapshots selected sources and
produces a candidate comment body and target.

**Publication lineage** — The local correlation from an accepted composition
and draft revision to the authoritative GitHub mutation receipt and reconciled
server comment.

## Goals

- Persist reviewer and agent discussion as private review threads across
  restart.
- Support PR-level and exact diff-subject threads without presentation
  coordinates.
- Display whether each exact revision was authored by the reviewer, an agent
  run, or an unknown legacy source.
- Record the best available model and effort information without overstating
  certainty.
- Preserve revision and many-to-many derivation history.
- Let the reviewer select exact turn revisions and structured findings and
  squash them into a local draft.
- Support deterministic reviewer-authored composition without invoking AI, and
  optional explicit agent-assisted composition through the one existing agent
  invocation path.
- Preserve all selected private sources after composition and publication.
- Correlate a successful publication with the returned GitHub comment identity
  while leaving the public body ordinary.
- Preserve the existing explicit private-context opt-in and revision-staleness
  policies.
- Bound storage, loading, projection, and rendering work.

## Non-goals

- Synchronizing private threads or provenance across machines.
- Publishing, resolving, approving, requesting changes, or verifying anything
  automatically.
- Letting an agent bypass the RFC-BPR-0008 draft and mutation path.
- Calling model APIs directly or storing provider credentials.
- Inferring model, effort, tool version, or identity from executable names,
  profile labels, response style, prompt text, or provider marketing names.
- Treating AI confidence or provenance as proof that a review claim is correct.
- Adding hidden metadata, HTML comments, bot markers, custom trailers, or a
  mandatory AI disclosure to GitHub comment bodies.
- Persisting raw environment values, credentials, arbitrary process logs,
  unrelated filesystem content, or duplicated source bodies.
- Turning every stream frame or tool event into a conversational turn.
- Building cross-review or team-shared private knowledge management.
- Replacing GitHub's ordinary shared conversation model.
- Requiring PR-level publication targets before RFC-BPR-0008 exposes them. The
  required direct path for this baseline is the existing inline
  `source_line_ref` draft; additional native draft targets may be adopted only
  through RFC-BPR-0008's authority.

## Product behavior

### Creating and continuing a private thread

The reviewer can create a private thread from:

- the pull request as a whole;
- an exact file comparison;
- an RFC-BPR-0004 source line, source boundary, selected range, change group,
  alignment segment, or move relation;
- an ordinary GitHub conversation identity;
- an exact validated agent finding snapshot.

The creation surface must say, in equivalent product language:

> Private · stored on this device · not visible on GitHub

A reviewer-written note creates a reviewer-authored turn revision. Asking an
agent creates a reviewer-authored prompt turn and one `agent_run`. A complete
assistant response creates an agent-authored successor turn linked to that run.
The side chat and private thread UI must use the existing invocation host rather
than adding a second process-launch path.

### Thread and turn lifecycle

Thread lifecycle is `open`, `resolved_locally`, or `archived`. Staleness,
composition, and publication are derived badges rather than thread lifecycle
states:

- one thread can yield more than one draft or public comment;
- a published thread can continue receiving private turns;
- resolving locally does not resolve a GitHub thread;
- archiving hides the thread from the default active view without deleting it.

A turn is stable. Editing creates a successor revision under the same turn. The
latest revision is the default projection; history remains inspectable.

Agent terminal failures, cancellation, timeout, and protocol failure are
retained as run outcomes. Partial assistant output may be shown as incomplete,
but it is not eligible for automatic composition. The reviewer may explicitly
adopt or copy it into a new reviewer-authored revision, which retains an
`adopted_from_incomplete_run` derivation.

### Provenance presentation

The compact author line should distinguish at least:

```text
You
Codex · model not reported
Codex · GPT-5.6 Pro (requested, not confirmed) · high effort (requested)
Claude · claude-sonnet-* (protocol reported) · effort not reported
You · edited from Codex response
Legacy local note · author unknown
```

The exact strings and product names are presentation policy, but the facts and
qualification are not. A details affordance must expose:

- profile identity and display name;
- protocol;
- exact review/base/head/epoch binding;
- requested model and effort, when present;
- configured defaults, when present;
- protocol-reported model and effort, when present;
- content-self-reported values, when retained;
- tool/harness version, when reported;
- remote session/thread/turn/run identifiers, when reported;
- terminal outcome;
- provenance conflicts, including requested and reported values that differ.

A compact label may prefer a protocol-reported value over a requested value,
but the detailed record must retain both. `Unknown` is a real state and is never
filled by inference.

### Squash to Draft

The reviewer selects one or more exact current or historical turn revisions,
validated finding snapshots, or existing local note migration records and
chooses **Squash to Draft**.

The product opens a local composition workspace showing:

- the exact selected sources and their authorship/provenance;
- source body digests and whether any source has since gained a successor;
- source revision and anchor staleness;
- the intended draft target;
- the candidate body;
- the actor and derivation of each candidate revision.

The workspace supports three explicit modes:

1. **Reviewer-authored.** The reviewer writes the candidate while sources remain
   visible. No agent runs.
2. **Exact copy.** One source's eligible body or a finding's suggested comment
   seeds the candidate byte-for-byte, with a `copied_from` edge.
3. **Agent-assisted squash.** The reviewer selects an allowed profile and
   explicitly sends only the selected sources through the existing invocation
   path. The returned proposal is an agent-authored candidate revision with
   exact run provenance. It remains a proposal until accepted.

Editing any candidate creates a reviewer-authored successor. Accepting a
candidate creates or updates a local RFC-BPR-0008 draft and stores the
composition identity on the draft. It does not call a mutation port.

The required baseline target is one exact inline `source_line_ref`. When all
selected sources share that target it may be proposed automatically. Otherwise
the reviewer must explicitly choose an exact target or choose **Split into
Drafts**, which partitions eligible sources by exact target. The product never
chooses a neighboring line. Sources without a publishable target remain local
until the reviewer supplies one.

### Publish and round trip

Publishing is the existing RFC-BPR-0008 operation:

1. the draft enters `publishing` only after an explicit gesture;
2. the core emits a semantic publication proposal;
3. the host revalidates host, repository, PR, expected head, anchor, acting
   identity, safe target, and idempotency;
4. the GitHub adapter performs the ordinary mutation;
5. the host reconciles server truth;
6. the local ledger records publication lineage using the mutation receipt's
   `ServerCommentId`, exact target, acting identity snapshot, public body digest,
   and reconciled server identity.

The reviewer-approved prose is byte-preserved. Native publication sends that
body unchanged. When RFC-BPR-0022 selects a file-level representation, the wire
body is its canonical visible code permalink, a blank line, and that exact prose.
This generated public anchor is not private provenance. The composition path
must not add a provenance footer, invisible marker, or mandatory AI disclosure.
A reviewer may write a visible disclosure as ordinary prose.

RFC-BPR-0022 owns representation selection and parent/child mutation recovery;
this RFC consumes its successful logical publication receipt. Local lineage
records approved prose and public wire-body digests separately, representation,
logical publication identity, and the canonical server receipt. A child attempt
is not a second publication. Saving or recovering lineage never retries a POST.
Both RFCs reuse the one RFC-BPR-0008 draft and coordinator. Cross-pack card
dependencies order the shared publication integration; they do not make local
private-thread creation or composition depend on an off-hunk write.

A PR-wide, range, boundary, or historical private subject does not itself grant
a publication target. The required public path remains an explicitly selected,
eligible exact current/right source line under RFC-BPR-0008 and RFC-BPR-0022.
RFC-BPR-0021 retains the general historical shared-comment revision join.

After publication, the private sources remain intact. The UI may show that a
source contributed to one or more drafts/comments and may warn when the same
exact source set/body/target was already published. Such a warning is advisory;
intentional restatement remains possible through an explicit publish flow.

## Normative domain model

Names may be reconciled during architecture allocation, but the distinctions
below are normative.

### Identities

Use independent strong-ID namespaces for:

- `private_review_thread_id`;
- `private_review_turn_id`;
- `private_review_turn_revision_id`;
- `agent_run_id`;
- `review_composition_id`;
- `review_composition_revision_id`;
- `review_publication_lineage_id`.

IDs are durable, monotonic within one exact review scope, and never allocated by
scanning retained rows. Exhaustion is a typed quota refusal.

### Thread subject

A thread stores an immutable review binding plus one typed subject variant. The
subject may be PR-wide or name an RFC-BPR-0004 identity, ordinary GitHub
conversation identity, or exact agent finding reference. It never stores a Qt
model index, screen coordinate, visual row, ribbon geometry, or display-only
path as authority.

Reanchoring does not mutate the original subject. If a future feature supports
reviewer-approved reanchoring, it creates an explicit successor subject/thread
relation and preserves the original.

### Actor and authorship

Every turn revision has exactly one actor:

- `reviewer`, optionally correlated with the local GitHub account identity used
  for the review scope;
- `agent_run`, naming one durable run;
- `legacy_unknown` for migrated data whose origin was not retained.

There is no free-form authoritative author label. Presentation labels are
derived from typed actor and provenance facts.

### Agent-run provenance

One durable agent-run record snapshots at least:

- run ID and parent agent session ID;
- profile ID and profile display name at dispatch;
- protocol;
- exact review/revision binding;
- invocation entry point and selected context identities;
- requested model, requested effort, and other supported requested settings;
- profile-configured defaults that materially affected the request;
- protocol-reported model, effort, tool/harness version, and remote identifiers;
- content-self-reported model/effort only when explicitly parsed into a distinct
  low-trust field;
- terminal outcome and completeness;
- bounded input-selection digest and output digest;
- whether private context was included and exactly which local source IDs were
  selected.

Requested, configured, protocol-reported, and content-self-reported values are
separate optional facts. A missing value is not filled from another category.
When values disagree, both are retained and a typed conflict diagnostic is
projected.

The record snapshots the revision binding rather than depending on the retained
agent-session catalog, because that catalog currently prunes old session rows.
A provenance record referenced by a retained private turn must remain readable
after session pruning.

### Turn revisions and lineage

A turn revision stores:

- stable turn and revision IDs;
- predecessor revision, if any;
- exact actor;
- bounded body and cryptographic body digest;
- completion state (`complete`, `incomplete`, `failed`, or `cancelled`);
- optional reply-to revision;
- immutable derivation edges;
- exact subject/revision binding inherited from the thread;
- monotonic local ordering information.

Wall-clock timestamps may be retained for presentation but do not determine
identity, ordering legality, or replay outcomes.

Derivation relation kinds include at least:

- `replies_to`;
- `edited_from`;
- `copied_from`;
- `refined_from`;
- `composed_from`;
- `promoted_from_finding`;
- `adopted_from_incomplete_run`.

Edges point to exact immutable source revisions or a durable finding snapshot
identified by run, finding ordinal, and content digest. They are scoped to the
same exact review and may not form a cycle. An edit never transfers authorship
from the predecessor.

### Finding identity

A durable finding source is not merely `(session, ordinal)`. One session may
execute multiple turns. The identity must include `agent_run_id`, finding
ordinal, and a digest of the validated structured finding. The stored snapshot
retains provider, analyzed base/head OIDs, verdict, anchor state, claim,
evidence, consequence, and suggested comment under existing bounds.

### Composition

A composition snapshots its exact source IDs/digests and target at creation.
It stores immutable candidate revisions with actor and derivation, and has a
small lifecycle: `editing`, `accepted_as_draft`, or `abandoned`.

Core policy must reject:

- missing or duplicate sources;
- sources from another review scope;
- incomplete agent output unless explicitly adopted by the reviewer;
- an invalid or guessed target;
- a source digest mismatch presented as the same revision;
- cycles in composition/derivation lineage;
- direct transition from composition to published GitHub state.

A stale source may still be inspected and composed, but its staleness is
explicit and the eventual mutation remains subject to RFC-BPR-0008 head and
anchor revalidation. No stale acknowledgment retargets the source.

### Draft and publication lineage

An accepted composition gives the local draft an optional durable
`review_composition_id` and exact accepted composition revision. The draft ID
must be durably allocated. If RFC-BPR-0008 has already introduced durable
local drafts when this RFC is imported, extend that authority; otherwise add
the minimal durable draft persistence needed here without creating a second
draft state machine.

A publication-lineage record is created only after a successful mutation and
server reconciliation. It stores the exact draft/composition revision, target,
expected and accepted head, acting identity, logical publication identity,
idempotency key digest, approved prose digest, public wire-body digest, chosen
representation, `ServerCommentId`, and reconciled server comment identity. Failed or
cancelled attempts remain typed mutation attempts and do not masquerade as
publication lineage.

## Architectural allocation

### Functional core

The core owns:

- private-thread, turn, revision, actor, and lifecycle value types;
- semantic subject legality and staleness decisions;
- provenance fact categories and conflict diagnostics;
- immutable revision and acyclic derivation rules;
- finding-source identity and composition admission;
- source-set/target partitioning for Split into Drafts;
- composition and draft transition legality;
- duplicate-publication advisory decisions from bounded observations;
- the invariant that composition cannot publish;
- semantic deltas and explanations.

The core receives compact bounded observations. It contains no Qt, SQLite,
filesystem, process, protocol, clock, thread, network, GitHub provider, or
credential dependency.

### Application host

The host owns:

- durable ID allocation, SQLite transactions, migration, paging, retention,
  purge, crash recovery, and quota enforcement;
- loading bounded thread summaries and turn pages;
- atomic persistence of reviewer prompts before dispatch;
- agent-run creation, requested-setting capture, protocol-reported metadata,
  terminal outcomes, and complete assistant-turn persistence;
- immutable finding snapshots and derivation edges;
- explicit private-source selection and supplement construction;
- dispatch of agent-assisted squash through the one existing invocation host;
- durable draft/composition correlation;
- mutation-receipt and reconciliation correlation;
- request/session/revision/generation identity, cancellation, and stale-result
  rejection;
- redaction and secret-free diagnostics.

The host does not decide authorship labels, silently choose targets, infer model
facts, or publish because an agent completed.

### SQLite adapter

The SQLite adapter implements narrow application ports. At the source baseline,
schema `5` is current and was confirmed during import. Allocate the next live
schema version during implementation after rechecking the tree; this import
reserves no schema number. The separate mutation-recovery database is version
`2` and remains RFC-BPR-0022's migration authority.

The schema must use `STRICT` tables, foreign keys, transactions, prepared
statements, the existing review-scope authority, and monotonic sequence rows.
Conceptual tables are:

- `private_review_thread`;
- `private_review_turn`;
- `private_review_turn_revision`;
- `private_review_derivation`;
- `agent_run_provenance`;
- `agent_finding_snapshot`;
- `review_composition`;
- `review_composition_revision`;
- `review_composition_source`;
- durable draft correlation, if not already provided by RFC-BPR-0008;
- `review_publication_lineage`;
- non-pruned high-water sequence tables.

Exact table decomposition may change during allocation, but it must preserve one
storage authority and every semantic distinction above. A compatibility facade
may keep `store_note` temporarily, but it must write a one-turn private thread;
`local_note` and private threads may not remain competing authorities.

### Agent adapters

Protocol adapters may expose bounded metadata events for model, effort,
tool/harness version, and remote IDs when the protocol genuinely provides
them. Unsupported fields remain absent. Adapters must not scrape prose or
executable names and call that protocol-reported metadata.

The existing internal App Server remote session/thread/turn identities should
be surfaced through typed result metadata where the protocol provides them.
Each invocation receives a new durable `agent_run_id`, even when it continues
an existing `agent_session`.

### Qt presentation adapter

Qt owns:

- private/shared filters or roots in the conversation drawer;
- the local-only rail/badge and thread cards;
- turn transcript, revision-history affordance, provenance chips, and detail
  popover/panel;
- exact source selection;
- composition workspace, target choice, Split into Drafts, profile/effort
  selection, and candidate preview;
- explicit Accept as Draft and separate Publish actions;
- focus, keyboard navigation, accessibility names/descriptions, scaling,
  clipping, and incremental model updates.

Qt emits typed intents and applies incremental projections by stable identity.
It neither reads SQLite nor launches agents nor calls GitHub directly. The
implementation should avoid simply stacking another large panel into the
already crowded drawer; it must establish a coherent private/shared
conversation navigation model.

### Composition root

`src/app` wires the new store ports, run metadata, private-thread host,
composition host, Qt projection, existing invocation host, existing draft
editor, and existing mutation host. A feature reachable only in a unit-test
helper, mock, or alternate executable does not satisfy this RFC.

## Persistence, migration, and limits

### Migration of local notes

Every legacy `local_note` row becomes one private thread with one turn and one
initial revision:

- the review scope is preserved;
- the PR-wide or file/comparison attachment is preserved exactly;
- the text bytes are preserved exactly;
- the old local-note identity is retained as migration provenance or a stable
  mapping;
- actor is `legacy_unknown`, never `reviewer` and never `agent`;
- no model, effort, session, or finding source is invented;
- the new high-water sequences advance past every migrated identity.

Migration is forward-only and atomic. Tests cover every supported prior schema,
including exact v5 data. A newer schema is refused before mutation. After a
successful migration there is one private-content authority, not dual writes to
old and new tables.

### Bounded access

Do not extend `private_ledger_observation` to carry every thread and turn. Add
narrow ports for:

- bounded thread summaries by review scope and filter;
- one thread header;
- cursor-paged turn revisions;
- provenance detail for selected run IDs;
- bounded composition sources;
- publication-lineage lookup by local or server identity.

The initial implementation must define constants and tests no weaker than:

- 4,096 private threads per review scope;
- 65,536 retained turn revisions per review scope;
- 16 KiB per reviewer-authored or composition body;
- 1 MiB per complete agent assistant body, matching the existing total stdout
  bound and never silently truncating it;
- 64 MiB total retained private-thread body bytes per review scope;
- at most 64 turn revisions and 512 KiB of body text in one page;
- at most 256 derivation sources in one composition.

A live implementation may choose tighter values during architecture allocation
when UX evidence supports them. It may not silently weaken the boundedness,
truncate accepted bodies, or convert quota refusal into success.

A complete assistant body may exceed the 512 KiB page budget. Such a page
returns the exact revision identity, total byte count, digest, and an explicit
body continuation rather than silently omitting or truncating the revision.
Bounded body reads use that immutable identity and cursor and return at most
512 KiB each. Completion and composition eligibility refer to the full persisted
body, never a displayed chunk. Architecture allocation must prove progress for
a 1 MiB body and refuse composition inputs exceeding the invocation's total
input budget before dispatch, even if every source individually fits storage.

### Deletion and retention

The reviewer can archive without deleting. Explicit deletion or review-scope
purge may remove private threads, provenance, compositions, and local lineage
according to documented cascade rules. It never deletes or edits GitHub state.
Database deletion still loses only private state and recreates a clean ledger.

The product must not retain source text, prompts, or agent output outside the
private ledger and documented diagnostic retention merely to make provenance
look complete. Evidence fixtures remain synthetic and secret-free.

## Security and privacy

- No private thread or composition source enters an agent context without exact
  explicit selection for that invocation.
- Selecting a thread does not implicitly include all of its history; selected
  revision IDs are carried in the intent and revalidated by the host at
  dispatch.
- No provider, AWS, GitHub, or agent credential is stored in provenance.
- Allowed environment names remain names; values are not logged or persisted.
- Remote run identifiers are bounded opaque strings, not authority tokens.
- Public comment bodies contain reviewer-approved prose and, for the
  RFC-BPR-0022 file representation, its canonical visible code permalink;
  private provenance never enters either representation.
- Safe-target policy applies to every automated mutation test.
- Evidence and diagnostics use IDs/digests or synthetic text, never private PR
  content.

## Failure behavior

- Database refusal leaves the prior private thread/composition state unchanged.
- A reviewer prompt that cannot be committed is not dispatched to an agent.
- An agent result that cannot be correlated to the exact run/session/binding is
  retained only as a typed refused result and cannot create a turn.
- A stale result is visible against its original binding and never retargeted.
- Missing runtime metadata displays as unknown and does not fail an otherwise
  valid run.
- Requested/reported metadata disagreement is visible but does not rewrite
  either fact.
- Composition source disappearance or digest mismatch refuses acceptance and
  reloads the current source state.
- Failure to persist draft/composition correlation prevents publication from
  beginning; publication must not create an untraceable server comment.
- Failure to persist publication lineage after a successful remote mutation is
  recovered by the mutation's idempotency and reconciliation records, not by
  issuing a second comment.

## Acceptance criteria

1. A reviewer can create a PR-level or exact semantic-subject private thread,
   add reviewer turns, restart BetterPullReview, and recover the thread and its
   revision history exactly.
2. Side-chat prompts and complete assistant responses can be retained as turns
   without adding a second agent launch path, and the thread remains bound to
   the exact review revision it was created against.
3. Every turn revision has a typed actor. New reviewer text is `reviewer`, agent
   text names one exact `agent_run_id`, and migrated notes are
   `legacy_unknown`.
4. Editing agent-authored text creates a reviewer-authored successor with an
   immutable `edited_from` edge; the original bytes, actor, and run provenance
   remain inspectable.
5. Agent provenance retains requested, configured, protocol-reported, and
   content-self-reported model/effort facts separately. Missing facts remain
   unknown, disagreements are visible, and no value is inferred from a profile
   or executable name.
6. One agent session can produce multiple uniquely identified runs and finding
   sets without collision; retained provenance remains readable after session
   catalog pruning.
7. Thread subjects and composition targets use exact semantic identities.
   Head/epoch movement produces visible stale or unplaceable state and never a
   nearest-line or silent reanchor.
8. A reviewer can select exact revisions/findings, open a composition workspace,
   inspect sources and provenance, and produce a reviewer-authored, exact-copy,
   or explicitly agent-assisted candidate.
9. `Squash to Draft` creates only local composition/draft state. Tests prove
   zero GitHub mutation, zero resolve operation, and zero review submission at
   this step.
10. Sources remain present and traceable after composition. Multiple exact
    targets require explicit target selection or Split into Drafts; the product
    does not guess.
11. Accepting a candidate records the exact composition revision on a durable
    RFC-BPR-0008 draft and survives restart.
12. Publishing requires a separate explicit gesture and traverses the existing
    target/head/acting-identity/idempotency/safe-target mutation path.
13. A successful publication records local lineage to the returned server
    comment and reconciled server state. The GitHub body contains no hidden or
    mandatory BetterPullReview/AI marker and appears under the authenticated
    reviewer's identity.
14. Legacy local notes migrate byte-for-byte into one-turn private threads with
    unknown authorship, no invented provenance, no ID reuse, and no second
    private-note authority.
15. Private thread pages, provenance details, and composition inputs are bounded
    and cursor-loaded. Quota, oversize, and stale-source cases refuse whole and
    never silently truncate.
16. Private content reaches an agent only when exact source revisions are
    explicitly selected and successfully revalidated at dispatch.
17. Deleting the local ledger or an explicitly selected private thread changes
    no GitHub state; the remaining ledger recovers transactionally after abrupt
    termination.
18. The complete workflow is reachable through the real BetterPullReview
    composition root and is usable by keyboard and assistive technology at
    100%, 125%, and 200% scale without clipping or whole-model resets for a
    bounded turn update.

## Verification matrix

| Acceptance | Direct surface | Required proof |
|---|---|---|
| 1, 3, 4, 7 | Functional core + fake store | Replay create/reply/edit/archive/stale transitions; assert exact actors, immutable predecessors, and no guessed target |
| 2, 5, 6 | Agent invocation host and protocol fixtures | Continue one session across multiple runs; report full, partial, conflicting, and absent metadata; restart and prune sessions while run provenance remains |
| 8-10 | Composition core/host/Qt | Select mixed reviewer/agent/finding sources; exercise all three modes; prove source snapshots and explicit target partitioning |
| 9, 12 | Mutation trace | Capture every host/provider call; Squash to Draft emits none, Publish emits exactly the RFC-BPR-0008 path after revalidation |
| 11 | SQLite + draft editor | Accept candidate, kill/restart process, recover exact body, target, composition revision, and draft state |
| 13 | Designated safe GitHub target or signed recorded fixture plus independent reader | Compare acting identity, public bytes, returned server ID, reconciliation, and local lineage; search body for no injected marker |
| 14 | Forward migration fixtures | Migrate every supported prior schema and exact v5 rows; compare text/attachments byte-for-byte and authorship=`legacy_unknown` |
| 15 | Quota/page/benchmark tests | Cross every count/byte/page limit, prove typed whole-operation refusal, and measure changed-set-only update work |
| 16 | Context-supplement and process-boundary tests | Attempt unchecked, deleted, edited, duplicate, cross-review, and over-limit source selection; inspect child input and secret canaries |
| 17 | Crash/process-kill and database-loss tests | Kill during atomic turn/composition/publication-lineage transactions; delete ledger and independently inspect unchanged GitHub state |
| 18 | Public executable and Qt tests | Navigate, create, compose, accept, and publish through the real window; inspect focus, accessible names, scaling, selection retention, and model statistics |

## Required test families

- Core example/property/replay tests for state machines, cycle rejection,
  ordering, source partitioning, and advisory duplicate detection.
- SQLite schema-contract, v1-through-current migration, WAL recovery,
  concurrent-store, high-water exhaustion, pagination, quota, purge, and
  process-kill tests.
- Agent protocol fixture tests for Codex App Server, Codex JSONL, Claude stream
  JSON, and external-terminal unknown metadata behavior.
- Host tests for dispatch ordering, cancellation, stale completion, duplicate
  completion, metadata conflicts, source revalidation, and persistence failure.
- Qt model/delegate/widget tests for private/shared distinction, transcript
  updates, revision history, provenance details, source selection, composition,
  keyboard, accessibility, scaling, and no full reset on one changed turn.
- End-to-end composition-root tests proving a private conversation survives
  restart, squashing performs no mutation, and the separate publish path records
  the server correlation.
- Security fixtures proving no private source crosses without opt-in and no
  credential/environment value enters storage or evidence.

## Documentation impact

Implementation requires coordinated updates to:

- `docs/architecture/review-ledger.md`;
- `docs/architecture/review-round-trip.md`;
- `docs/architecture/ai-agent-boundary.md`;
- `docs/architecture/domain-and-state-model.md`;
- `docs/architecture/system-architecture.md`;
- product/workbench documentation for private versus shared conversations;
- agent profile/protocol documentation for requested and reported runtime
  metadata;
- schema, migration, backup, purge, quota, crash-recovery, and privacy
  documentation;
- test/evidence guidance for designated mutations and secret-free fixtures;
- operator documentation explaining Squash to Draft, provenance qualifications,
  stale sources, Split into Drafts, publication, and deletion.

The documentation ledger must explicitly record impact or no-impact for GitHub
and GHES authority, acting identity, native review semantics, local cache,
credentials, network isolation, packaging, performance limits, accessibility,
and cross-platform behavior.

## Prohibited designs

- Adding `Author = "AI"` or `Model = "..."` to `local_note_record` and calling
  the feature complete.
- Treating the configured profile or executable as proof of the runtime model or
  effort.
- Rewriting an agent-authored row in place after a reviewer edit.
- Identifying findings only by session and ordinal.
- Persisting every protocol frame as a turn or keeping unbounded transcripts in
  `private_ledger_observation`.
- Loading all threads/turns to render one drawer.
- Dual-writing legacy `local_note` and new thread tables indefinitely.
- Letting session pruning delete provenance referenced by retained turns.
- Direct SQLite access from Qt or direct agent/GitHub effects from a widget.
- A second side-chat launcher or model-specific API client.
- A single gesture that squashes and publishes.
- Deleting or hiding source turns merely because they were promoted.
- Guessing an anchor, line, model, effort, user, or publication correlation.
- Injecting provenance into the GitHub body without reviewer-authored text.
- Declaring completion from unit tests while the feature is absent from the
  composed application.

## Implementation order

1. Import and reconcile this RFC, source audit, provisional cards, live IDs,
   dependencies, architecture allocation, and verification ledger.
2. Implement core identities, subjects, actors, revision/lineage state machines,
   provenance fact categories, and composition rules.
3. Implement the next live SQLite schema, legacy-note migration, durable ID
   allocation, paged ports, quota, crash recovery, and durable draft linkage.
4. Expose typed run metadata through agent request/protocol/result boundaries and
   assign one durable run per invocation.
5. Implement private-thread host workflows and durable side-chat/finding
   ingestion with exact context selection.
6. Implement composition, agent-assisted squash, draft acceptance, and
   publication-lineage correlation through existing hosts.
7. Implement the coherent private/shared Qt conversation and composition UI,
   then wire it through `src/app`.
8. Complete focused, public-path, UX/accessibility, performance, security,
   migration, crash, and designated mutation evidence.
9. Update documentation, simplify, independently review the authority and
   publication boundaries, and close only when dependencies and direct evidence
   permit.
