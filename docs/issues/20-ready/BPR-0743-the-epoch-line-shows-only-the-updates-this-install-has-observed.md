---
schema: "implement-rfc.card.v1"
id: "BPR-0743"
title: "The epoch line shows only the updates this install has observed"
status: "Ready"
rfcs: ["RFC-BPR-0029","RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"71815a38","found_by":"attended-prep-2026-09-10 d19","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"6dc6dc36-4034-4e05-82cf-e06cc04f02e5","owner_direction":"2026-09-12"}
acceptance: [{"criterion":"AC1 The design pass's two recorded decisions (the derivation; whether the ordinal moves) are on this card before the model changes, with the signed-out probe's actual result beside them","outcome":"pass","revision":"f953ce9c","evidence":{"kind":"observation","detail":"Recorded in the card's 'Design pass, 2026-09-12 (d23, mandate r14)' section, before any model change and before any product source change -- none was made. The signed-out probe was RUN, not reasoned about: GET https://api.github.com/repos/NVIDIA/stdexec/issues/1846/timeline?per_page=100 with no credential returned HTTP 200, x-ratelimit-limit 60, x-ratelimit-resource core. So the timeline is anonymously readable over REST and the epoch line does not become a signed-in feature; the card's open worry does not hold. 18 items, of which 2 head_ref_force_pushed, each carrying a GitHub-side id and node_id and the head the branch was moved TO (6ffbd0ac at 2026-08-01T16:45:08Z, 51d2dad0 at 2026-08-09T22:19:26Z; the second equals the PR's current head.sha). 6ffbd0ac appears nowhere in GET /pulls/1846/commits, which demonstrates rather than asserts that commits cannot reconstruct the updates a force push overwrote; it is still fetchable anonymously (200 from both upstream and the fork), so an import would not list updates it cannot then diff. Three limits fell out of the same response and the design is built around them: the REST force-push event has NO before-commit (GraphQL has beforeCommit and is refused anonymously), timeline array position is not an ordering key and its committed entries carry AUTHOR dates not push times, and an ordinary push produces no event at all. Decision 1, the derivation: the portable identity is a digest over the pull request's authority scope and the update's HEAD commit OID, carried beside review_epoch_id. Not the event node_id -- it exists only for force pushes, so the current head usually has none, and catalog_database.cpp:86-88 excludes opaque provider tokens from the catalogue by design. Not the base/head pair the card offered as fallback, because the base OID is time-dependent even for a live read and is not recoverable at all for a historical update. Open sub-question recorded, not papered over: a force push back to a previous head gives two updates one identity. Decision 2, the ordinal: NOT redefined; {Authority, Incarnation, Ordinal} stays the local storage key. Redefining it cannot buy portability while Incarnation is in the id; two of its seven serializations are already beyond the catalogue's reach (git refs on disk, MCP bindings and agent-launch JSON already emitted); and density is an adapter guarantee (epoch_catalog.cpp:3029-3046, :3145-3162) that history paging and alias resolution depend on while core requires only strict increase (epoch_transition.cpp:2519-2537). Three of this card's own anchors were wrong and are corrected in the section: the anonymous admission header's path, the ordinal id write line, and the sixteen-hex-digit ref encoding's file."}},{"criterion":"AC2 Not another fixture: the synthetic three-row epoch_comparison_control test is not evidence for this card","outcome":"pending","revision":"f953ce9c","evidence":{"kind":"observation","detail":"Unbreached and untested. No check of any kind is claimed by this pass, so nothing has been offered as evidence, synthetic or otherwise. The constraint binds AC3's honest check and is carried forward intact."}},{"criterion":"AC3 The honest check runs through the host: a pull request whose history contains at least one force push is read twice into two independent catalogues and the two produce the same epoch list, identities and order; a second live handoff with a different headRefOid still appends with the author-upload cause","outcome":"pending","revision":"f953ce9c","evidence":{"kind":"observation","detail":"Not attempted, deliberately. The design pass concluded that the change exceeds what an owner direction recorded on the closed RFC-BPR-0003 can carry, and the scope's escalation clause says to bring that back rather than absorb it. Nothing is implemented until the owner answers. One thing this check now has that it did not: NVIDIA/stdexec#1846 is a confirmed suitable subject -- it carries two force pushes and one overwritten head, established by the probe above."}},{"criterion":"AC4 BPR-0646's handle criterion is asked again, unchanged, on a bundle carrying this card","outcome":"pending","revision":"f953ce9c","evidence":{"kind":"observation","detail":"Not asked and not askable. BPR-0646's handle criterion stays right as written and is not reworded; it is not judgeable on any bundle buildable today, which is why BPR-0646 was not a candidate in v009 and is not asked again until the epoch line carries the pull request's own updates. Unchanged by this pass."}}]
---

# The epoch line shows only the updates this install has observed

The owner, 2026-09-11, rejecting `BPR-0646-workbench-chrome` in the v008
attended session (decision `6dc6dc36`, 2026-09-12T04:24:06Z), verbatim:

> epoch handle doesnt move, only shows a single epoch event thought I'm
> sure there were multiple updates

