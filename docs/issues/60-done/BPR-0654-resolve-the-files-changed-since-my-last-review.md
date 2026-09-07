---
schema: "implement-rfc.card.v1"
id: "BPR-0654"
title: "Resolve the files changed since my last review for the overview"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"With a last-reviewed head behind the current head, the overview marks exactly the files that differ between the two, counts them, and Continue review lands on the first of them","outcome":"pass","revision":"60ad21c3","evidence":{"kind":"observation","detail":"application_overview_since_review_contract (new): over github-com and ghes-3.18, the seed run marks the first head reviewed and the reuse run, open at the second head over the retained cache, prints OVERVIEW_SINCE_REVIEW:files=1:changed=review.cpp:entry=review.cpp from the overview page's projection (the fixture's two heads differ in review.cpp's blob, verified with git ls-tree on the retained cache). Mutating the host to answer an empty set makes the reuse run print files=0 and the contract fail. application_epoch_host pins resolve_changed_paths: exactly the differing paths in request order, one lookup per path per side through the one retained session, the selected comparison untouched, refusals for another head, another snapshot, a pending ingestion and a closing host; the always-changed mutation fails it."}},{"criterion":"application_pull_request_overview, qt_pull_request_overview_page and the composition contracts pass with their digests repinned","outcome":"pass","revision":"60ad21c3","evidence":{"kind":"observation","detail":"application_pull_request_overview, qt_pull_request_overview_page, application_github_public_path_github_com, application_defect_composition_contract, application_epoch_persistence_{github-com,ghes-3_18} pass; application_review_round_trip_composition_contract repinned in two passes and green; test_network_isolation_manifest passes with the new Windows uncontained pin; check_boundaries, strict tidy on main.cpp, github_application.cpp, epoch_application_host.cpp and its test, clang-format --Werror and verify_documentation.py pass. application_epoch_maintenance_public_path is red at 60ad21c3 with this slice stashed (a re-assert race like BPR-0655's) and is carded separately."}}]
---

# Resolve the files changed since my last review for the overview

[BPR-0647](BPR-0647-pull-request-overview-page.md) decides
the overview from the accepted snapshot, the private ledger and the retained
update rows. One column it cannot yet answer exactly is "changed since my
last review": the set of files that differ between the head I last reviewed
and the current head is resolved by no host today, so the overview carries
`overview_change_since_review::unknown` for every row while the head has
moved, the heading says the changes are not yet resolved, and Continue
review falls back to the first file not read.

## Scope

- Resolve the changed set in the host from material the epoch host already
  holds: the comparison between the last-reviewed update's head and the
  current head, resolved without disturbing the reviewer's selected
  comparison, or the per-epoch file records the catalog retains.
- Feed it to `overview_context::ChangedSinceReviewPaths`, which the
  projection builder and the page already honour: changed and new chips,
  the "Files changed" count, and Continue review landing on the first
  changed file.
- Owned paths: `src/app/src/github_application.cpp` (the join), any epoch
  host query it needs under `src/application`, and their focused tests.

## Acceptance

- With a last-reviewed head behind the current head, the overview marks
  exactly the files that differ between the two, counts them, and Continue
  review lands on the first of them.
- `application_pull_request_overview`, `qt_pull_request_overview_page` and
  the composition contracts pass with their digests repinned.

## Notes (2026-09-07)

- **Source.** The catalog retains no per-epoch file list and no host
  compared two arbitrary commits, but the retained cache answers
  `find_blob_at_path` for any commit it holds. The epoch host gained
  `resolve_changed_paths(request, completion)`: for the accepted review,
  with `Right` the accepted head and no ingestion in flight, it verifies
  `Left` with `inspect_commit_object` and looks every requested path up at
  both commits on the review's cache actor; a path whose blob differs (an
  absent side counts) is changed. The job is admitted like every other and
  its completion runs on the decision lane, so the reviewer's selected
  comparison is neither replaced nor waited on. Refused before admission
  (returns false) for another head, another snapshot's authority, a pending
  ingestion, or a closing host.
- **Join.** `publish_overview` asks once per (last-reviewed head, accepted
  head, snapshot file list) when the ledger's last-reviewed head differs
  from the accepted head, keeps the answer until an input moves, and feeds
  `overview_context::ChangedSinceReviewPaths`; the completion republishes
  the overview. A refusal and a `stale` answer are asked again on the next
  publish (the catalog delta after ingestion publishes one); a failure is
  kept until an input moves, so a missing object does not turn into a job
  per publish.
- **Composed course.** A moved head cannot be exercised in one run: the
  fixture re-accept strips the smart transport, so the second head's
  ingestion fails and the epoch host keeps the first head. The new
  `application_overview_since_review_contract` runs the epoch fixture's two
  phases per fixture with `--overview-since-review`: the seed run marks the
  first head (the one opened before its refresh brought the second) as
  reviewed; the reuse run opens at the second head over the retained cache
  and, once both epochs are in the history and no row is still unknown,
  prints `OVERVIEW_SINCE_REVIEW:files=1:changed=review.cpp:entry=review.cpp`.
  With the host mutated to answer an empty set the reuse run prints
  `files=0` and the contract fails; with the test's blob comparison mutated
  to "always changed" `application_epoch_host` fails.
- **Follow-up, not carded.** The plain public-path course's overview branch
  is unchanged; the flag is only meaningful over the epoch phases and fails
  the run as `OVERVIEW_SINCE_REVIEW_FAILED:phase` elsewhere.
- **Found at this head, belonging to no card.**
  `application_epoch_maintenance_public_path` fails at 60ad21c3 with this
  slice stashed, alone, in `epoch_maintenance_select_right` (stage 15) with
  the epoch status "Resolving the exact comparison…": the stage re-asserts
  the left selector on every tick while the core is resolving the anchor it
  just dispatched, the same race BPR-0655 fixed in the defect probe. Carded
  separately.
