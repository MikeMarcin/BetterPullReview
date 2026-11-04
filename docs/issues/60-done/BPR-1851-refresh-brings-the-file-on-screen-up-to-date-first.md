---
schema: "implement-rfc.card.v1"
id: "BPR-1851"
title: "A pull request's refresh brings the file on screen up to date first"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"performance","found_by":"Owner, 2026-11-03: \"Is there anyway to prioritize refreshing the current file on a PR refresh? I think you get the theme of the next update by now: I'm trying to make the app more responsive. Less waiting around for loading\"","reservation":"docs/issues/_reservations/BPR-1851--BPR-1851--915cd8e26cde.json","related_cards":["BPR-1447","BPR-1575","BPR-1576","BPR-1841","BPR-1842","BPR-1848"],"frontend_design":"frontend-design:frontend-design loaded before the presentation change; no new visual language: the review's refresh veil gives way to BPR-1576's accepted partial plate once the file is fresh. Offscreen renders inspected; the plate's words were shortened and the plate given room so it no longer elides its last letter.","measurement":"Release (qualification-release), macOS arm64, offscreen, public-path fixture holding every read 330 ms (BPR-1842's measured gh round trip); before at 3457b3a02 and after at d475b2bee on the same course, three rounds each."}
acceptance: [{"criterion":"AC1 The file on screen and its threads are up to date before the rest of the refresh ends","outcome":"pass","revision":"d475b2bee","evidence":{"kind":"command","command":"build/qualification-release/tests/end_to_end/refresh_current_file_public_path --round-trip-ms 330 --measure --rounds 3; ctest --preset qualification-release -R '^application_refresh_current_file_public_path$'","detail":"A reply posted on the open file's thread is on its conversation at 2,008/1,997/1,997 ms after Refresh (before: 6,018/6,023/6,025 ms, only when the refresh ended). The refresh still ends at 5,980-6,010 ms and reads the same 18 requests. The course fails unless the reply arrives a round trip before the refresh ends."}},{"criterion":"AC2 The file stays on screen and is not washed over once it is up to date","outcome":"pass","revision":"d475b2bee","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(application_refresh_current_file_public_path|application_refresh_feedback_public_path|qt_pending_veil|qt_refresh_feedback_window)$'","detail":"No sample in any round lost the comparison. The light wash and rule run from 400 ms until the file is fresh (before: until 6,000 ms); from then the review's foot shows the partial plate 'Refreshing comments and checks…'. BPR-1575's refresh course (new reply counted, note words, failure and rate limit) passes unchanged."}},{"criterion":"AC3 Core decides, host performs: one conversation per refresh, same head only, no authority","outcome":"pass","revision":"d475b2bee","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(core_github_window_read|application_github_window_read_host|github_paged_read|application_body_digest_port)$'","detail":"core::github_window_read::accept_conversation_observation admits one per pending refresh, only after its file list, never on a first open, a cancelled or stale request, or when the facts show threads unread or another pull request, head, base or mode; lane state is unchanged. The host lays it over the accepted snapshot without a new snapshot id or an epoch handoff and publishes once; a moved head lays nothing. The provider observes it only for a refresh, right after the threads' last page, equal to the completed snapshot's reviews and threads. assemble_feedback_body_digests reproduces the whole snapshot's digests from the two sets."}},{"criterion":"AC4 Neighbouring courses and gates","outcome":"pass","revision":"d475b2bee","evidence":{"kind":"command","command":"ctest --preset qualification-release -R 'refresh|conversation|feedback|reply|resolution|github_window|digest|paged' plus architecture_boundaries, test_network_isolation_manifest, qt_composed_github_scale_100, qt_workbench_window_scale_100, progressive_open, pending_states, home_return; _coding-standard/scripts/format.py; _coding-standard/scripts/tidy.py --paths <changed TUs>","detail":"76 courses pass. clang-format 22 applied to every changed file. clang-tidy 22 over the six changed translation units other than github_application.cpp reports one finding, workbench_window.cpp:12916 finish_choice naming, a line BPR-1848 recorded and this card did not change; github_application.cpp (one changed condition) was not tidied."}}]
---

# BPR-1851: A pull request's refresh brings the file on screen up to date first

## Report

The owner, 2026-11-03: "Is there anyway to prioritize refreshing the current
file on a PR refresh? I think you get the theme of the next update by now: I'm
trying to make the app more responsive. Less waiting around for loading"

## Measured