One screenshot,
[the epoch line on NVIDIA/stdexec#1846](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-epoch-single-update-2026-09-11.png):
the row reads "Updates" with a single node captioned "Sep 11, latest", the
toolbar reads "no changes" and the body "Diff ready — 0 semantic changes".

## What the code says

A review epoch records *this application's own observation* of a pull
request's base and head, not a reconstruction of the pull request's history
on GitHub.
[RFC-BPR-0003](../../rfcs/RFC-BPR-0003-git-cache-pr-identity-review-epochs.md):
"A review epoch records base OID, head OID, observed time, ordinal, cause,
and material summary. A head change creates an author-upload epoch. A
base-only change creates a base-update epoch." The list therefore grows only
as this install reads the pull request again over time.

Nothing in the tree enumerates a pull request's past updates. The read
contract asks for `headRefOid` and the base ref and nothing else
(`src/adapters/github/src/github_read_contract.cpp:29,649,663,672,682`), and
one read produces one handoff carrying one base/head pair
(`src/application/src/github_window_read_host.cpp:459-489`, with
`Origin = pull_request_revision_origin::live` at `:480`). The model reserves
a cause for imported history — `epoch_origin::explicit_historical_import`
and `epoch_primary_cause::explicit_historical_import`
(`src/core/include/bpr/core/epoch_model.hpp:466,493`) — and validates and
persists it (`src/core/src/epoch_transition.cpp:215,230,246,281,2592`,
`src/adapters/sqlite/src/epoch_catalog.cpp:2901,3554,4294`), but no producer
for it exists in `src/app`, `src/adapters/github` or `src/application`. This
is a capability that was reserved and never built, not one that broke.

Signing out does not degrade it further: the anonymous branch
(`github_window_read_host.cpp:429-433`, `:436`, `:490-498`) only drops the
viewer and skips the current-viewer review scan; base, head and the epoch
append are identical either way.

The line drew what it was given. `qt::epoch_line` short-circuits every
geometry path at one update (`src/adapters/qt/src/epoch_line.cpp:183-189`,
`:221-228`), so both bracket handles collapse onto the single node and
there is nowhere to drag to; "latest" comes from `:211-212` and the update
count from `:147-149`. Retention is not the cause either: the cap is
100,000 epochs per pull request
(`src/application/include/bpr/application/epoch_catalog_port.hpp:28`,
asserted in `src/adapters/sqlite/src/catalog_database.cpp:77`); the RFC's
hundred-epoch figure is the resident Qt page, not a storage bound.

The catalogue on this Mac agrees. In
`~/Library/Application Support/BetterPullReview/BetterPullReview/state/v1/epoch-catalog.sqlite3`
the `pull_request_scope` row for pull request 1846 has `access_mode` 2
(public preview, no viewer), `next_ordinal` 2, `retained_epoch_count` 1 and
`previous_distinct_head_ordinal` NULL; its one `epoch` row is ordinal 1,
`origin` 1 (`initial_observation`), `primary_cause` 1, observed at
1789186737423 = 2026-09-12T04:18:57.423Z, five minutes before the verdict,
and `logs/diagnostics.log` ends its epoch stages at the same instant. The
pull request was new to this install and the owner's own first read minted
the single epoch.

So BPR-0646's criterion cannot be satisfied by naming a different pull
request, which is what both the v007 and the v008 asks assumed. No pull
request has more than one update on its first open. Whether
NVIDIA/stdexec#1846 has several updates on GitHub is very likely and was not
checked here: the diagnosis used the local catalogue and the source only, with
no network read.

## Held for the owner's decision

Two separable things are wrong and only the first needs the owner:

1. **History the application never had.** To show a pull request's earlier
   updates on the first open, the product has to read them (the timeline or
   the commits behind each force-push) and mint epochs with the
   `explicit_historical_import` cause the model already reserves. That is a
   new GitHub read and a new producer, it has an anonymous-access question
   of its own, and it is the only way BPR-0646's criterion as written
   becomes judgeable on a fresh open. The alternative is to change the
   criterion: the handles are judged after this install has seen the pull
   request change, which is the product as designed.
2. **The line does not say so.** With one update the row gives the reader
   nothing to distinguish "this pull request has one update" from "this is
   all I have seen since you first opened it", which is exactly the reading
   the owner made. This part needs no new read and no owner decision on
   scope, only a decision on where it is said.


That question is answered below, and the answer is neither of its two
options.

## Owner answer, 2026-09-12 (mandate r11)

Verbatim:

> An Epoch is an update to the PR, epochs identity should be stable and
> repeatable if I look at the PR on different machines. A purely observed
> epoch is not useful and terribly confusing.

The second option — keep the line a record of this install's observations
and reword BPR-0646's criterion — is refused outright. The first is
necessary but is not the whole answer, because the owner is not only asking
for a longer list. He is asking for a different **identity**: the same pull
request read on two machines must yield the same epochs, with the same
identities, in the same order. Three consequences follow, and the third is
the one the ask did not anticipate.

**An epoch is a property of the pull request, not of the reading.** The row
is the pull request's updates. Whether this install has seen them decides
what it can *show*, never what the list *is*.

**Today's identity is machine-local in both of its variable parts, not just
in its population.** `core::review_epoch_id` is
`{Authority, Incarnation, Ordinal}`
(`src/core/include/bpr/core/epoch_model.hpp:444-450`). `Incarnation` is a
random value minted per catalogue lifetime, added by
[BPR-0044](../60-done/BPR-0044-rfc-0003-epoch-catalog.md) so that
"recreated ordinals cannot collide with stale IDs" (`:17`) — so an epoch id
is not stable across a catalogue rebuild on this Mac, let alone across two
Macs. `Ordinal` is minted locally: the SQLite scope row owns `next_ordinal`
and hands it out under compare-and-swap
(`src/adapters/sqlite/src/epoch_catalog.cpp:3030-3045`, `:3147-3156`), so
ordinals are dense in *observation* order. The core transition never mints
one; it validates a caller-supplied `ProposedOrdinal`, requiring only that
it exceed the latest live ordinal
(`src/core/src/epoch_transition.cpp:2519-2537`), and writes it into the id
at `:2649`. So "stable and repeatable … on different machines" cannot be
reached by adding a producer behind the existing model. The identity has to
be derived from the pull request.

**The catalogue migrates by rebuild, not by relabel.** No GitHub-side event,
node or push identity is stored in the `epoch` table or in `epoch_anchor`
(`src/adapters/sqlite/src/catalog_database.cpp`), so an existing
locally-minted ordinal cannot be mapped onto a pull-request-derived one from
the columns on disk. The incarnation mechanism is precisely what makes a
rebuild under a new incarnation safe.

### What the product can see today

- The pull request read is one REST call, `GET /repos/{o}/{r}/pulls/{n}`
  with no query (`src/adapters/github/src/github_read_contract.cpp:1931-1933`,
  path at `:701-707`); base and head come from its record, and
  `src/application/src/github_window_read_host.cpp:480` writes
  `pull_request_revision_origin::live` unconditionally.
- `read_operation::list_pull_request_commits` **already exists end to end**:
  the enum, the recorded contract manifest, `all_read_operations` and
  `anonymous_read_operations`
  (`src/adapters/github/src/github_read_contract.hpp:27,49,70,89`), the REST
  route `GET /repos/{o}/{r}/pulls/{n}/commits?page=N&per_page=100`
  (`github_read_contract.cpp:1939-1943`), a fixture validator (`:1249`),
  paging and a normalizer, and anonymous admission
  (`github_anonymous_read_admission.hpp:62`). This corrects the diagnosis
  above: it is not true that nothing in the tree enumerates anything about a
  pull request's history.
- **But commits are not updates.** A force push rewrites the commit list
  rather than appending to it, so the commit enumeration reconstructs only
  the surviving lineage. The updates a review epoch most needs — the ones a
  force push overwrote — are exactly the ones it cannot see.
- What does enumerate updates is the timeline
  (`headRefForcePushedEvent`, `baseRefForcePushedEvent`,
  `baseRefChangedEvent`, `PullRequestCommit`), and **nothing in the tree
  asks for it**: no `timelineItems`, no `/issues/{n}/timeline`, no
  `/compare/`. Each was searched for.
- Signed out it is not merely absent, it is refused by construction:
  `admit_anonymous_read` rejects every GraphQL-routed operation
  (`src/adapters/github/src/github_anonymous_read_admission.hpp:99-108`),
  and the owner's v008 read was signed out (`access_mode` 2). A
  cross-machine-stable list on a signed-out open therefore needs a REST
  timeline read admitted anonymously, and it spends another request against
  the sixty-per-hour unauthenticated budget
  ([RFC-BPR-0017](../../rfcs/RFC-BPR-0017-unauthenticated-public-review-read.md):223,
  `:293-299`), where one pull request open already spends ten.

### What this does to BPR-0646

[[BPR-0646]]'s handle criterion is **right as written and is not reworded**.
The handles should move because a pull request with several updates should
show several nodes; what is wrong is the product, not the criterion. It
follows that the criterion is still not judgeable on any bundle that can be
built today, so BPR-0646 is **not** a candidate in the v009 packet and is
not asked again until the epoch line carries the pull request's own updates.

### What this does to part 2

Part 2 survives and is re-aimed. Once epochs are derived from the pull
request, one node means the pull request has one update and needs no
disclaimer. What still needs saying is when the history the line holds is
**partial** — a timeline read that failed, was refused, or was not permitted
signed out — so the row distinguishes a complete history from a partial one
rather than an observed one from a complete one.

## Scope

The slice opens with a bounded design pass and records its outcome on this
card **before** the model changes, because two choices below decide the blast
radius.

- **Epoch identity is derived from the pull request.** Two installs reading
  the same pull request produce the same epochs with the same identities in
  the same order. Choose and record the derivation: a pull-request-side
  event identity if the read can obtain one, otherwise a deterministic
  function of the pull request's identity and the update's base/head pair.
  `Incarnation` stays a catalogue-local guard; it must not remain part of an
  identity the product claims is portable.
- **Decide and record whether `ordinal` stays the dense local ordering key
  with a portable identity beside it, or is itself redefined.** Keeping it
  is much cheaper. If the slice redefines it, these are the coupling points
  it carries, all established:
  git retained refs encode the ordinal as sixteen hex digits
  (`src/adapters/git/src/installed_git_cache.cpp:1147-1157`, the encoding
  pinned by [BPR-0632](../60-done/BPR-0632-exercise-many-epoch-large-review-application.md):59-61);
  the `previous_upload_to_latest` and `my_last_reviewed_to_latest` aliases
  order by ordinal (`src/core/src/epoch_transition.cpp:996-1016`, `:1018`);
  the review ledger stores the comparison span as ordinals
  (`src/adapters/sqlite/src/review_ledger_database.cpp:108-109`); window
  restoration stores an anchor ordinal
  (`src/adapters/sqlite/src/window_review_restoration_store.cpp:109`); the
  MCP context surfaces them to agents
  (`src/adapters/mcp/src/mcp_context_helper.cpp:768-770`); and history
  paging is ordinal-keyed and pinned Done by
  [BPR-0056](../60-done/BPR-0056-rfc-0003-history-paging.md):36-37 and
  [BPR-0066](../60-done/BPR-0066-rfc-0003-bounded-qt-epoch-history.md):65,75.
  The amber "my last review" tick is **not** on that list: it matches by head
  OID, not ordinal (`src/adapters/qt/src/epoch_line.cpp:132`, `:164-170`).
- **The read gains an enumeration of the pull request's updates**, minting
  epochs through the `explicit_historical_import` origin and cause the model
  already reserves and the catalogue already validates and persists
  (`src/core/src/epoch_transition.cpp:215,230,246,281,2592`;
  `src/adapters/sqlite/src/epoch_catalog.cpp:2901,3554,4294,4564-4568`,
  where a historical epoch deliberately does not advance `latest_live_ordinal`
  or `previous_distinct_head_ordinal`). Today the only place that sets
  `Origin` is `github_window_read_host.cpp:480` and it writes `live`; the
  only producer of the import cause anywhere is a test
  (`tests/application/epoch_application_host_tests.cpp:3636-3650`).
- **Say what happens signed out.** Probe the anonymous timeline route before
  designing around it. If it turns out a public pull request's updates cannot
  be enumerated without a credential, bring that back rather than silently
  making the epoch line a signed-in feature: it changes what the owner was
  promised, and his v008 read was signed out.
- **Migration.** A rebuild under a new incarnation, not an in-place relabel.
  Review-ledger and restoration rows that name ordinals must not be silently
  orphaned; say what happens to them.
- **Part 2**, re-aimed: the epoch line, or the row beside it, says when the
  history it holds is partial, without adding chrome the accepted asset does
  not carry.
- **Escalate rather than absorb.** RFC-BPR-0003 is Auto-Verified. If the
  design pass concludes the change is larger than an owner direction recorded
  on a closed RFC can carry, bring it back for the owner's decision on
  whether it wants an RFC of its own, instead of reopening RFC-BPR-0003 from
  inside a card.
- Owned paths, once the design pass has chosen:
  `src/adapters/github/src/github_read_contract.{hpp,cpp}` and the anonymous
  admission, `src/application/src/github_window_read_host.cpp`,
  `src/core/include/bpr/core/epoch_model.hpp` and
  `src/core/src/epoch_transition.cpp`, the SQLite catalogue, and for part 2
  `src/adapters/qt/src/epoch_line.cpp`,
  `src/adapters/qt/src/epoch_comparison_control.cpp` and
  `tests/adapters_qt/epoch_comparison_control_tests.cpp`.

## Acceptance

- The design pass's two recorded decisions (the derivation; whether the
  ordinal moves) are on this card before the model changes, with the
  signed-out probe's actual result beside them.
