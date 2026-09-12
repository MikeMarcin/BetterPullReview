---
id: "RFC-BPR-0029"
title: "Machine-Independent Epoch Identity and Pull Request Update History"
status: "Proposed"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0003","RFC-BPR-0017"]
pack_version: "post-v2"
---

# RFC-BPR-0029: Machine-Independent Epoch Identity and Pull Request Update History

## Status and scope

Proposed, and not accepted. The owner asked for this RFC on 2026-09-12. Asked
whether the epoch work should be an RFC of its own or an amendment recorded
against the closed RFC-BPR-0003, he answered, verbatim:

> It's own rfc

That answer authorizes one thing: writing this document. It does not accept
the decisions below and it is not approval to build. Nothing here is
implemented, and no source, schema, read operation or test changed with it.

Nothing is built until the owner has answered the three asks under Owner asks
at the end of this document. The first is whether he accepts it. The other two
decide what identifies an epoch and what its date is, and those are the first
things any implementation writes.

The engineering questions that remain are recorded under Open questions, as
open. Each is answered in this document, with its evidence, before the
acceptance criterion it governs is claimed.

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
3. **The reading is repeatable**: two machines reading the same pull request
   hold the same epochs, with the same identities, in the same order.

The first two are reachable. The third is reachable for identity, is not
reachable in full for population (Decision 3), and has no design yet for order
(open question 5).

## Relationship to RFC-BPR-0003

[RFC-BPR-0003](RFC-BPR-0003-git-cache-pr-identity-review-epochs.md) is
Auto-Verified. This RFC does not reopen it, change its status, rewrite its text
or invalidate its evidence. RFC-BPR-0003 carries a pointer here and nothing
more.

While this RFC is Proposed, nothing in RFC-BPR-0003 is superseded. Once
accepted, this RFC **supersedes RFC-BPR-0003 in part**, and only in two
respects:

- **What identifies an epoch.** RFC-BPR-0003's core "stores stable epoch
  identities", and in the shipped model that identity is `review_epoch_id`.
  Under this RFC, `review_epoch_id` is the catalogue's local storage key. The
  identity the product may claim is the same on two machines is the derived
  identity of Decision 1, carried beside it.
- **What base OID and observed time mean for an imported epoch.** RFC-BPR-0003's
  rule "A review epoch records base OID, head OID, observed time, ordinal,
  cause, and material summary" was written for observations. For an epoch
  imported from the pull request's history, this RFC decides what the base OID
  is (Decision 4, open question 4) and what its time is (open question 5).

Everything else in RFC-BPR-0003 stands unchanged:
- the app-owned bare caches and the Git policy;
- the retained private refs;
- the ordinal and its density;
- the cause labels;
- the default pane choices and named comparisons;
- history paging;
- the non-goal "Reconstructing a head that BetterPullReview never observed and
  GitHub no longer exposes", which this RFC keeps and on which Decision 3 rests.

An epoch this install observes live keeps exactly the meaning RFC-BPR-0003
gives it.

This is a document of its own, not an amendment under the closed RFC, because
of what the work touches. RFC-BPR-0003's epoch sentence pins six things, and
this work moves four of them: identity, base OID, observed time, and the
catalogue schema version that stores them.

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

## Proposed decisions

These are the design pass's decisions, proposed and not yet accepted. Decision 1
corrects the design pass in one place, and says so.

### Decision 1: the identity is a digest of the pull request and the update's head

> The portable epoch identity is a versioned digest over the pull request's
> provider identity and the update's head commit OID, with its object format.
> It is carried beside `review_epoch_id`, not inside it.

**Correction.** The design pass wrote "a digest over the pull request's
authority scope and the update's head OID". Read literally, the authority scope
is `pull_request_authority_scope_id`, which is random and partitioned by
account, so a digest over it would differ between machines. The inputs must be
the provider's identity of the pull request: a host term (open question 7) and
GitHub's numeric pull request id.

Every locally minted or reader-specific value is excluded:
- the authority scope;
- the incarnation;
- the ordinal;
- `git_host_id`;
- the profile, the account and the viewer;
- the access mode;
- the observed time.

**Not the timeline event's `node_id`.** It exists only for force pushes, so the
current head usually has none. It is also exactly the kind of opaque provider
token the catalogue excludes by design (`catalog_database.cpp:86-88`).

**Not the base/head pair.** The base OID changes over time even for a live
read. Two installs reading an hour apart, with the base branch advancing in
between, record different bases for the same head. Putting the base into the
identity would bring back the machine-locality the owner asked to remove.

Under this rule, a force push back to an earlier head gives two updates one
identity. That is open question 1 and owner ask A2.

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

### Decision 3: identity can be stable; population cannot be made equal

Suppose machine A opens the pull request and observes head X, which arrived by
an ordinary push. The author then force-pushes to Y. Machine B opens the pull
request afterwards. B's timeline says the branch was force-pushed to Y and names
X nowhere:
- the event has no before-commit;
- the commits list holds only the surviving lineage;
- GraphQL is refused signed out.