A new course, `application_refresh_current_file_public_path`, opens #17 from
Home through the composed application on the public-path fixture, reads its
first file in Review, has someone reply on that file's thread
(`refresh_outcome::new_reply`), and presses Refresh. The fixture holds every
read of the refresh for a set round trip (330 ms, as [[BPR-1842]]). The
course prints when the reply was on the file's conversation, when the refresh
ended, when the review was washed, when the partial plate showed, how many
samples lost the comparison, and what the refresh read. `--measure` only
prints; `--rounds N` repeats; `--render-directory` saves two renders.

Release, milliseconds from Refresh, three rounds:

| | Before | After |
|---|---|---|
| Reply on the file's conversation | 6,018 / 6,023 / 6,025 | 2,008 / 1,997 / 1,997 |
| Refresh ended | 6,018 / 6,023 / 6,025 | 6,010 / 5,980 / 5,996 |
| Review washed | 400 to 6,000 | 400 to 2,000 |
| Comparison off screen | 0 samples | 0 samples |
| Reads | 18 | 18 |

## Root causes

- **Nothing reached the screen until the whole read was accepted.** A
  refresh reads the repository, the pull request, files, commits, reviews,
  the review threads, issue comments, search, checks, statuses, five
  merge-requirement reads, the timeline and the merge base one after another
  on the `gh` lane. The threads, the only part the file on screen shows, are
  complete after the sixth read (~2 s), but they waited in the staged
  snapshot for the other twelve (~4 s more).
- **The review was washed for the whole refresh.** The review's veil took
  BPR-1576's `refreshing` look (light wash and sweeping rule) from 400 ms
  until acceptance, so a file that was in fact current looked stale.
- Not causes: the comparison itself never left the screen, and a same-head
  refresh already kept the file's diff ([[BPR-1303]], [[BPR-1305]]).

## Built

- **Core** (`github_window_read`): `accept_conversation_observation(window,
  request, github_conversation_observation_facts)`. One per pending refresh,
  only after its file list observation, only for a refresh of the accepted
  pull request (`ExpectedRepository`/`ExpectedPullRequest` equal the lane's),
  and only when the facts say the threads were read and the pull request,
  head, recorded base and access mode are the accepted snapshot's. The lane's
  state and authority do not change.
- **Provider** (`perform_paged_github_read`): for a refresh, a third
  `overview_observed` right after the review threads' last page carries
  `Conversation` (reviews, threads, their availability, the lock), without
  authority.
- **Host** (`github_window_read_host`): the overview callback takes a third
  stage after the file list and digests its bodies on the provider's thread
  ([[BPR-1447]]). On the lane, `publish_conversation` copies the accepted
  snapshot, lays the reviews and threads over it, assembles its feedback
  digests from the two digest sets (`assemble_feedback_body_digests`, no
  hashing), validates it, asks the core, and installs it under the same
  snapshot id with the next markdown revision. It publishes a
  `PullRequest` projection with `PreserveWindowNavigation` and
  `ConversationRefreshed`, without a transition: no epoch handoff, no sibling
  share, no cached overview write. The refresh's own acceptance follows as
  before.
- **Composition**: the read-state, note-path and defect-name caches, keyed
  by snapshot id, are reset on a refreshed conversation as on an acceptance.
  The conversation panel, cards, Threads view, overlays and drafts take it
  through the existing PullRequest publication.
- **Qt** (`workbench_window`): while a refresh runs and its conversation is
  current, the review's veil presents BPR-1576's partial plate,
  "Refreshing comments and checks…", in place of the wash and rule.
  `pending_veil`'s partial plate keeps 4 px beyond its words' advance; at
  the exact width it elided the last letter (Home's plate too).
- The refresh note's change count ([[BPR-1575]]) still compares the
  baseline at the start of the refresh with the accepted end, so "1 new
  comment" is unchanged.

## Left open

- **A refresh whose head moved** shows nothing early: the threads anchor to
  the new head while the diff on screen is the old one's. Bringing the
  current file's new diff first means fetching that head's objects and
  diffing the current file before the rest, after acceptance; not measured
  here.
- **Reading the threads earlier.** On a refresh, threads could be read
  right after the file list, before commits and reviews, which would bring
  the file fresh at about 1.3 s instead of 2.0 s. It changes the provider's
  recorded read order and leaves the observation's reviews older than its
  threads, so it was not done.
- **Signed-out (public preview) reads** read inline comments over REST, not
  threads, and are not observed early.
- **Issue comments and the Overview's checks** still arrive with the
  acceptance; Discuss's general comments refresh then.
- **Reactions** on a reply that only the early conversation shows appear at
  acceptance.
- No owner look was requested: the change reuses BPR-1576's accepted partial
  plate and the review's words; renders were inspected offscreen.