- **Not another fixture.** `tests/adapters_qt/epoch_comparison_control_tests.cpp:811-860`
  builds three synthetic rows and publishes them straight to the widget —
  `epoch_row(3,"1","4")`, `epoch_row(2,"1","3")`, `epoch_row(1,"1","2")` at
  `:830-833`, `control.publish(loaded_catalog_update({…}))` at `:834` — with
  no host, no read and no catalogue, so it passes today while a real read
  yields one epoch, and it cannot distinguish an imported history from a
  synthesised one. It is not evidence for this card.
- The honest check runs through the host: a recorded pull request whose
  history contains at least one force push is read twice into two
  independent catalogues, and the two produce the same epoch list, the same
  identities and the same order. A second `live` handoff with a different
  `headRefOid` for the same scope still appends with the author-upload cause.
- [[BPR-0646]]'s handle criterion is asked again, unchanged, on a bundle
  carrying this card.

## Packet pointer, 2026-09-12 (v009)

The current attended packet is
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md). This card is not a candidate there and
neither is [[BPR-0646]]: the handle criterion is not judgeable until this card
lands, which is the point of the answer recorded above. The v008 packet, where
BPR-0646 was asked for the third and last time under the old reading, is
historical.

## Design pass, 2026-09-12 (d23, mandate r14)

