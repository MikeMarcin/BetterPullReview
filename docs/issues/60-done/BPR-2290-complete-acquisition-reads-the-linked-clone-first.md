---
schema: "implement-rfc.card.v1"
id: "BPR-2290"
title: "A complete acquisition reads the linked clone first, borrows it as a reference and honours the cache's haves; a force-pushed head's import is logged, waits behind the reviewer and is never repeated after a wall"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2288--BPR-2291--19d6a88ae1a3.json","parent":"BPR-2288","origin":"owner card BPR-2288 (2026-11-19), Changes 1, 3 and 4","related_cards":["BPR-0846","BPR-1873","BPR-1884","BPR-2288","BPR-2291"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b170738ff","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^gh_cli_git_smart_transport$'","detail":"A complete historical acquisition whose linked clone holds the head succeeds with last_fetch_source linked_clone and only gh's local qualification launches (1 + help topics): no clone, token or API process. Its pack holds the head's commit, tree and new blobs and not the have's base, blob or ancestor. A shallow linked clone is not read (source host). A clone lacking the head is cloned from the (fixture) host with --reference, the pack still leaves out what the have reaches, and the linked clone's count-objects is unchanged; a sparse depth clone never takes a reference."}},{"criterion":"AC2","outcome":"pass","revision":"b170738ff","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^git_haves_trimmed_pack_historical$'","detail":"Real epoch host, Git cache and SQLite catalog over local upload-pack: a complete review's background import of a force-pushed head offers the live base and head as haves; the served pack (345 bytes) is complete but not self-contained against the live head (index-pack exit 1); the log has historical_acquire succeeded with 'mode=complete_closure source=host linked_clone=0 haves=2 pack_bytes=345 wire_bytes=730 elapsed_ms=43'."}},{"criterion":"AC3","outcome":"pass","revision":"b170738ff","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^git_haves_trimmed_pack_historical-wall$'","detail":"Two force-pushed heads, the transport times out on a historical fetch: one attempt only (HISTORICAL_WALL_ATTEMPTS:1), the failure is logged as historical_acquire / transport / timeout, and the live head stays latest."}},{"criterion":"AC4","outcome":"pass","revision":"b170738ff","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|application_epoch_host|gh_cli_transport|git_haves_trimmed_pack_.*)$'","detail":"The real Quill sink persists epoch_stage historical_acquire with its detail; the existing epoch host, transport and haves courses pass unchanged."}},{"criterion":"AC5","outcome":"pass","revision":"b170738ff","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^architecture_boundaries$' && python3 scripts/verify_documentation.py --root .","detail":"Boundaries pass once the linked clone's Git directory is resolved in gh_cli_transport.cpp (gh_cli_linked_clone_objects); DOCUMENTATION_OK."}}]
---

# BPR-2290: A complete acquisition reads the linked clone first and never repeats a full clone

Part of [[BPR-2288]] (Changes 1, 3 and 4's log).

## Cause, as found

The owner's sample shows the file view's `resolve_selected_diff` waiting on
the review's cache actor while another worker sat in
`acquire_git_protocol_v2_historical_pack`. That is the background import of a
force-pushed head (`start_historical_import`), which a complete review
(`object_loading_for` answers `complete` when the pull request's file or
commit list is beyond the read limits, or its commit count is unknown) runs on
its own after the live load:

- it asked for the head with no haves, and the gh transport cloned it with
  `--revision` and no depth, filter or reference: the repository's history;
- the transport ignored haves altogether and never read the linked clone;
- it ran on the cache actor at the reviewer's priority, so the file view
  waited behind it;
- a failure (the two-minute bound) skipped to the next force-push event and
  started the same kind of clone again, up to eight times;
- nothing was logged for it.

## Fix

- `git_smart_request_context::HistoryClone`; `epoch_application_host::set_history_clone`,
  set from the window's clone link projection for the review's repository; the
  live and historical complete acquisitions carry it.
- gh transport (`acquire_pack`, `clone_and_pack`): with a usable (not shallow)
  linked clone, each wanted commit it holds is read from it through a local
  store's alternates, with no clone, credential or viewer request; a commit it
  lacks is cloned with `--reference=<clone>` (`compile_gh_cli_git_clone_request`
  now admits a reference for a complete clone). The local `pack-objects` leaves
  out what each readable have reaches (complete mode only; the cache already
  accepts such packs, [[BPR-0846]]). `last_fetch_source()` reports where the
  objects came from.
- Historical import: the live base and head the cache holds as complete
  closure are its haves; it runs at preparation priority unless the reviewer
  selected it; it is logged as the new `historical_acquire` epoch stage with
  a detail of mode, source, haves, pack and wire bytes and duration; a
  timeout or size limit sets `HistoricalStopped`, so no further background
  import starts for that review.
- `docs/architecture/system-architecture.md` (gh CLI acquisition paragraph).

## Not done here ([[BPR-2291]])

- With no linked clone, a complete acquisition still clones the commit's
  whole history from the host (AC2 of BPR-2288). A selected-objects review was
  already bounded (depth one, no blobs).
- The two-minute bound is still a whole-operation timeout, not a progress
  timeout; the file view shows no fetching state, bytes or Cancel.

## Decision

The base commit the owner named and the force-pushed head the code fetched
could not be told apart from the sample alone; both come through this path
(a live complete acquisition clones base and head as clones 0 and 1), and both
now read the linked clone first.

## Not verified

The owner's repository and build: an epoch host course against GitHub through
`gh` was not run (no live mutation needed; the fixture `gh` runs real Git).