No read B can make recovers X.

So A legitimately holds an epoch that B cannot derive. With a derived identity,
the two agree on every update both can see, and A holds one more. The statement
the product can make is:

> Identity is the same on every machine. Population is best-effort, and the
> epoch line says when the history it holds may be partial.

History may be partial in two situations:
- the timeline read failed, was refused, or was cut off by the page cap (open
  question 3);
- a force push overwrote updates this install did not see.

How the line says so is RFC-BPR-0028's presentation and part 2 of
[[BPR-0743]]. This RFC requires only that the fact be representable.

### Decision 4: every epoch keeps a base OID

> A base OID stays required for every epoch, live or imported. A head-only epoch
> is not proposed.

Today the requirement is the same whatever an epoch's origin:
- core requires `BaseBranch`, `Base` and `Head` to be valid for any accepted
  revision (`epoch_transition.cpp:2199-2218`);
- `plan_epoch_mutation` rejects a revision that fails this (`:2470-2481`);
- `valid_summary` repeats the check (`:2284-2300`);
- the schema makes `epoch_anchor.object_digest` `NOT NULL` and non-zero.

The timeline supplies no base for a historical update, which leaves three
options:

- **Rejected: the base ref's OID at import time.** Two machines importing on
  different days would record different bases for the same update, and the
  epoch would claim a base that was not its base.
- **Candidate only: merge-base(head, base ref).** It is deterministic for the
  same commit graph and can be computed through the existing cache. But it turns
  the base from an observed value into a derived one, it moves if the base
  branch is rewritten, and it is unvalidated (open question 4).
- **Not proposed: a head-only historical epoch.** It would change core
  validation, `valid_summary` and the schema, and so change what an epoch is.
  It comes back to the owner only if the candidate fails validation.

An imported epoch's base-branch identity has the same gap when the base branch
was changed, because the `epoch` table also requires it (open question 2).

### Decision 5: migrate by adding the identity, not by rebuilding

> Add the portable identity as a new column on `epoch`, backfill it for existing
> rows, and let imports append with fresh ordinals. Nothing is re-minted and
> nothing is renumbered.

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

Appending works because the catalogue already separates the newest live epoch
from the highest ordinal. An imported epoch consumes an ordinal but advances
neither `latest_live_ordinal` nor `previous_distinct_head_ordinal`
(`epoch_catalog.cpp:4564-4568`). The price is that ordinal order stops being
chronological, which is why order needs its own design (open question 5).

Because the migration refuses unknown versions by design, a build older than
the new schema version refuses a migrated catalogue. What backfill can compute
from the catalogue alone is open question 7.

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

## Open questions

Each question below is open. Each is answered in this document, with its
evidence, before the criterion it governs is claimed. Questions 1 and 5 need the
owner and are asked under Owner asks. The rest are engineering questions, to be
settled by probe, measurement or build.

### 1. Head collision

If a pull request is force-pushed back to a head it had before, two updates
share a head OID, and Decision 1 gives them one identity. Whether that is one
epoch or two depends on what the owner means by "an update to the PR". The code
cannot settle it (owner ask A2).

If the answer is two, the identity needs a second term that is also the same on
every machine. A force-push event's `created_at` is such a term, but the earlier
occurrence may have arrived by an ordinary push, which has no event (limit 3).
The nearest existing constraint is the unique index
`epoch_source_revision(incarnation, source_observation)`
(`catalog_database.cpp:224-228`).

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

### 4. Merge-base validation

Merge-base(head, base ref) is Decision 4's candidate, and it is unvalidated.
These are not established:
- that it equals the base this install observed live for the same head;
- what it yields after the base branch is rewritten;
- how deep the cache must fetch the base branch to compute it;
- what it means for a criss-cross history with more than one merge base.

This is settled by comparing the candidate against live-observed epochs,
including one across a base force push. If it fails, Decision 4 comes back for
a new decision rather than defaulting to a head-only epoch.

### 5. The date, and with it the order

- **Force push.** GitHub's `created_at` is the same on every machine, and this
  install's observed time is not.
- **Ordinary push.** The probed route records no push time at all (limit 3).
  The only dates every machine sees are the ones written into the commit
  (limit 2).