The bounded design pass this card opens with. **No model change and no
product source change was made**, deliberately: the escalation conditional in
the scope fires, and the exact ask it produces is at the end of this section.
Every anchor below was re-read at `f2c49e12`; where this card's earlier
anchors had drifted, the corrected ones are given.

### The signed-out probe, run rather than reasoned about

The scope says "Probe the anonymous timeline route before designing around
it." It was run, on 2026-09-12, against the owner's own pull request from the
v008 capture, with no credential of any kind:

    GET https://api.github.com/repos/NVIDIA/stdexec/issues/1846/timeline?per_page=100

**HTTP 200.** `x-ratelimit-limit: 60`, `x-ratelimit-resource: core` — the
unauthenticated budget, so the route is admitted anonymously and is not a
signed-in feature. The concern this card raised, that a public pull request's
updates might not be enumerable without a credential, **does not hold**. What
the response says, item by item:

- 18 items: 8 `commented`, 3 `committed`, 2 `mentioned`, 2 `subscribed`,
  **2 `head_ref_force_pushed`**, 1 `reviewed`.
- Each force-push event carries a **pull-request-side identity and the head
  the branch was moved to**:
  `id 28822634866`, `node_id HRFPE_lADOFdOXg87q99XtzwAAAAa19oFy`,
  `commit_id 6ffbd0ac…`, `created_at 2026-08-01T16:45:08Z`; and
  `id 29193968055`, `node_id HRFPE_lADOFdOXg87q99XtzwAAAAbMGJm3`,
  `commit_id 51d2dad0…`, `created_at 2026-08-09T22:19:26Z`.
