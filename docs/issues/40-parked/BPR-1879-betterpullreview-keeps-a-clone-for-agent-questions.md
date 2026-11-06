---
schema: "implement-rfc.card.v1"
id: "BPR-1879"
title: "BetterPullReview can keep its own clone for agent questions"
status: "Parked"
parked_on: "human-review"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"feature","found_by":"split from BPR-1873 AC4 (owner decision 2026-11-05)","related_cards":["BPR-1873"],"builds_on":"BPR-1873 (merged to main at fbe94102e; parked only on its own look, accepted in session 235e4b41)"}
acceptance: [{"criterion": "AC1 Choosing the option creates the clone once, with the stated size and location, cancellable, and the question continues when it finishes", "outcome": "pass", "revision": "04cb14e56", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(gh_cli_git_smart_transport|application_clone_link_host|qt_agent_setup_view)$'", "detail": "Real Git through the fake gh: the kept clone is made through the sparse lease with every object and no checkout, owner-private, moved into agent-clones-v1/github.com-42 in the profile; a second request for an existing folder is refused; Cancel during a running clone returns cancelled within seconds and leaves nothing. The host offers the location and GitHub's size before it starts, publishes bytes received, links the clone (Managed) when it is in place so the Repository step is done and the setup moves on, links one already in place without a download, and reports a failure or a review that cannot keep one. The view says about 126 MB and the location, shows 42 MB of about 126 MB with Cancel, and Try keeping one again after a failure."}}, {"criterion": "AC2 Later questions reuse it and fetch only deltas", "outcome": "pass", "revision": "04cb14e56", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(gh_cli_git_smart_transport|application_clone_link_host)$'", "detail": "With the kept clone made at the base, the head's workspace delta borrowing it holds exactly the head's new commit, tree and two blobs; a new window over the same store links the kept clone with no download. The kept clone itself is not fetched into (see Left)."}}, {"criterion": "AC3 It is shown in Storage and removed by profile purge", "outcome": "pass", "revision": "04cb14e56", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(sqlite_insights_storage|sqlite_release_maintenance_files)$'", "detail": "Storage reports Clones kept for the agent with the kept clone's bytes, inside an exact total; a purge inventory takes a kept clone (.git only, read-only pack included) and removes agent-clones-v1, refuses a checked-out file or a dot-named folder in it, and keeps the shared registry."}}, {"criterion": "AC4 The owner accepts the option's look (attended review)", "outcome": "pending", "revision": "04cb14e56", "evidence": {"kind": "observation", "detail": "Attended session 68223c0c601a899da1ced7ad64b1476f, candidate BPR-1879-keep-a-clone-v002 at cf7c7bdde (offscreen renders: offer, downloading, past GitHub's size, failed; light and dark), one Accept/Reject question. v001 (fb4d6822) holds a Reject that was a note misdirected from BPR-1878, not a verdict on this look."}}, {"criterion": "AC5 The documentation gate passes", "outcome": "pass", "revision": "04cb14e56", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .", "detail": "DOCUMENTATION_OK with the RFC-BPR-0092 amendment's as-built note."}}]
---

# BPR-1879: BetterPullReview can keep its own clone for agent questions

Split from BPR-1873, which landed the linked-clone delta fetch, the PR-only
workspace and the Repository step's choice (link a clone / this pull request
only). This card adds the third option the owner asked for: **Let
BetterPullReview keep one**.

## Change

- Add the option to the Repository step, beside linking a clone; state the
  approximate download from GitHub's repository `size` and the location on
  disk.