- **Order.** The architecture treats observed time as display and diagnostics
  only, and says "Ordinal and catalog revision define ordering"
  ([git and epoch model](../architecture/git-and-epoch-model.md), "Epoch facts
  and cause policy"). After Decision 5, ordinal order is the order in which this
  install observed and imported updates, and that is not the same on two
  machines.

The design pass states that order is stable across machines, but nothing in it
defines an ordering key that is the same on every machine. What the line's
dates mean is owner ask A3. The ordering key is an engineering choice that
depends on that answer. Writing GitHub's push time into `observed_at` before
the choice is made would be either a redefinition or a falsehood.

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

### 7. The host term, and what backfill can compute

Decision 1 needs a host term that is the same for every reader of the same
host, and `git_host_id` is not one (What is established). The handoff carries a
canonical hostname and a host authority fingerprint
([git and epoch model](../architecture/git-and-epoch-model.md), "Pull-request
authority and revision"). Not yet decided: which of the two to use, how it is
normalized, and what happens to a GitHub Enterprise Server reached under two
names.

It is also not established that the catalogue holds enough to backfill
existing rows without a network read. The catalogue stores a head OID per epoch
and a `pull_request_id` per scope. It is not known whether that stored id is
the provider value, or whether any host term is stored. If backfill cannot be
computed locally, it waits for the next read of each pull request.

## Owner asks

Each ask is one question. Nothing is built before A1 is answered yes. A2 and A3
decide the identity and the order, which are the first things implementation
writes.

**A1. Acceptance.**

> Do you accept RFC-BPR-0029 as the plan for the epoch work? Under it, each
> update's identity comes from the pull request and that update's commit, so it
> is the same on every machine. The list is the pull request's updates as far as
> GitHub still shows them, and it says when it may be missing some. Every update
> keeps a base commit, and existing catalogues gain the new identity without
> losing anything.

**A2. The same commit twice.**

> If a pull request is force-pushed back to a commit it already had earlier, is
> that one update on the epoch line, or two?

**A3. An update's date.**

> GitHub's timeline says when a force push happened, but not when an ordinary
> push did. For an ordinary push, the only date every machine sees is the one
> inside the commit. Should the epoch line show dates that are the same on every
> machine (GitHub's time for a force push, the commit's own date otherwise), or
> the time this app first saw each update, which differs from machine to
> machine?

## Architectural allocation

- **Core** owns the portable identity type, its derivation as a pure function
  of provider-side inputs, the legality of an imported epoch, validation of the
  base OID rule, and the partial-history fact. It has no clock, network or Git.
- **Host** owns the timeline read and its cap, the merge-base computation
  through the app-owned cache, the migration and backfill, and time as explicit
  input.
- **GitHub adapter** owns the new anonymous REST operation, its admission,
  paging and normalizer.
- **SQLite adapter** owns the schema version, the identity column and the
  backfill.
- **Qt adapter** presents epochs and the partial-history fact, and derives
  nothing.

## Prohibited designs

- Any locally minted or reader-specific value in the portable identity: the
  authority scope, the incarnation, the ordinal, `git_host_id`, the profile,
  the account, the viewer or the access mode.
- The timeline event's `node_id` as the identity.
- The base ref's OID at import time, recorded as a historical epoch's base.
- GitHub's push time written into `observed_at` before open question 5 is
  settled.
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

- Two independent catalogues that read the same public pull request, whose
  history contains at least one force push, through the host and not a widget
  fixture, hold the same portable identity for every update both can see, in
  the same order.
- The portable identity of an update is equal whether the update was read signed
  in or signed out, and does not change when the catalogue is deleted and
  recreated.
- A live read that finds a new head for the same pull request still appends an
  author-upload epoch, and an imported epoch advances neither the latest live
  ordinal nor the previous distinct head ordinal.
- An update observed live and later overwritten by a force push stays on the
  install that observed it. The epoch line says when the history it holds may
  be partial: when the timeline read failed, was refused or was cut off by the
  page cap, or when a force push overwrote updates this install did not see.
- Every epoch, live or imported, carries a valid base OID derived by the rule
  this RFC settles, and no imported epoch records the base ref's OID at import
  time as its base.
- A catalogue at the current schema version migrates by adding and backfilling
  the identity. No incarnation is re-minted and no ordinal is renumbered, and
  review-ledger, window-restoration, retained-ref, MCP and agent-launch
  references to existing epochs still resolve.
- The history read is anonymous REST and stays within a stated page cap, and an
  open that cannot make the read still produces today's live epoch.
- BPR-0646's attended handle criterion is asked again, unchanged, on a signed
  bundle carrying this work.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Identity is the same on two machines | Epoch host over two fresh SQLite catalogues | Recorded pull request and timeline responses for a force-pushed pull request, read into both; identities and order compared |
| Identity excludes local values | Epoch host | The same update read signed in, signed out, and after the catalogue is recreated; identities compared |
| Live append is unchanged | Epoch host | A second live handoff with a new head; cause and live ordinals inspected |
| Partial history is declared | Composed application | Timeline read failed, refused and capped, and a force push over an unseen update; the line's state inspected |
| The base OID rule holds | Core plus the app-owned cache | The rule's base compared with live-observed bases for the same heads, including across a base force push |
| Migration loses nothing | SQLite catalogue, on a copy of a current-version file | Ledger, restoration, ref and binding references resolved after migration; incarnation unchanged |
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

- [[BPR-0743]] carries the implementation. It is Blocked on the owner's answers
  to A1, A2 and A3. Its AC1 is the design pass this RFC records.
- [[BPR-0646]] (RFC-BPR-0028) depends on BPR-0743 for its attended handle
  criterion.
- No closeout card is allocated while this RFC is unaccepted. One is allocated
  when the owner accepts it, with its immutable RFC base at the accepted
  revision.
