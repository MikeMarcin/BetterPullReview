---
schema: "implement-rfc.card.v1"
id: "BPR-2291"
title: "What BPR-2288 still needs: the file view says it is fetching and can cancel, a fetch is bounded by progress, and no review fetches the whole history of a large repository; workspace expiry and checkout cost split to BPR-2293 and BPR-2294"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "1e0c55a2e", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(qt_diff_workbench_scale_(100|125|200)|application_epoch_host|qt_pending_veil|qt_epoch_comparison_scale_100)$'", "detail": "test_a_file_waiting_on_a_fetch_says_so_and_cancels: a plain wait offers no Cancel; with a fetch the plate says 'Fetching the base commit from github.com…' and 'Nothing has arrived yet', then '12.4 MB received so far.', Cancel sits under both lines and calls the cancel once; the fetch ending restores the plain wait; a failure sentence shows no Cancel. history_fetch_is_bounded_and_observed: during the fake transport's fetch history_fetch() names base and head, the host and the 1234 bytes the transport reported, and nothing after. The window dispatches cancel_epoch_refresh_intent (existing core transition)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "1e0c55a2e", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(github_cli_process_runner|gh_cli_transport|gh_cli_git_smart_transport)$'", "detail": "test_progress_bounded_requests: a child whose store grows every 100 ms for 1.2 s finishes under a 400 ms idle bound; a child that sends nothing stops as timeout at its 300 ms idle bound well before its 5 s ceiling; a ceiling past 60 min or an idle bound past the total is refused. A compiled clone request carries IdleTimeout 60 s and Timeout 30 min."}}, {"criterion": "AC3", "outcome": "pass", "revision": "1e0c55a2e", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(application_github_window_read_host|application_epoch_host)$'", "detail": "test_loading_budget_shape: past 32 MiB, shapes past the file or commit limit or with no reported count load selected objects; within it or unknown they keep complete. history_fetch_is_bounded_and_observed: a lease without sparse support refuses such a review with fetch_history_too_large and no transport.acquire or transport.fetch; a sparse-capable lease retains sparse_commit_trees."}}, {"criterion": "AC4", "outcome": "pass", "revision": "1e0c55a2e", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -j 6 -R 'architecture_boundaries|git_protocol|epoch|gh_cli|github_cli_process|window_read_host|diff_workbench|pending_veil|quill_logging$|haves_trimmed|composed_github|github_application|rfc0015' && python3 scripts/verify_documentation.py --root .", "detail": "45/45 passed, including architecture_boundaries, git_protocol_boundary, core_epoch_policy and the haves-trimmed historical courses; DOCUMENTATION_OK. clang-format 22 check and clang-tidy (warnings as errors) clean on every changed C++ file."}}]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2288--BPR-2291--19d6a88ae1a3.json","parent":"BPR-2288","origin":"split from owner card BPR-2288 (2026-11-19) after BPR-2289 and BPR-2290","related_cards":["BPR-2288","BPR-2289","BPR-2290","BPR-1972"],"split_into":["BPR-2293","BPR-2294"]}
---

# BPR-2291: What BPR-2288 still needs

[[BPR-2289]] and [[BPR-2290]] landed the cheap parts of [[BPR-2288]]. This card
holds the rest; the owner's card states the reasons in full.

## Scope

