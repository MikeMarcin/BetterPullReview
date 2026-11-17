---
schema: "implement-rfc.card.v1"
id: "BPR-2251"
title: "What changed since my review reads the reviewed code only when asked"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2251--BPR-2251--b7b70de4ba63.json","related":["BPR-2247","BPR-2248"],"acceptance":["BPR-2251:no-prefetch","BPR-2251:offered","BPR-2251:on-press","BPR-2251:refusal"]}
---

# BPR-2251: What changed since my review reads the reviewed code only when asked

## Report

The owner, 2026-11-17: "Ideally fetching those historical things doesn't
slow down our loading of the current state for reviewing". Approach chosen
by the owner: "Put it off until you need it is probably the right lazy
approach".

[[BPR-2247]] made What changed since my review fetch the reviewed commit,
its history and file contents when the review lacks them. The window read
that eagerly as soon as a returning pull request loaded, on the review's
serial cache actor.

## Scope

- Opening a pull request reads nothing of its past for What changed since
  my review.
- The question is offered whenever an earlier reviewed (or Seen) head
  differs from the current head.
- A press reads the change since the review, fetching what the review lacks
  as [[BPR-2248]]'s commit asks do, then asks.
- A fetch that fails is refused on the press with words that say why, not
  as a disabled menu item.

## Acceptance

- `BPR-2251:no-prefetch`: a returning pull request's current state loads and
  its files' diffs read while no historical read is issued; a test with a
  blocked historical fetch proves the load does not wait on it.
- `BPR-2251:offered`: the Pull Request menu and the Agent tab offer the
  question as soon as the earlier head differs from the head.
- `BPR-2251:on-press`: a press reads the change and sends it with the
  question.
- `BPR-2251:refusal`: a reviewed commit GitHub no longer serves refuses the
  press, saying it could not be fetched; nothing is sent.

## Findings

- Measured before the fix with `application_epoch_host`
  (`a_held_returning_fetch_leaves_the_review_actor_free`), a sparse review
  whose chain fetch the fake transport holds until released: the reviewer's
  own blob read (`materialize`, the diff worker's seam) was not done after
  2 s and sat queued behind the fetch (`reviewer-read-done=0
  actor-work-queued-behind-fetch=1`).
- Cause: every review read runs on the review's one serial cache actor
  (`profile_epoch_actor`, `src/application/src/profile_epoch_actor_registry.cpp`),
  and `resolve_returning_change` held it across the whole job, GitHub's
  chain-tree acquisition and exact blob reads included. The window started
  that job as soon as a returning pull request was accepted
  (`refresh_returning_review`), at reviewer priority, so the file diffs,
  preparation and permalink reads opened next waited behind the fetch. It
  shares no GitHub GraphQL slot with the snapshot read; the snapshot and
  ingestion themselves run before it is admitted.

## Fix

- Window (owner's choice): `refresh_returning_review` reads nothing of the
  past. `ask_what_changed` (Pull Request menu, Agent tab suggestion through
  the new `agent_tab_view::set_latest_upload_dispatch`, queued Ask) reads the
  change at the press, keeps it for those heads, then asks; a read that fails
  is refused on the Agent tab with the reason (could not be fetched, too
  large, cancelled, review changed, still loading).
- Core: `latest_upload_state` is `hidden` or `offered`, decided on the heads
  alone; `returning_material` and the Ask menu's `reading_commits` and
  `earlier_unreadable` reasons are gone. Qt: the menu item no longer changes
  its words, and the Agent tab's unavailable line is gone.
- Host: the change since a review runs in phases. Commit inspection, tree
  diffs and cache blob reads stay on the actor; the chain-tree acquisition
  and exact blob reads run on a worker of their own
  (`returning_change_off_actor`), so a slow or failing fetch holds no actor.
  [[BPR-2248]]'s commit asks share this path. Cancellation is unchanged: the
  job's stop token reaches the transport, and the window ignores an answer
  for other heads or a closed window.
- RFC-BPR-0116 records the owner amendment for AC02.

## Evidence

Lane commits `a7c38166d` (host) and `aff18f922` (window, core, Qt),
2026-11-17, macOS dev preset; full `cmake --build --preset dev`.

| Criterion | Check | Result |
|---|---|---|
| `BPR-2251:no-prefetch` | `application_epoch_host` (`a_held_returning_fetch_leaves_the_review_actor_free`: with the chain fetch held, the reviewer's blob read completes, `reviewer-read-done=1 actor-work-queued-behind-fetch=0`; the change resolves once released); every `application_returning_review_*_public_path` course opens a file's diff with `returning_change_reads == 0` | Pass |
| `BPR-2251:offered` | `core_returning_review` (`latest_upload_is_offered_before_anything_is_read`); the public path courses: the menu item is enabled and the Agent tab suggests it before any read | Pass |
| `BPR-2251:on-press` | `application_returning_review_latest_public_path` (one read at the press, the answer carries the change, `changes=1`); `application_commit_asks_public_path` (the shared host path) | Pass |
| `BPR-2251:refusal` | `application_returning_review_gone_public_path`: a press on the Agent tab suggestion is refused, "could not be fetched from GitHub", naming `0badc0d`; nothing launches | Pass |

Also passed: `core_quick_ask`, `qt_ask_menu`, `application_ask_menu_public_path`,
`application_explain_file_public_path`, `architecture_boundaries`, the
`agent_chat` suites, `qt_composed_github_scale_*`; the format check on every
changed C++ file; clang-tidy on the changed translation units reports only
findings on lines this card did not touch; documentation gate.

## Remaining risks

- `resolve_commit_filter_touches` (the Commits tab filter on a sparse review)
  still acquires its chain trees on the actor. It runs only when the filter
  is used, not at load.
- A press while the review is still ingesting is refused ("still loading");
  it is not queued until ingestion finishes.

## Owner decision (2026-11-17)

Refusing with "still loading" when the review hasn't finished loading is
correct. The owner: "Still loading is fine, it can even be a reason to grey out
an ask. It's better to say that and let the user do something else immediately
than lock them into waiting for it". A click never waits for loading. An ask may
also be shown disabled, with "still loading" as its reason.
