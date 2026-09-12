---
schema: "implement-rfc.card.v1"
id: "BPR-0743"
title: "The epoch line shows only the updates this install has observed"
status: "Ready"
rfcs: ["RFC-BPR-0003","RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"71815a38","found_by":"attended-prep-2026-09-10 d19","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"6dc6dc36-4034-4e05-82cf-e06cc04f02e5","owner_direction":"2026-09-12"}
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