1. **File view state (BPR-2288 Change 4, AC3/AC4's view half).** While a file
   waits on an acquisition, it says "Fetching the base commit from <host>…",
   with bytes when known, and a Cancel that stops it; after a timeout it says
   what failed and what the reviewer can do. The `historical_acquire` and
   `protocol_acquire` stages already log the facts.
2. **Progress timeout (Change 3).** The clone runner's deadline becomes an
   idle bound: a clone still receiving data (its `tmp_pack` growing) is not
   killed at two minutes; one that receives nothing for N seconds is.
3. **No history without a linked clone (Change 2, AC2).** A complete
   acquisition with no linked clone must not clone the commit's history: fetch
   it shallow (`--depth=1 --filter=blob:none` and the shown files' blobs), or
   allow `complete_closure` only below a repository-size threshold (GitHub's
   `diskUsage`). This changes what a complete review's cache holds and needs
   the cache's closure checks to accept it.
4. **Workspace expiry (Change 5, AC6).** Retained workspaces are removed after
   their session ends, after N days unused, and under a total-size cap, oldest
   first; Settings shows their total size with Remove. The 113 GB already on
   disk is reclaimed by this, not by hand.
5. **Checkout cost (AC5 "ready in seconds").** A retained workspace still
   writes every file of a large head; check out from the linked clone's index
   as a cache, or sparsely (the pull request's files, widened on demand).

## Split (2026-11-19)

Items 4 and 5 are their own cards: [[BPR-2293]] (workspace expiry, size cap
and the Settings row; BPR-2288 AC6) and [[BPR-2294]] (a large head's
checkout in seconds; BPR-2288 AC5's "ready in seconds"). This card closes
items 1-3.

## Acceptance

BPR-2288's AC2, AC3 (message) and AC4 (view), and AC7 for the tests this card
adds:

- AC1. A file waiting on a live fetch says which commits are fetched from
  which host and how much has arrived, offers Cancel, and Cancel stops the
  fetch (`cancel_epoch_refresh_intent`); a wait on anything else offers none;
  a stalled fetch ends in a sentence saying what failed and what to do.
- AC2. A clone from the host is stopped when nothing arrives for a minute, not
  at a total bound while it still receives.
- AC3. No review fetches the whole history of a repository GitHub reports past
  32 MiB: a sparse-capable connection loads selected objects whatever the pull
  request's counts, and any other connection refuses before asking the host.
- AC4. `architecture_boundaries` and the documentation gate pass.

## Fix

- **Progress bound.** `github_cli_process_request::IdleTimeout`: the runner
  restarts the idle clock whenever the request's output or its supervised
  store grows, and stops it as `timeout` after the idle bound; `Timeout` is
  then only the ceiling (`github_cli_process_max_progress_bounded_timeout`,
  60 min). Every non-kept `gh repo clone` takes a 60 s stall bound
  (`gh_cli_clone_stall_bound`) and a 30 min ceiling (`gh_cli_clone_ceiling`).
- **No whole history of a large repository.** `accepted_pull_request_shape`
  carries GitHub's repository size (`RepositoryBytes`, from REST `size`).
  `object_loading_for` answers `selected_objects` for any shape whose
  repository is past `maximum_complete_closure_repository_bytes` (32 MiB, the
  CLI clone's pack ceiling), so a sparse-capable connection fetches base and
  head at depth one with no blobs, and the blobs of the files opened. A
  connection without sparse support refuses such a complete acquisition before
  any host request with the new `fetch_history_too_large` failure, whose
  sentence says why and what to do.
- **The file view says it.** `git_smart_wire_consumer::received` and the
  protocol v2 client's counted consumer report bytes as they arrive (wire
  bytes natively; the gh clone's store, sampled twice a second). The epoch
  host's `history_fetch()` says which commits a live fetch wants (base, head),
  the host and the bytes so far, and nothing once it ends. The window reads it
  twice a second; the comparison veil says "Fetching the base commit from
  github.com…", then "12.4 MB received so far." (or "Nothing has arrived yet"),
  with a new Cancel (`pending_veil::set_cancel`) that dispatches
  `cancel_epoch_refresh_intent`. A stalled fetch ends as `fetch_network`,
  whose sentence now names the minute with nothing received.
- `docs/architecture/system-architecture.md` and `product-contract.md` say so.

## Decisions

- Threshold: a single 32 MiB whole-history budget from GitHub's reported size,
  not a per-transport limit; an unknown size keeps today's rule (REST always
  reports it). Past it, a pull request beyond GitHub's file or commit limits
  loads selected objects, overriding the old "limit violation grants no lazy
  acquisition" rule for those repositories only: a complete clone there could
  never fit the CLI clone's 32 MiB pack and only failed after minutes.
- Cancel reuses the review history's own cancel; it applies to the live
  preparation, which is what a waiting file waits on. A force-pushed head's
  background import is not named or cancelled here (BPR-2290 made it yield
  and stop after a wall).

## Not done

- Workspace expiry and the Settings row: [[BPR-2293]]. Large-head checkout:
  [[BPR-2294]].
- A native (non-CLI) connection still cannot load selected objects, so a
  large repository's over-limit pull request is refused there, not loaded.
- No live run against the owner's repository or GitHub; fixtures only.
