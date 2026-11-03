---
schema: "implement-rfc.card.v1"
id: "BPR-1842"
title: "A pull request is usable as its parts arrive, starting with its name"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"performance","found_by":"Owner, 2026-11-03: \"Loading a PR needs to be faster or at least become more usable more quickly. For example the title bar doesn't fill in until the whole PR is loaded. The description and summary load quickly enough, the rest takes longer than I would like.\"","reservation":"docs/issues/_reservations/BPR-1842--BPR-1844--749b2d50ac34.json","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0795","BPR-1200","BPR-1357","BPR-1576","BPR-1841"],"frontend_design":"frontend-design:frontend-design loaded before the plate and heading words; no new visual language: the provisional rows and the plate are BPR-1576's accepted loading pattern.","measurement":"Release (qualification-release), macOS arm64, offscreen, public-path fixture holding every read 330 ms (one measured `gh api` launch and round trip on the owner's Mac); before and after interleaved under the same load (~40)."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4be2b4a25","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_progressive_open_public_path$'","detail":"Composed application: the title row names octo-org/review-repository #17 at 0.70 s, with the Overview's title (0.69 s), against the accepted review at 6.38 s; before, it filled only at acceptance (>= 6.35 s). Switching to #18 never shows #17's name while #18 loads. Release A/B, three interleaved rounds."}},{"criterion":"AC2","outcome":"pass","revision":"4be2b4a25","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_progressive_open_public_path|application_pending_states_public_path|qt_pending_veil)$'","detail":"The Overview lists GitHub's changed files at 1.02 s (before: 6.35 s, at acceptance) under the partial plate 'Loading comments, reviews and checks…'; a file activated from that list says 'Opening <file> when the review has loaded…' and opens in Review by itself once the review is accepted and the Files drawer lists it. The BPR-1576 course is unchanged and passes."}},{"criterion":"AC3","outcome":"pass","revision":"4be2b4a25","evidence":{"kind":"command","command":"ctest --preset dev -R '^(git_process_contract|git_cache_contract)$'","detail":"The review cache reuses one admission of its installed Git across pull requests: a second process over the same Git and isolation root launches nothing to admit it, and a Git whose file changed is admitted again. Release A/B with no network delay: accepted review to first diff for the second pull request 0.93 s mean (907/927/954 ms) against 1.63 s (1476/1423/1983 ms) before; the first pull request of a session is unchanged (~1.6-2.1 s)."}},{"criterion":"AC4","outcome":"pass","revision":"4be2b4a25","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_github_window_read|application_github_window_read_host|github_paged_read|git_anonymous_public_read|qt_composed_github_scale_100|qt_workbench_window_scale_100|application_attended_recent_replay|application_personal_discovery_public_path|application_overview_double_click)$'","detail":"Core admits one file list per pending read, only after its metadata, never for a cancelled or replaced read; the host shows it once without snapshot, epoch or file authority; the provider observes it right after the last files page. clang-format 22 check passes; clang-tidy 22 over the changed translation units reports only four findings on lines from earlier commits."}}]
---

# BPR-1842: A pull request is usable as its parts arrive, starting with its name

## Report

The owner, 2026-11-03: "Loading a PR needs to be faster or at least become
more usable more quickly. For example the title bar doesn't fill in until the
whole PR is loaded. The description and summary load quickly enough, the rest
takes longer than I would like."

## Measured

A new course, `application_progressive_open_public_path`, opens two pull
requests from Home through the composed application on the public-path
fixture. The fixture holds every read of the load for a set round trip
(`EveryReadMilliseconds`, 330 ms: one `gh api` launch and answer measured on
the owner's Mac) and logs what the load read. The course prints when each
surface first became usable.

Release, interleaved before/after under the same load (milliseconds from the
gesture, pull request #17):

| Surface | Before | After |
|---|---|---|
| Overview title and description | 690 | 690 |
| Title row names the pull request | >= 6,350 (at acceptance) | 700 |
| Overview file list | 6,350 | 1,020 |
| Review accepted, Files drawer | 6,380 | 6,380 |
| Accepted review to first diff, second pull request (no network delay) | 1,630 | 930 |
| Reads | 19 (18 for a second pull request) | unchanged |

## Root causes

- **The title row** (`workbench_window::update_strip_identity`) was fed only by
  the accepted review's projection. GitHub's first answer already reached the
  Overview as a provisional observation ([[BPR-0795]]) but carried no
  owner, repository or number, and nothing gave it to the title row.
- **The file list** arrived on the third read but was held inside the snapshot
  until all nineteen reads were done; nothing observed it earlier.
- **Accepted review to first diff** spent about a second, every time a
  pull request was opened, admitting a fresh installed Git for the review
  cache: three launches to resolve and check it, then the sha1 and sha256
  capability probes, a dozen launches in all (macOS `sample` of the course,
  `open_actor_cache` → `make_installed_git_process`).
- **The reads themselves** are the largest remaining cost and are not changed
  here: repository, pull request, files, commits, reviews, threads, comments,
  search, checks, statuses, five merge-requirement reads, the timeline, two
  merge-base reads and a confirming re-read, one after another on the `gh`
  foreground lane (one process at a time).

## Built

- **Core** (`github_window_read`): `accept_files_observation`, at most one per
  pending read and only after its metadata observation; a cancelled or
  replaced read admits none.
- **Provider** (`perform_paged_github_read`): a second
  `overview_observed` with the complete file list right after its last page,
  without authority; `github_pull_request_overview_observation::Files`.
- **Host** (`github_window_read_host`): the overview callback posts the
  metadata, then the file list, once each; `github_loading_overview_projection`
  gains `Owner`, `RepositoryName`, `Number` and `Files`
  (`project_github_file_rows`, shared with the cached catalogue).
- **Qt**: the workbench keeps a loading identity for the title row until the
  review is accepted or the read ends, never compared as the open pull request
  (refresh and local-cycle logic still see only the accepted name). The
  Overview presents the provisional rows with review-only columns hidden under
  the partial plate, and remembers a file activated meanwhile; the workbench
  opens it in Review once the accepted review's Files drawer lists it, if the
  Overview is still shown.
- **Git** (`installed_git_admissions`): the review cache holds one; a process
  over the same executable, resolved Git and isolation root (same canonical
  path, size and modification time) is configured as the first admission left
  it, without launches. Any difference admits again. Nothing persists across
  runs.

## Left open

- Concurrent or fewer reads. The accepted review still waits for every read in
  series; with `gh` each costs a process launch. Running the optional tail
  (search, checks, statuses, merge requirements, timeline) on further `gh`
  lanes, or accepting the review before that tail and filling it in after,
  would move the Files drawer and the first diff seconds earlier. Both change
  the provider's read model and the snapshot's acceptance, and are a design
  question for their own card.
- The first pull request of a session still admits its Git after acceptance;
  admitting it while the GitHub reads run would take that second off as well.
- The Files drawer and the comparison still wait for the accepted review and
  its epoch; only the Overview's list is provisional.