- `51d2dad0…` equals the pull request's current `head.sha`, so `commit_id` is
  the head **after** the push. And `6ffbd0ac…` appears **nowhere** in
  `GET /pulls/1846/commits` (three commits, all dated 2026-08-09T22:17:35Z).
  So this card's claim that commits cannot reconstruct the updates that matter
  is now **demonstrated** rather than asserted: `6ffbd0ac…` is an overwritten
  head that only the timeline still names.
- The overwritten head is still **fetchable**: `GET /repos/NVIDIA/stdexec/commits/6ffbd0ac…`
  is 200 anonymously, and so is the same call against the fork the
  `commit_url` points at. An import would not be listing updates it then
  cannot diff — contingent, of course, on GitHub still retaining the object.

Three limits came out of the same response, and they are what the design has
to be built around rather than footnotes to it:

1. **The REST force-push event has no before-commit.** Its complete key set is
   `id, node_id, url, event, commit_id, commit_url, created_at,
   performed_via_github_app`. GraphQL's `HeadRefForcePushedEvent` does carry
   `beforeCommit`, and GraphQL is refused anonymously
   (`src/adapters/github/src/github_anonymous_read_admission.hpp:83-110` — note
   the path, which this card previously gave as
   `include/bpr/adapters/github/`, and which is wrong).
2. **Array position is not an ordering key and `committed` dates are not push
   times.** The returned order runs 2026-08-01T16:45Z, 2026-08-09T16:11Z,
   2026-08-09T19:01Z, then `committed` entries dated 2026-08-01T15:20Z and
   twice 2026-02-13, then 2026-08-09T22:19Z. The commits endpoint reports
   committer dates of 2026-08-09T22:17:35Z for all three, so the timeline's
   `committed` timestamps are **author** dates — writer-controlled, and not
   when anything was pushed.
3. **An ordinary push is not an event.** The three `committed` entries are
   exactly the surviving lineage. Nothing distinguishes "three commits arrived
   in one push" from "three pushes", so ordinary-push update boundaries are
   not recoverable at all.

### Decision 1 — the derivation: the head OID, not the event id, and not the pair

The scope offered "a pull-request-side event identity if the read can obtain
one, otherwise a deterministic function of the pull request's identity and the
update's base/head pair". The read **can** obtain one. It should still not be
used, and the recorded decision is:

> **The portable epoch identity is a digest over the pull request's authority
> scope and the update's head commit OID. It is carried beside
> `review_epoch_id`, not inside it.**

Against the event identity, three reasons, the first two of which are fatal:

- **It does not exist for most updates.** `node_id` is present only on a
  force-push event. The pull request's *current* head — the update that
  matters most, and the only one a live read ever produces — has no timeline
  event at all unless it happened to arrive by force push. An identity that
  exists for some updates and not others is not an identity.
- **Storing it would break a stated catalogue invariant.** The schema's own
  header comment (`src/adapters/sqlite/src/catalog_database.cpp:86-88`) says
  no path, provider string, opaque token or generic payload enters the
  catalog, and it holds: there is no GitHub event id, node id or push identity
  in `epoch`, `epoch_anchor` or `pull_request_scope`
  (`catalog_database.cpp:137-163`, `:184-222`, `:231-248`). Every
  GitHub-derived value there is an 8-byte opaque numeric identity or a digest.
  `node_id` is precisely the opaque provider token that invariant excludes.
- It is not needed. The head OID is pull-request-side, already a first-class
  type, already stored, and identical on every machine.

Against the **base/head pair**, which is this card's own fallback wording, one
reason, and it is a finding rather than a preference: **the base OID is
time-dependent even for a live read.** Two installs reading the same pull
request an hour apart, with the base branch advancing in between, mint epochs
with different base OIDs for the same head. Putting the base into the identity
reintroduces one layer down exactly the machine-locality the owner asked to
remove. And for a historical update the base at that time is not recoverable
at all, by limit 1 above.

**One sub-question this pass does not resolve, recorded rather than papered
over.** If a pull request is force-pushed back to a head it previously had — a
rebase reverted — two updates share a head OID and therefore one identity.
Whether that is one epoch or two is a question about what the owner means by
"an update to the PR", not a question the code can settle; the near-neighbour
already in the model is the unique index
`epoch_source_revision(incarnation, source_observation)`
(`catalog_database.cpp:224-228`). Whoever implements this must answer it
first.