- A full clone (RFC-BPR-0092 amendment: not blobless, so no lazy blob fetch
  can run in the agent's sandbox), owned by the profile under its application
  data, made in the background with progress and Cancel, outside the
  transport's temporary-store limits, through BetterPullReview's own gh
  credential and pinned private Git configuration.
- Later questions use it as the workspace's alternate exactly like a linked
  clone (BPR-1873's delta fetch), keeping it current by fetching into it.
- It appears in Insights › Storage and is removed by profile purge.

## Acceptance

- AC1. Choosing the option creates the clone once, with the stated size and
  location, cancellable, and the question continues when it finishes.
- AC2. Later questions reuse it and fetch only deltas.
- AC3. It is shown in Storage and removed by profile purge.
- AC4. The owner accepts the option's look (attended review).
- AC5. `python3 scripts/verify_documentation.py --root .` passes.

## Progress

### Before the card: the combined merges (2026-11-05)

Main at `b7ca30414` did not build: `git_cache_contract_tests` still passed
`std::nullopt` as the workspace source after BPR-1873 changed
`materialize_agent_workspace`. `786f90c7d` (Repair the combined agent-tab
merges) passes an empty source; with it the whole dev preset builds and the
agent-tab tests the merges touched pass.

### Built (`04cb14e56`)

- **Repository step.** For a sparse review read through a `gh` lease,
  **Let BetterPullReview keep one** sits between linking a clone (still the
  default button) and **Ask about this pull request only**, with "Downloads
  about SIZE from GitHub once, in the background, and keeps it in LOCATION.
  Later questions fetch only what is new." SIZE is GitHub's repository
  `size`, now read from the existing repository read, in decimal units.
  While it runs: "Downloading OWNER/NAME for the agent: X of about SIZE", a
  progress bar and **Cancel**. After a failure: why, and **Try keeping one
  again**. `frontend-design:frontend-design` was loaded first.
- **Download.** `clone_link_host::keep_clone` prepares the job on the
  decision lane, bound to the review on screen, and runs it on its own
  worker; `epoch_application_host::keep_managed_clone` uses the review's
  lease; the transport's `acquire_managed_clone` runs `gh repo clone
  --no-checkout` of every object into a private temporary root with the
  pinned private configuration and identity checks, on a runner outside the
  32 MiB / 96 MiB limits, checks `HEAD`, makes it owner-private and moves it
  to `agent-clones-v1/HOST-ID`. `architecture_boundaries` pins the new
  runner factory. (First built with ceilings of 4 to 64 GiB and two hours;
  see below.)
- **Reuse.** The kept clone is linked as the repository's clone (marked
  `Managed` from its location, not stored), so later questions borrow it as
  BPR-1873's linked clone does. Settings names it "kept by BetterPullReview".
- **Storage and purge.** Insights › Storage measures `agent-clones-v1`;
  profile purge removes it; the resets keep it.

### No download limit (owner decision 2026-11-05)

"I don't really care about a limit, a cancel and a progress would be
better." The kept clone's runner now has no byte or time limit: no per-file
ceiling, no store threshold, no deadline (`make_github_cli_managed_clone_process_runner(aGit)`;
`ManagedClone` lifts the deadline, and `architecture_boundaries` pins both
and keeps every other runner's limits). The qualification, identity and
`HEAD` check launches keep the usual bounds over their own empty folder, so
supervising them never counts the clone. GitHub's size no longer reaches the
transport; it is only what the step says.

- Progress: bytes received, sampled twice a second, of about SIZE; past
  SIZE, or with no SIZE, the bar is busy and the step says "X so far (GitHub
  said about SIZE)". Objects and percent are not shown: they exist only in
  Git's standard error, which the adapter never exposes.
- Cancel is honoured before it starts, during qualification, during the
  clone and before the final move; each leaves nothing in the profile.
- Evidence: `gh_cli_git_smart_transport` (the fake writes 40 MiB, past a
  sparse read's 32 MiB, over two seconds before cloning: the clone succeeds
  and at least three progress reports reach 40 MiB; a stop before it starts,
  one 30 ms in, and one during the clone are each a cancel that leaves
  nothing), `qt_agent_setup_view` (the past-the-size state),
  `architecture_boundaries`. Its ctest timeout is 120 s, up from 90 s, for the
  kept clone's course.

### Shorter copy (after the v002 reject)

`frontend-design:frontend-design` loaded again. Every state of the
Repository step's no-clone options now has a short label and at most one
short line:

- Offer: **Keep a clone for me**, then "About 126 MB, kept in ~/Li…/github.com-42"
  (the location shortened in the middle to one line; the full path is the
  tooltip; "Kept in …" when GitHub gave no size).
- Downloading: one row, a bar, "42 MB of 126 MB" and **Cancel**; past the
  size or with none, a busy bar and only the bytes ("131 MB").
- Failed: "No connection to GitHub." (or "GitHub refused the sign-in.",
  "Too large to keep.", "Needs a GitHub CLI sign-in.", "Download failed.")
  and **Retry**.
- Neighbours (BPR-1873's): **This pull request only** with "Weaker answers:
  no callers, tests or history."; the check's fallback is **This pull
  request only, for now**.

`qt_agent_setup_view` asserts each label and line exactly.

### Attended review

- v001: session `fb4d6822d9af23fc7264361af7b46b53`
  (`/Users/jaeger/.mask/bpr-1879-v001-20261105/attended-review`), candidate
  `BPR-1879-keep-a-clone` at `04cb14e56`. Its recorded Reject ("still not
  enough, i need the ability to pick effort level. It needs to bottom setting
  picker from agents tab too.") was a misdirected note meant for BPR-1878
  (owner, via the coordinator, 2026-11-05); it is not a verdict on this
  look, and the owner asked for the look to be put up again. Host stopped.
- v002: session `68223c0c601a899da1ced7ad64b1476f`
  (`/Users/jaeger/.mask/bpr-1879-v002-20261105/attended-review`), candidate
  `BPR-1879-keep-a-clone-v002` at `cf7c7bdde`, the same single
  Accept/Reject question with fresh renders including the past-the-size
  progress state. **Rejected** by the owner: "Those frontend ux designs are
  way too worded and verbose". Host stopped.

Offscreen renders of the setup view: the fixture-backed frozen build does
not read its reviews sparsely through `gh`, so it cannot show this option.

### Left

- AC4, the owner's look.
- The kept clone is not fetched into: the card's "keeping it current by
  fetching into it" is not built. Each question fetches what the kept clone
  lacks into the workspace instead, so AC2 holds, but on an active
  repository that delta grows with the clone's age and will eventually meet
  the 32 MiB per-file limit (`too_large`). Fetching into it needs a local
  write into the profile's clone after the delta (index the pack and record
  a ref under the same isolation), which is its own change; recommended as
  a follow-up card.
- Not exercised live on BPR-Test: the fake `gh` runs real Git against a
  local source. A live full clone of `MikeMarcin/BPR-Test` through the app
  is the remaining direct check.
