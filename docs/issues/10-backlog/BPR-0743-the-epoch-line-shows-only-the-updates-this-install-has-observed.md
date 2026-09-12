---
schema: "implement-rfc.card.v1"
id: "BPR-0743"
title: "The epoch line shows only the updates this install has observed"
status: "Backlog"
rfcs: ["RFC-BPR-0003","RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"71815a38","found_by":"attended-prep-2026-09-10 d19","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"6dc6dc36-4034-4e05-82cf-e06cc04f02e5","held_for":"owner decision on scope"}
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

Ask the owner: should the application import a pull request's earlier
updates on first open (a new capability under RFC-BPR-0003), or does the
epoch line stay a record of what this install has observed, with BPR-0646's
criterion reworded to match? Part 2 proceeds either way.

## Scope

Fixed once the answer lands. Part 2 regardless:

- The epoch line, or the row beside it, distinguishes an observed history
  from a complete one at one update, without adding chrome the accepted
  asset does not carry.
- Owned paths for part 2: `src/adapters/qt/src/epoch_line.cpp`,
  `src/adapters/qt/src/epoch_comparison_control.cpp`,
  `tests/adapters_qt/epoch_comparison_control_tests.cpp`. Part 1, if the
  owner wants it: `src/adapters/github/src/github_read_contract.cpp`,
  `src/application/src/github_window_read_host.cpp`,
  `src/core/src/epoch_transition.cpp` (the existing import cause) and the
  SQLite catalogue.

## Acceptance

- To be fixed from the owner's answer. Whatever lands, the check is not
  another fixture: `tests/adapters_qt/epoch_comparison_control_tests.cpp:811-860`
  builds three synthetic rows and publishes them straight to the widget, so
  it passes today while a real read yields one epoch. The honest check runs
  through the host: a second `live` handoff with a different `headRefOid`
  for the same scope appends ordinal 2 with the author-upload cause and the
  line then carries two nodes.
- BPR-0646's remaining criterion is reworded or re-asked from this card's
  outcome; it is not asked again on another pull request unchanged.