### Decision 2 — the ordinal stays the dense local ordering key

> **`Ordinal` is not redefined. `{Authority, Incarnation, Ordinal}` remains
> the local storage key; the derived identity above is what the product may
> claim is the same on two machines.**

Four reasons, strongest first.

1. **Redefining it cannot buy portability anyway.** This card frames the
   derivation and the ordinal as two separable choices. They are not: while
   `Incarnation` — 32 random bytes minted once per catalogue file
   (`catalog_database.cpp:1705-1707`, written at `:663-665`) — remains in
   `review_epoch_id` (`src/core/include/bpr/core/epoch_model.hpp:444-451`),
   the triple is machine-local no matter what the third field means.
   Portability is bought by **adding** a derived identity, not by re-deriving
   one of three fields.
2. **Two of its seven serializations are already outside the catalogue's
   reach.** Git retained refs are written on disk as
   `refs/betterpullreview/v1/scope/<64 hex>/epoch/<16 hex ordinal>/{base|head}`
   (`src/adapters/git/src/installed_git_process.cpp:1250-1259`, `:1296-1298`
   — **not** `installed_git_cache.cpp:1147-1157`, which only casts to
   `uint64`; this card's anchor was misattributed). And MCP revision bindings
   are handed out as text,
   `host/repo/pr/basehex/headhex/<leftEpoch>/<rightEpoch>`
   (`src/adapters/mcp/src/mcp_context_helper.cpp:731-741`, decoded at
   `:758-771`), as is the agent launch context JSON's `left_epoch`/`right_epoch`
   (`src/application/src/agent_launch.cpp:1597-1604` — a wire this card's list
   missed). Renumbering orphans refs already on disk and invalidates bindings a
   user or an agent may be holding.
3. **Density is relied on above the layer that guarantees it.** Core imposes
   no contiguity at all: `epoch_transition.cpp:2519-2537` requires only that
   `ProposedOrdinal` be valid and strictly exceed the latest live ordinal.
   Density comes from the SQLite CAS, which refuses any proposal that is not
   exactly `next_ordinal` (`epoch_catalog.cpp:3029-3046`) and advances by one
   (`:3145-3162`). History paging then keys on it
   (`epoch_history_cursor.Before`, `epoch_model.hpp:425-431`), and alias
   resolution compares ordinals directly (`epoch_transition.cpp:996-1007`).
   That is an adapter guarantee the layers above have quietly come to depend
   on, and moving the ordinal moves all of it.
4. The amber last-review tick is unaffected and this is confirmed at HEAD: it
   matches by head commit OID, not by ordinal
   (`src/adapters/qt/src/epoch_line.cpp:131-135`, `:163-174`).

The id write itself is at `epoch_transition.cpp:2646-2654` (the `.Ordinal` at
`:2652`), not `:2649`.

### Decision 3 — identity can be made stable; population cannot be made equal

This is the limit the probe proves, and it is why the identity must not be the
ordinal.

Machine A opens the pull request and observes head X, which arrived by an
ordinary push. The author then force-pushes to Y. Machine B opens the pull
request afterwards. Its timeline shows `head_ref_force_pushed → Y`, and **X
appears nowhere**: the REST event carries only the after-commit (limit 1), the
commits list carries only the surviving lineage (limit 3), and GraphQL's
`beforeCommit` is refused anonymously. No read B can make recovers X.

So A legitimately holds an epoch B cannot derive. Were the ordinal the
portable identity, A and B would then disagree about the identity of every
update after X. With a derived identity they agree about every update **both
can see**, and A simply holds one row more.

The honest statement the product can make is therefore:

> **Identity and order are stable and repeatable across machines. Population
> is best-effort, and the line says when it is partial.**

That is part 2 of this card, re-aimed again. d20 aimed it at "a timeline read
that failed, was refused, or was not permitted signed out". There is now a
fourth and more common case it must also cover: **an update existed and GitHub
no longer exposes it.**

### Decision 4 — the base OID, which is where this exceeds the card

Every epoch requires a valid base OID, and the requirement is **origin-blind**.
`epoch_transition.cpp:2199-2218` requires `BaseBranch.valid() && Base.valid()
&& Head.valid()` with matching algorithms for any accepted revision;
`plan_epoch_mutation` rejects at `:2470-2481` with `invalid_observation`;
`valid_summary` (`:2284-2300`) repeats it for every epoch; and at the schema
level `epoch_anchor.object_digest` is `NOT NULL` and non-zero
(`catalog_database.cpp:238-241`). **A head-only epoch is not representable at
HEAD, in core or on disk.**

The timeline does not supply a base OID for a historical update. Three ways
out, and each changes something normative:

- **(a) Use the base ref's OID at import time.** Rejected. It is
  time-dependent, so two machines importing on different days record different
  bases for the same historical update — the defect the owner asked to remove,
  reintroduced. It also records something *false*: the epoch would claim a
  base that was not the base then.
- **(b) Compute it as merge-base(head, base ref).** The strongest candidate.
  It is deterministic given the same commit graph, which a public repository
  supplies identically to every clone, and it is computable locally through
  the git cache that already exists. It changes the base from an **observed**
  value to a **derived** one, and it can move if the base branch is itself
  rewritten.
- **(c) Permit a head-only historical epoch.** Requires changing the core
  validation, `valid_summary`, and the `NOT NULL` schema. It changes what a
  review epoch *is*.

And there is a second term with the same shape: **observed time**. A
historical epoch's only honest timestamp is GitHub's `created_at` on the
force-push event, which is not this install's observation time. Writing
GitHub's push time into a field the RFC calls observed time is either a
redefinition or a falsehood, and it has to be the redefinition for two
machines to agree.

### Decision 5 — migration: do not re-mint, do not renumber

This card said "a rebuild under a new incarnation, not an in-place relabel".
The intent is right; the mechanism does not exist. `migrate_schema`
(`catalog_database.cpp:1018-1062`) is rebuild-then-**refuse**: `user_version`
is 3 (`:71`, written at `:649-655`), v1→v2 and v2→v3 are table rebuilds
(`:733-916`, `:1041-1053`), and any other version is refused rather than
recreated (`:754-756`, `:1021-1023`). Crucially the incarnation is **carried
forward verbatim** through every migration (`:1045`, `:822-823`) and is never
re-minted for an existing file.

A v3→v4 that did re-mint it would orphan, in one step: every
`window_review_restoration` row, which stores the incarnation explicitly
(`src/adapters/sqlite/src/window_review_restoration_store.cpp:99-109`); and a
renumbering on top would orphan every retained git ref, every `review_ledger`
row naming `left_epoch`/`right_epoch`
(`src/adapters/sqlite/src/review_ledger_database.cpp:243-244`), and every MCP
binding and agent-launch context already emitted.

**The cheap path is also the honest one, and it works because the model
already separates the two things.** Add the derived identity as a new column,
backfill it for existing rows from the head OID they already store, and let
the import append with fresh ordinals. Nothing is re-minted, nothing is
renumbered, nothing is orphaned. Ordinal order stops being chronological,
which is the price — and it is payable precisely because a historical epoch
already, by design, advances neither `latest_live_ordinal` nor
`previous_distinct_head_ordinal` (`epoch_catalog.cpp:4564-4568`) while still
consuming an ordinal through the CAS. The separation between "the newest live
epoch" and "the highest ordinal" is already in the catalogue; historical
import was built for it.

### Decision 6 — the read itself is the cheap part

One new anonymous REST read operation. It is mechanical and the sites are
enumerated: the enum (`src/adapters/github/src/github_read_contract.hpp:22`),
`all_read_operations` (`:65`), `anonymous_read_operations` (`:85`), the
exhaustive switches in `github_read_contract.cpp` (`:715-734`, `:736-758`,
`:1760-1789`, `:1792-1821`), the REST route table (`:1924-1985`, shaped like
`list_issue_comments` at `:1959-1964`), the anonymous admission switch
(`github_anonymous_read_admission.hpp:56-74`, which is **code**, a `constexpr`
exhaustive switch with no default, not a data list), the normalizer
(`github_read_normalizer.cpp:852-866`, `:967-980`), the paged read
(`github_paged_read.cpp:1324-1339`, `:1461-1475`) and five test switches.
Paging copies `read_rest_array_collection`
(`github_paged_read.cpp:2744-2791`) with `Link`-header `rest_has_next_page`
(`:1609-1660`), exactly as `list_pull_request_commits` does.

Two costs worth naming before anyone commits to it. Following
`list_review_comments`' precedent — in `all_read_operations` and
`anonymous_read_operations` but deliberately **not** in
`read_operation_catalog` (`github_read_contract.hpp:57-64`) — **no recorded
manifest or fixture regeneration is needed**, which removes the largest
feared cost. But the request budget is real: the probe confirms the
unauthenticated limit is **60 per hour**, one pull request open already spends
ten ([RFC-BPR-0017](../../rfcs/RFC-BPR-0017-unauthenticated-public-review-read.md):223,
`:293-299`), and a long-lived pull request's timeline pages. A page cap is
required, and a page cap is a *policy* choice: a truncated timeline is a
partial history, which part 2 then has to declare.

### The escalation, and the exact ask

The conditional in this card's scope — "if the design pass concludes the
change is larger than an owner direction recorded on a closed RFC can carry,
bring it back" — **fires.** Not on size, on kind. Four things change that
RFC-BPR-0003 fixed:

1. **What identifies an epoch.** A second identity concept is added beside
   `review_epoch_id`, and the RFC's identity becomes the local storage key
   rather than the identity.
2. **What "base OID" means** for a historical epoch: observed, derived from a
   merge base, or absent. All three are normative and one must be chosen.
3. **What "observed time" means** for a historical epoch: this install's
   observation, or GitHub's push time. Two machines cannot agree on the first.
4. **The catalogue schema version**, against a migration mechanism that
   refuses unknown versions by design.

That is four of the six things the RFC's sentence "A review epoch records base
OID, head OID, observed time, ordinal, cause, and material summary" pins,
touched in a single change. It is an RFC's worth of normative movement, not a
note appended under a closed one. **Nothing is implemented on this design pass
until the owner answers**, and the ask is one question:

> The epoch work you asked for changes what RFC-BPR-0003 says an epoch *is* —
> what identifies it, what its base OID means when GitHub only remembers the
> head, and whether its timestamp is when you saw it or when it was pushed.
> Do you want it as an RFC of its own, or as an amendment recorded against
> the closed RFC-BPR-0003?

Two things that do **not** need him, recorded so the ask stays single: the
signed-out question is settled (the timeline is anonymously readable, so this
is not a signed-in feature), and BPR-0646's handle criterion stays right as
written and unasked until this lands.

## Bound to RFC-BPR-0029, 2026-09-12 (d24, mandate r15)

The ask above is answered. The question was whether the epoch work should be an
RFC of its own or an amendment recorded against the closed RFC-BPR-0003. The
owner answered, verbatim: "It's own rfc".

This card now belongs to
[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md),
which records this design pass as its proposed decisions. `rfcs` names
RFC-BPR-0029 in place of RFC-BPR-0003. RFC-BPR-0028 stays, because
[[BPR-0646]]'s handle criterion waits on this card. The references to
RFC-BPR-0003 above record how the work was first framed, and they are left as
written.

The card leaves human-review because the question it was waiting on has been
answered. It is **Blocked on `rfc-bpr-0029-owner-asks`**. RFC-BPR-0029 is
Proposed and not accepted, and nothing is built until the owner answers its
three asks:
- **A1:** does he accept the RFC?
- **A2:** is a force push back to an earlier commit one update or two?
- **A3:** what is an update's date?

A2 and A3 decide the identity and the order, and those are the first things any
implementation writes.

AC1 stands. AC2, AC3 and AC4 are unchanged and pending, and none of them is
reworded.

One correction to Decision 1 is recorded on the RFC rather than rewritten here.
"A digest over the pull request's authority scope" cannot mean
`pull_request_authority_scope_id`. That value is random and partitioned by
profile and account (`docs/architecture/git-and-epoch-model.md`, "Cache and
epoch scope"), so a digest over it would differ between machines. The inputs
have to be the provider's identity of the pull request plus the head OID. The
provider identity is GitHub's numeric pull request id, normalized at
`src/adapters/github/src/github_paged_read.cpp:2388`, together with a host term
that is still open.

The RFC also carries as open everything this pass left open or did not reach:
- the head collision;
- the base-branch events the probe never saw;
- paging a large timeline;
- merge-base validation;
- the date, and with it an order that is the same on every machine, which this
  pass asserted but did not define;
- the unbuilt site list for the new read, where a search finds one site the list
  omitted (`src/app/src/large_review_fixture.hpp:305`);
- the host term, and what backfill can compute.

## Revised proposal after owner answers, 2026-09-12 (d25, mandate r16)

The owner **did not accept RFC-BPR-0029 as written**. Support for portable
identity, partial-history reporting and lossless migration was conditional on
correcting three errors: separate updates can reach the same head; surviving
commits are not a count of pushes; and a computed comparison base must never
be presented as the original historical base. His A2 answer is that A→B→A
contains **two** occurrences sharing A's head, but no occurrence may be
invented without GitHub evidence. A missing boundary is reported as incomplete
history. His A3 answer selects GitHub event time where available and a
consistently selected commit timestamp otherwise, labeled as a **commit date**
and never a push time; local first-seen remains separate, and deterministic
ordering cannot claim equal timestamps prove push order.

[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
now proposes typed occurrence identities (numeric GitHub force-push event id
where recorded, head identity for an unambiguous live transition), no epochs
inferred per surviving commit, unknown historical base separate from an
optional derived comparison base, and migration that preserves legacy rows
even when their portable occurrence identity cannot safely be backfilled.
Existing catalogue rows lack both hostname and GitHub event id; the proposal
records that engineering limit rather than assigning duplicate heads one
identity. The committer timestamp is the RFC's consistent no-event date
choice, subject to code-path verification before implementation acceptance.

This card remains **Blocked on `rfc-bpr-0029-owner-asks`**, now one ask only:
whether the owner accepts the *revised* proposal. AC1's d23 design-pass
evidence remains historical and is not silently rewritten to match the new
proposal. AC2, AC3 and AC4 are pending; no epoch product work, closeout card or
new BPR-0646 attended candidate is authorized until the revised RFC is
accepted. Its AC3 must test the same evidenced occurrences across two
catalogues and explicitly check the incomplete/ambiguous cases; it cannot
claim that missing ordinary push boundaries were reconstructed.

## Revised RFC accepted, 2026-09-12 (d26, mandate r17)

The owner answered **yes** to the revised RFC-BPR-0029 acceptance question.
[The accepted RFC](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
was committed at `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d` and its
closeout is [[BPR-0750]]. The earlier blocked note above records the state
before that answer; it no longer governs. This card is Ready, and its original
AC1 evidence and pending AC2–AC4 remain intact. Product work must preserve
typed evidenced occurrences, explicit incomplete history, unknown historical
bases separate from derived comparison bases, and lossless migration with
unresolved portable identity where provenance is insufficient. A fixture that
publishes three rows straight to Qt cannot satisfy AC3. The next owner is
assigned only after BPR-0729's active file ownership has safely cleared.
