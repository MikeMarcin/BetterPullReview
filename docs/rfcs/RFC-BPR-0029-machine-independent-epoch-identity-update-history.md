---
id: "RFC-BPR-0029"
title: "Machine-Independent Epoch Identity and Pull Request Update History"
status: "Accepted"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0003","RFC-BPR-0017"]
pack_version: "post-v2"
---

# RFC-BPR-0029: Machine-Independent Epoch Identity and Pull Request Update History

## Status and scope

Accepted on 2026-09-12 under mandate r17. The owner asked for this RFC on 2026-09-12. Asked
whether the epoch work should be an RFC of its own or an amendment recorded
against the closed RFC-BPR-0003, he answered, verbatim:

> It's own rfc

That answer authorized writing this document, not building it. In mandate r16
the owner explicitly **did not accept the first draft as written**. He resolved
its A2 and A3 questions and required three corrections: separate updates that
return to the same head, do not count surviving commits as pushes, and never
present a derived comparison base as the original historical base. The revised
proposal below incorporates those answers. The owner accepted that revised
proposal. No epoch source, schema, read operation or test had changed under
this RFC before acceptance.

The engineering questions that remain are recorded under Open questions. They
require evidence before the acceptance criterion they govern is claimed; they
are not additional owner questions by default.

The substance is the design pass recorded on [[BPR-0743]] (its AC1, at
`f953ce9c`). That card carries the implementation. [[BPR-0646]]'s attended
handle criterion, which is RFC-BPR-0028's AC-02, waits on it.

## Motivating requirement

Rejecting BPR-0646's epoch line in the v008 attended session (decision
`6dc6dc36`, 2026-09-12T04:24:06Z), the owner wrote, verbatim:

> epoch handle doesnt move, only shows a single epoch event thought I'm sure
> there were multiple updates

Asked whether the product should import a pull request's earlier updates or
keep the epoch line a record of what this install had observed, he refused
both and stated the requirement this RFC exists to meet, verbatim:

> An Epoch is an update to the PR, epochs identity should be stable and
> repeatable if I look at the PR on different machines. A purely observed
> epoch is not useful and terribly confusing.

Three requirements follow. RFC-BPR-0003's note of the same day reads
"repeatable" the same way.

1. **An epoch is an update to the pull request**, not an observation of it.
   Whether this install saw an update decides what it can show, never what the
   list is.
2. **An epoch's identity is the same on every machine** that reads the same
   pull request.
3. **The reading is repeatable where evidence permits**: two machines identify
   the same *known occurrence* the same way and present a deterministic order.
   The line declares gaps and ambiguity instead of manufacturing missing
   occurrences or claiming a commit date proves push order.

No GitHub read can recover every ordinary push boundary or every overwritten
head. Equal population is therefore impossible in general (Decision 3).

## Relationship to RFC-BPR-0003

[RFC-BPR-0003](RFC-BPR-0003-git-cache-pr-identity-review-epochs.md) is
Auto-Verified. This RFC does not reopen it, change its status, rewrite its text
or invalidate its evidence. RFC-BPR-0003 carries a pointer here and nothing
more.

As accepted, this RFC **supersedes RFC-BPR-0003 in part**, and only in two
respects:

- **What identifies an epoch.** RFC-BPR-0003's `review_epoch_id` remains a
  catalogue-local storage key. A separate, versioned occurrence identity
  identifies only a PR update supported by sufficient evidence (Decision 1).
- **Historical base and time.** RFC-BPR-0003's observed base and observed time
  remain truthful for live observations. An imported occurrence may have no
  known historical base. Its derived comparison base, when available, is a
  distinct fact, and its event or commit date is distinct from local first-seen
  time (Decisions 4 and 7).

Everything else in RFC-BPR-0003 stands unchanged:
- the app-owned bare caches and the Git policy;
- the retained private refs;
- the ordinal and its density;
- the cause labels;
- the default pane choices and named comparisons;
- history paging;
- the non-goal "Reconstructing a head that BetterPullReview never observed and
  GitHub no longer exposes", which this RFC keeps and on which Decision 3 rests.

An epoch this install observes live retains RFC-BPR-0003's local record,
including its observed base and time. Portable occurrence fields are separate.

This is a document of its own because occurrence identity, historical evidence,
comparison availability and catalogue migration change the model beyond the
closed RFC's local-observation scope.

## Why today's epoch cannot meet it

- **Every part of the id is local.** `core::review_epoch_id` is
  `{Authority, Incarnation, Ordinal}`
  (`src/core/include/bpr/core/epoch_model.hpp:444-451`), and all three parts
  are local to one catalogue:
  - `Authority` is a `pull_request_authority_scope_id`. That is a random 256-bit
    value the catalogue allocates for a cache partition of profile, profile
    revision, host, account, repository and object format
    ([git and epoch model](../architecture/git-and-epoch-model.md), "Cache and
    epoch scope").
  - `Incarnation` is 32 random bytes minted once per catalogue file
    (`src/adapters/sqlite/src/catalog_database.cpp:1705-1707`).
  - `Ordinal` is handed out by the SQLite scope row under compare-and-swap, in
    observation order (`src/adapters/sqlite/src/epoch_catalog.cpp:3029-3046`,
    `:3145-3162`).
- **Imports are reserved but never produced.** The model reserves an
  `explicit_historical_import` origin and cause, and the catalogue validates
  and persists them. Nothing outside a test produces one
  (`tests/application/epoch_application_host_tests.cpp:3636-3650`). The one
  read sets `pull_request_revision_origin::live`
  (`src/application/src/github_window_read_host.cpp:480`).
- **Nothing asks for the timeline.** No code in the tree asks GitHub for a pull
  request's timeline.

A producer placed behind the existing model would therefore give a longer list,
and its identities would still differ between machines. Because the account is
part of the partition, they would also differ between a signed-in and a
signed-out read on the same machine.

## What is established

These facts come from the design pass on [[BPR-0743]]. Its source anchors were
re-read at `f2c49e12`. `git diff --stat f2c49e12 0f2ede86 -- src` is empty, so
they still hold at the revision this RFC was written against.

### The timeline is readable signed out

The probe was run on 2026-09-12 with no credential:

    GET https://api.github.com/repos/NVIDIA/stdexec/issues/1846/timeline?per_page=100

It returned **HTTP 200**, with `x-ratelimit-limit: 60` and
`x-ratelimit-resource: core`, which is the unauthenticated budget. The response
held 18 items, two of them `head_ref_force_pushed`.

Each force-push event carries a GitHub-side `id` and `node_id`, a `created_at`,
and, in `commit_id`, the head the branch was moved **to**:
- `6ffbd0ac…` at 2026-08-01T16:45:08Z;
- `51d2dad0…` at 2026-08-09T22:19:26Z, which is the pull request's current
  `head.sha`.

`6ffbd0ac…` appears nowhere in `GET /pulls/1846/commits`, which lists only the
surviving lineage, yet it can still be fetched anonymously. Commits therefore
cannot reconstruct the updates a force push overwrote. Only the timeline names
them.

### Three limits of that response

1. **A force-push event has no before-commit.** Its complete key set is `id`,
   `node_id`, `url`, `event`, `commit_id`, `commit_url`, `created_at` and
   `performed_via_github_app`. GraphQL's `HeadRefForcePushedEvent` does have
   `beforeCommit`, but GraphQL is refused anonymously
   (`src/adapters/github/src/github_anonymous_read_admission.hpp:83-110`).
2. **Array position is not an ordering key, and `committed` dates are not push
   times.** The items were not in time order, and the timeline's `committed`
   entries carry author dates.
3. **An ordinary push is not an event.** The three `committed` entries are the
   surviving lineage. Nothing distinguishes three commits pushed at once from
   three separate pushes.

### What the probe did not cover

It covered one pull request, one page of timeline, and no base-branch events.
That is why open questions 2 and 3 are open.

### The pull request's provider identity is already in hand

- **Repository and pull request ids come from GitHub.** `core::repository_id`
  and `core::pull_request_id` are normalized from GitHub's numeric `id` fields
  (`src/adapters/github/src/github_paged_read.cpp:2273`, `:2326`, `:2388`),
  which are the same for every reader of the same host.
- **The catalogue stores some of it.** The `pull_request_scope` table stores an
  8-byte `pull_request_id` and the `pull_request_number` (`catalog_database.cpp`,
  the `pull_request_scope` schema object).
- **The host id is not provider data.** `core::git_host_id` is not read from the
  provider in the paths examined: the designated bootstraps write
  `git_host_id{ 1U }`
  (`src/application/src/designated_review_authority_bootstrap.cpp:175`,
  `src/adapters/github/src/designated_live_public_observer.cpp:275`).

## Accepted decisions

These are the accepted design decisions. Decision 1
corrects the design pass in one place, and says so.

### Decision 1: identify evidenced update occurrences, not only heads

> A versioned occurrence identity is a digest of canonical host name, GitHub's
> numeric pull request id, and a typed occurrence discriminator. A force-push
> event uses its numeric REST `id` (and after-head OID as a consistency check).
> An ordinary update seen as a live head transition uses the head OID in a
> distinct `observed-head` namespace. A bare surviving commit is never an
> update occurrence. `review_epoch_id` remains the local storage key.

The first draft's head-only digest collapses A→B→A. The owner answered that
these are **two** update occurrences sharing A's head. When both returns have
distinct GitHub force-push event ids, the event keys distinguish them. The
first observed A and a later force-push event returning to A also have
different typed keys. If the event or live transition needed to distinguish
two occurrences is absent, the reader reports an **ambiguous/incomplete
history**; it does not mint a second occurrence from a commit row, timestamp
or local observation count. A head snapshot with no proven transition is
provisional until the timeline and live history can reconcile it to one
occurrence. Two readers can claim equal portable identity only for the same
evidenced occurrence. This is the unavoidable limit of the REST evidence, not
a claim that two reader populations are equal.

The host term is the validated canonical hostname from the accepted handoff,
not `git_host_id` or the locally partitioned host-authority fingerprint.
GitHub's numeric pull request id is already normalized into `pull_request_id`
(open question 7 records the migration limit). The identity includes the Git
object format with a head discriminator. It excludes the local authority
scope, incarnation, ordinal, profile, account, viewer, access mode and local
first-seen time. It also excludes the base OID, which can change while the
head remains the same.

The force-push `node_id` is not used: it is an opaque provider token and is
absent for ordinary updates. The numeric event `id` is normalized by the
GitHub adapter and stored only as a fixed-width value or digest, consistent
with the catalogue's no-opaque-token rule (`catalog_database.cpp:86-88`).

### Decision 2: the ordinal is not redefined

> `Ordinal` stays the dense local ordering key, and
> `{Authority, Incarnation, Ordinal}` stays the local storage key.

- **Redefining it cannot make the triple portable.** `Authority` and
  `Incarnation` would still be in it. Portability comes from adding an
  identity.
- **Two of its serializations are beyond the catalogue's reach.**
  - Retained refs on disk, as
    `refs/betterpullreview/v1/scope/<64 hex>/epoch/<16 hex ordinal>/{base|head}`
    (`src/adapters/git/src/installed_git_process.cpp:1250-1259`, `:1296-1298`).
  - Bindings already handed out: the MCP revision binding
    (`src/adapters/mcp/src/mcp_context_helper.cpp:731-741`) and the agent
    launch context's `left_epoch` and `right_epoch`
    (`src/application/src/agent_launch.cpp:1597-1604`).
- **The layers above rely on density, and only the adapter guarantees it.**
  - The catalogue's compare-and-swap refuses any proposal other than
    `next_ordinal` (`epoch_catalog.cpp:3029-3046`).
  - Core requires only a strict increase
    (`src/core/src/epoch_transition.cpp:2519-2537`).
  - History paging and alias resolution both key on the ordinal
    (`epoch_model.hpp:425-431`, `epoch_transition.cpp:996-1007`).

The amber last-review tick matches by head OID, not by ordinal, so it is
unaffected (`src/adapters/qt/src/epoch_line.cpp:131-135`, `:163-174`).

### Decision 3: show known updates and declare missing boundaries

Suppose machine A opens the pull request and observes head X, which arrived by
an ordinary push. The author then force-pushes to Y. Machine B opens the pull
request afterwards. B's timeline says the branch was force-pushed to Y and names
X nowhere:
- the event has no before-commit;
- the commits list holds only the surviving lineage;
- GraphQL is refused signed out.

No read B can make recovers X.

So A legitimately holds an update that B cannot derive. The same limit applies
to ordinary pushes that the two readers did not observe: GitHub's surviving
commit list does not say whether several commits arrived in one push or in
several. One `committed` item is **not** one epoch. Import only force-push
events whose after-head is given, plus an actual live head transition; use the
current head snapshot as a provisional latest occurrence when necessary. Do
not synthesize boundaries between surviving commits. The statement the
product can make is:

> The same evidenced occurrence has the same identity on every machine.
> Population is best-effort, and the epoch line says when history is incomplete
> or an occurrence cannot be uniquely matched.

History is incomplete or ambiguous when:
- the timeline read failed, was refused, or was cut off by the page cap (open
  question 3);
- a force push overwrote updates this install did not see;
- ordinary push boundaries were not observed or a current head cannot be
  uniquely associated with a force-push event.

How the line says so is RFC-BPR-0028's presentation and part 2 of
[[BPR-0743]]. This RFC requires only that the fact be representable.

### Decision 4: distinguish historical base from comparison base

> A live epoch keeps its observed base. A historical update whose original base
> is not evidenced records that base as **unknown**. If a comparison can be
> built, its computed merge base is separately named `derived comparison
> base`; it is never shown or persisted as the original historical base. An
> update with no usable comparison base remains visible with comparison
> unavailable.

Today's live-epoch requirement is origin-blind, so implementation must give a
historical occurrence an explicit optional-anchor representation rather than
stuffing a guessed value into the existing base field:
- core requires `BaseBranch`, `Base` and `Head` to be valid for any accepted
  revision (`epoch_transition.cpp:2199-2218`);
- `plan_epoch_mutation` rejects a revision that fails this (`:2470-2481`);
- `valid_summary` repeats the check (`:2284-2300`);
- the schema makes `epoch_anchor.object_digest` `NOT NULL` and non-zero.

The timeline supplies no base for a historical update. The options are:

- **Rejected: the base ref's OID at import time.** Two machines importing on
  different days would record different bases for the same update, and the
  epoch would claim a base that was not its base.
- **Selected: optional historical anchor.** An occurrence can be named and
  shown from its event and head even when the historical base is unknown. The
  core and storage distinguish an update occurrence from a comparison-ready
  epoch; the existing live epoch rows and their invariants remain intact.
- **Derived comparison candidate:** `merge-base(head, selected base ref)` may
  support a diff through the app-owned cache. It is versioned with the base-ref
  selection and can change after a base rewrite, so no portability or
  historical-truth claim is made for it. Validation remains open question 4.

An imported occurrence's historical base-branch identity is likewise unknown
unless the timeline proves it (open question 2). The interface labels the
comparison base and its unavailable state; it never quietly substitutes the
current base ref as the historical one.

### Decision 5: lossless migration may leave identity unresolved

> Add occurrence evidence and an optional portable identity beside the current
> epoch rows. Preserve every existing row, local key, incarnation and ordinal.
> Backfill only when host and occurrence evidence suffice; mark other legacy
> rows unresolved until a later read can associate them uniquely. Never assign
> one portable key to two distinct local rows merely because heads match.

`migrate_schema` (`catalog_database.cpp:1018-1062`) rebuilds the versions it
knows. It carries the incarnation forward verbatim (`:1045`, `:822-823`) and
refuses any other version (`:754-756`, `:1021-1023`). The two alternatives
would orphan data:
- **Re-minting the incarnation** would orphan every `window_review_restoration`
  row, because those rows store it
  (`src/adapters/sqlite/src/window_review_restoration_store.cpp:99-109`).
- **Renumbering** would orphan all of that, plus the retained refs, the
  `review_ledger` spans
  (`src/adapters/sqlite/src/review_ledger_database.cpp:243-244`), and the MCP
  and agent-launch bindings already emitted.

The `epoch` and `epoch_anchor` tables store the head and local observation,
while `pull_request_scope` stores GitHub's numeric PR id. They store neither
the canonical hostname nor a GitHub event id. `repository_scope.host_authority`
is a locally partitioned fingerprint, not a portable hostname. Thus a blanket
offline head-only backfill in the first draft is **not lossless identity
backfill**. A live accepted handoff supplies the validated hostname; a
subsequent timeline read may associate an old row with an event only when the
association is unique. Ambiguous rows stay intact and explicitly unresolved.

Where a historical occurrence is comparison-ready and represented by an
imported epoch, it may consume a fresh local ordinal but advances neither
`latest_live_ordinal` nor `previous_distinct_head_ordinal`
(`epoch_catalog.cpp:4564-4568`). A head-only occurrence may require a separate
record until it can be compared. The local ordinal never claims chronological
order.

Because the migration refuses unknown versions by design, a build older than
the new schema version refuses a migrated catalogue. The migration must be
tested on a copy of a current-version file with duplicate-head history and
unresolved GHES host provenance, in addition to the current reference checks.

### Decision 6: one new anonymous REST read

> The update history is read from
> `GET /repos/{owner}/{repo}/issues/{number}/timeline` as one new REST read
> operation. It is admitted anonymously, paged like `list_pull_request_commits`,
> and bounded by a page cap.

Following `list_review_comments`, it joins `all_read_operations` and
`anonymous_read_operations` but not `read_operation_catalog`
(`src/adapters/github/src/github_read_contract.hpp:57-64`). No recorded
manifest or fixture regeneration is therefore expected.

The read is not free. Opening one pull request signed out already costs several
requests against the limit of 60 per hour
([RFC-BPR-0017](RFC-BPR-0017-unauthenticated-public-review-read.md)). An open
that cannot make the read still produces today's live epoch.

### Decision 7: labeled dates and deterministic display order

> Use GitHub's `created_at` as the event time of a force-push occurrence. Where
> no GitHub event time exists, use the head commit's **committer timestamp**
> consistently and label it `commit date`, never `push time`. Preserve the
> local first-seen time separately. Display order uses the selected UTC time,
> then a stable typed occurrence key as a tie-breaker; the tie-breaker is not
> evidence of push order.

This adopts the owner's A3 answer. Commit author dates in `committed` timeline
items are not push times; the commit object has both author and committer
timestamps, and this proposal selects the committer timestamp for every
no-event occurrence. An unavailable commit timestamp leaves an undated item
in a deterministic key order and marks history incomplete. The line labels
the source of each date. Equal timestamps, and even unequal mixed event/commit
timestamps, do not prove the relative order of two pushes. Local ordinal and
first-seen remain catalogue facts, not portable chronology.

## Open questions

The owner resolved the first draft's A2 and A3. The items below are engineering
questions or evidence tasks; each needs a recorded result before its governed
acceptance claim. An unexpected policy choice returns as one concrete ask,
without changing the accepted decisions silently.

### 1. Repeated-head reconciliation

Resolved rule: A→B→A is two occurrences with one repeated head. A distinct
force-push event id is sufficient evidence of a second occurrence; a surviving
commit or equal head alone is not. The engineering task is to reconcile a live
snapshot with a timeline event only when the association is unique, and to
retain ambiguity otherwise. A recorded A→B→A fixture must exercise both the
known and the missing-event cases.

### 2. Base-branch events have not been observed

The probed pull request had no `base_ref_changed` or `base_ref_force_pushed`
item. Three things about them are not known: their REST shape, whether they
carry an OID or only ref names, and whether this route returns them
anonymously. A base-branch change also bears on an imported epoch's base-branch
identity, which the schema requires.

This is settled by the same anonymous probe, run against two public pull
requests: one whose base branch was changed, and one whose base branch was
force-pushed.

### 3. Paging a large timeline

The probed timeline was 18 items on one page. A long-lived pull request's
timeline runs to several pages, and every page spends the unauthenticated
budget. These are not known:
- the page cap;
- which pages are read first when the cap binds;
- whether refresh re-reads the whole timeline or only its newest page;
- whether a conditional request lowers the cost.

A cap that binds makes the history partial (Decision 3). This is settled by
measurement on a long-lived public pull request and a stated cap. If the cap
would hide updates a reviewer expects to see, the question comes back to the
owner.

### 4. Comparison-base validation

Merge-base(head, selected base ref) is Decision 4's **derived comparison**
candidate, never the original historical base. It is unvalidated.
These are not established:
- when it permits a useful diff and when it should be unavailable;
- what it yields after the base branch is rewritten;
- how deep the cache must fetch the base branch to compute it;
- what it means for a criss-cross history with more than one merge base.

This is settled against live-observed epochs and a base-force-push fixture,
including a case with no computable base. A mismatch is labeled as a
comparison difference and never rewritten as the historical base.

### 5. Date source and order proof

Resolved policy is Decision 7. Engineering must verify that the chosen
committer timestamp is available from the cache for the head; normalize it to
UTC; and test deterministic tie-breaking for equal timestamps, two force-push
events with the same head, and conflicting event/commit date order. The UI must
never call this tie-break a known push sequence. `observed_at` remains local
first-seen and is not overwritten with a GitHub or commit time.

### 6. The read's site list is unbuilt

The design pass enumerated the sites the new operation touches. None is built:
- the enum (`github_read_contract.hpp:22`);
- `all_read_operations` (`:65`) and `anonymous_read_operations` (`:85`);
- the exhaustive switches in `github_read_contract.cpp` (`:715-734`,
  `:736-758`, `:1760-1789`, `:1792-1821`);
- the REST route table (`:1924-1985`);
- the anonymous admission switch (`github_anonymous_read_admission.hpp:56-74`);
- the normalizer (`github_read_normalizer.cpp:852-866`, `:967-980`);
- the paged read (`github_paged_read.cpp:1324-1339`, `:1461-1475`), with
  `read_rest_array_collection` at `:2744-2791` and `rest_has_next_page` at
  `:1609-1660`;
- five test switches. A search for `list_pull_request_commits` at this RFC's
  revision puts them at:
  - `tests/github_contract/github_read_contract_tests.cpp:611`
  - `tests/github_contract/github_anonymous_read_contract_tests.cpp:416`
  - `tests/github_contract/github_paged_read_tests.cpp:1385`
  - `tests/github_contract/gh_cli_transport_tests.cpp:314`
  - `tests/github_contract/native_https_anonymous_read_tests.cpp:198`

The same search finds a site the list omits:
`src/app/src/large_review_fixture.hpp:305` scripts a `list_pull_request_commits`
exchange for the large-review application fixture. A new read on the open path
is therefore likely to need a scripted exchange there too.

The list is a search result, not a proof. The switches are exhaustive with no
default, so the build is what shows the list complete.

### 7. Host term and backfill limit

Source inspection at d25 settles the new-read term and the offline limit.
`accepted_pull_request_handoff.Hostname` is a validated lowercase DNS-style
canonical hostname (`github_window_read_host.cpp:459`,
`github_host_authority.hpp:50`); the catalogue stores no hostname.
`pull_request_scope.pull_request_id` is the provider's numeric REST `id`
(`github_paged_read.cpp:2390`, `epoch_catalog.cpp:1123`). Existing `epoch`
rows have only local `source_observation`, local time, and base/head anchors;
they have no event occurrence id (`catalog_database.cpp:186-248`).

Therefore an offline migration cannot assign all rows a portable identity.
Whether an old GHES scope can be mapped safely through a separate durable
profile record is unproved; aliases for one GHES installation remain a
cross-host naming policy question. Until proved, those rows stay unresolved.
No reader-specific fingerprint may be silently used as a substitute.

## Owner answers and remaining ask

**A1, 2026-09-12:** The owner supports portable identity, partial-history
reporting and lossless migration, but **does not accept the first draft as
written**. He requires separate updates reaching the same head, known PR
updates rather than surviving commits counted as pushes, and an honest
separation of derived comparison base from original historical base.

**A2:** A→B→A is two occurrences sharing a head. Never invent an occurrence
without GitHub evidence; report incomplete history. Decisions 1 and 3 reflect
this answer.

**A3:** Use GitHub event time where available and a consistently selected
commit timestamp otherwise. Label commit dates as commit dates, not push
times; keep local first-seen separate. Deterministic order is separate from
the timestamp label, and equal timestamps do not establish push order.
Decision 7 selects committer time and a stable tie-break.

**Acceptance question on the revised proposal (answered):**

> Do you accept the revised RFC-BPR-0029 as the plan for the epoch work: only
> evidenced PR update occurrences get portable identities, missing ordinary
> push boundaries are reported as incomplete, unknown historical bases remain
> unknown while computed comparison bases are labeled separately, and existing
> catalogue rows migrate without loss even when some identities cannot yet be
> resolved?

**Owner answer, mandate r17, 2026-09-12: YES.** The owner accepted this revised
RFC as the plan for epoch work, with identities only for evidenced update
occurrences, incomplete ordinary-push history reported, unknown historical
bases kept distinct from derived comparison bases, and lossless migration
allowed to leave portable identity unresolved. BPR-0743 can proceed under
these constraints. Acceptance authorizes allocating its closeout card; it is
not implementation or verification evidence.

## Architectural allocation

- **Core** owns the typed occurrence identity, evidence/ambiguity state,
  historical-base versus derived-comparison-base distinction, deterministic
  ordering key and partial-history fact. It has no clock, network or Git.
- **Host** owns the timeline read and its cap, the merge-base computation
  through the app-owned cache, event/live reconciliation, and time as explicit
  input.
- **GitHub adapter** owns the new anonymous REST operation, its admission,
  paging and normalized numeric event id.
- **SQLite adapter** owns the schema version, optional occurrence identity,
  provenance and lossless migration/backfill.
- **Qt adapter** presents update occurrences, comparison availability, labeled
  time source and incomplete-history fact, and derives nothing.

## Prohibited designs

- Any locally minted or reader-specific value in the portable identity: the
  authority scope, the incarnation, the ordinal, `git_host_id`, the profile,
  the account, the viewer or the access mode.
- The timeline event's opaque `node_id` as the identity, or a head-only digest
  assigned to two known occurrences sharing a head.
- One epoch per surviving commit, or inferred ordinary push boundaries.
- The base ref's OID at import time or a computed merge base recorded or shown
  as the original historical base.
- GitHub event or commit time written into local `observed_at`/first-seen.
- Re-minting the incarnation or renumbering ordinals in order to migrate.
- A GraphQL route for the history, which is refused signed out.
- An epoch line that presents a partial history as complete.
- Evidence from a fixture published straight to the widget, with no host, read
  or catalogue, such as
  `tests/adapters_qt/epoch_comparison_control_tests.cpp:811-860`.

## Non-goals

- Reconstructing a head that no reader observed and that GitHub no longer
  exposes. This is RFC-BPR-0003's non-goal, kept.
- Changing the ordinal, its density, the retained-ref encoding, the named
  comparisons or history paging.
- Sharing catalogues, review ledgers or review state between machines. Identity
  is portable; storage stays local.
- The epoch line's presentation, beyond requiring that partial history be
  representable. RFC-BPR-0028 owns the presentation.

## Acceptance criteria

- Two independent catalogues reading the same force-push history through the
  host assign equal identities and deterministic order to the **same evidenced
  occurrences**. A→B→A yields two distinct identities for A. A missing event
  never yields a fabricated second A, and the history declares the gap.
- An ordinary live transition yields one occurrence per observed transition,
  not one per surviving commit. Its portable identity agrees across signed-in,
  signed-out and recreated catalogues when the same occurrence is unambiguous;
  ambiguous snapshots remain marked unresolved.
- A live read that finds a new head for the same pull request still appends an
  author-upload epoch, and an imported epoch advances neither the latest live
  ordinal nor the previous distinct head ordinal.
- An update observed live and later overwritten by a force push stays on the
  install that observed it. The epoch line says when the history it holds may
  be partial: when the timeline read failed, was refused or was cut off by the
  page cap, or when a force push overwrote updates this install did not see.
- Live epochs retain their observed base. An imported occurrence with no
  evidenced historical base shows `unknown`; a computed comparison base is
  separately labeled and comparison can be unavailable. No imported row
  claims the current base ref or merge base was its original historical base.
- Event dates are labeled GitHub event times, other dates commit dates, and
  first-seen remains local. Equal timestamps have a deterministic tie-break
  without a false push-order claim.
- A current-version catalogue migrates without changing any old epoch row's
  identity key, incarnation or ordinal. Unresolvable host or duplicate-head
  provenance remains explicitly unresolved; review-ledger,
  window-restoration, retained-ref, MCP and agent-launch references to old
  epochs still resolve.
- The history read is anonymous REST and stays within a stated page cap, and an
  open that cannot make the read still produces today's live epoch.
- BPR-0646's attended handle criterion is asked again, unchanged, on a signed
  bundle carrying this work.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Known occurrence is the same on two machines | Epoch host over two fresh SQLite catalogues | Recorded A→B→A force-push timeline read into both; two A occurrences remain distinct and corresponding identities/order agree |
| No invented ordinary push | GitHub adapter plus epoch host | Several surviving commits delivered in one push versus several observed head transitions; only evidenced transitions become occurrences and missing boundaries are declared |
| Identity excludes local values | Epoch host | The same unambiguous update read signed in, signed out, and after the catalogue is recreated; identities compared; ambiguous legacy snapshot stays unresolved |
| Live append is unchanged | Epoch host | A second live handoff with a new head; cause and live ordinals inspected |
| Partial history is declared | Composed application | Timeline read failed, refused and capped, and a force push over an unseen update; the line's state inspected |
| Historical base is not fabricated | Core plus app-owned cache and composed presentation | Event with no historical base stays unknown; optional merge base is labeled derived; rewritten or missing base makes comparison change or unavailable, not historical truth |
| Date and order labels are honest | Epoch host plus composed presentation | Event time, committer time, local first-seen, equal timestamps and conflicting mixed dates are separately asserted; stable tie-break never claims push order |
| Migration loses nothing | SQLite catalogue, on copies of current-version files | Legacy duplicate-head and GHES-without-host rows remain unresolved; ledger, restoration, ref and binding references resolve; incarnation/ordinals unchanged |
| The read is bounded | Anonymous transport | Requests per open counted against recorded pages; the cap stops the read |
| The owner can judge it | Signed bundle | BPR-0646's handle criterion asked in an attended session |

## Documentation impact

These documents change when this RFC is implemented, not before it is accepted:
- [git and epoch model](../architecture/git-and-epoch-model.md): "Exact identity
  model", "Cache and epoch scope", and "Epoch facts and cause policy", which
  says the ordinal defines ordering;
- the [product contract](../architecture/product-contract.md): its private
  retained revision history row, which speaks of objects "BetterPullReview
  observed";
- the [domain and state model](../architecture/domain-and-state-model.md);
- the [security threat model](../architecture/security-threat-model.md), for the
  new anonymous read and its budget;
- the [RFC-BPR-0003 verification plan](../verification/RFC-BPR-0003.md), which
  gains a pointer here.

## Cards

- [[BPR-0743]] carries the implementation. Acceptance of this revised
  proposal removes its prior owner-ask blocker. A2 and A3 are answered. Its AC1 is the earlier design pass,
  retained as historical evidence rather than silently rewritten.
- [[BPR-0646]] (RFC-BPR-0028) depends on BPR-0743 for its attended handle
  criterion.
- [[BPR-0750]] is the closeout card, allocated after acceptance with the
  immutable RFC base `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
  It remains open until every criterion is directly verified and canonical
  closeout prerequisites permit it.
