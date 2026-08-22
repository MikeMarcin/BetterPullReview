# Conversation-view mockups, v001

The deliverable [[BPR-0211]] and [[BPR-0212]] were waiting on: a rendered mockup
of each alternative, presented as one choose-exactly-one question. Owned by
[[BPR-0413]].

These are mockups for a product decision. Nothing here is implemented,
composed, or reachable from the running application, and the batch declares
that as a false downstream claim rather than leaving it to be inferred.

The first build of this batch was never presented. An independent review
falsified its central cost claim — "alternative B is one line" — along with the
feasibility concern on [[BPR-0212]] and the attribution of who narrowed the
git-graph request, and [[BPR-0413]] reopened to correct all three before the
page reached anyone. The corrected batch also carries a fifth mockup and a
fourth subject that the first one omitted entirely.

## Regenerating

The SVG sources are the artefact; the PNGs exist because the offline review page
previews images and not vectors.

```sh
cd docs/verification/reviews/conversation-view-mockups-v001
python3 render_mockups.py --out mockups
for f in mockups/*.svg; do qlmanage -t -s 1600 -o mockups "$f"; done
for f in mockups/*.svg.png; do mv "$f" "${f%.svg.png}.png"; done
```

`render_mockups.py` is standard-library only and deterministic, so the SVG bytes
reproduce anywhere Python 3.10+ runs.

Rasterisation is not portable and is not claimed to be. `qlmanage` is macOS
QuickLook, it resolves the system fonts named in the SVG font stacks, and it
letterboxes a non-square source — which is why the canvas is square. It was
byte-deterministic across repeated runs on the rendering host (same SHA-256),
but a different macOS version, font set, or WebKit build may produce different
bytes from the same SVG. The SVG is the reviewable source; the PNG is a
rendering of it.

Rendered on macOS 15 (Darwin 24.6.0), arm64, Python 3.14.6.

## Contents

| Path | What it is |
|---|---|
| `render_mockups.py` | the generator; product-surface colours and every label string are cited to live source, and the neutral page chrome is named as such |
| `mockups/01-today.svg` | the shipped conversations drawer, the baseline both alternatives are judged against |
| `mockups/02-whole-review.svg` | alternative A: every thread in the review, grouped by file |
| `mockups/03-treeverse.svg` | alternative B: a Treeverse-style map of one deep thread |
| `mockups/04-revision-join.svg` | [[BPR-0212]]: comments placed against the epoch axis |
| `mockups/05-lane-rails.svg` | the git-graph / lane drawer [[BPR-0191]] recorded the owner asking for, which was never built |
| `offline-review-input.json` | the batch input manifest |
| `offline-package/` | the built page: open `index.html` |

## What the mockups assert, and where it was checked

Every fact drawn was read from live source rather than assumed:

| Drawn | Source |
|---|---|
| the projection's fields, and that none of them is a revision | `src/application/include/bpr/application/github_projection.hpp:124-139` |
| every comment's parent is the thread root, so the tree is two levels deep | `src/application/src/github_projection_mapping.cpp:148,162,175,192` |
| the reply edge exists upstream and is discarded at projection time | `src/application/include/bpr/application/github_read_model.hpp:233` |
| heading / body / facts rows joined with `"  ·  "`, no lane rails | `src/adapters/qt/src/github_conversation_delegate.cpp:68-144` |
| the epoch label format and its cause and availability words | `src/adapters/qt/src/epoch_history_model.cpp:26-137` |
| authors render as `@login`, so every heading carries the `@` | `src/adapters/qt/src/github_conversation_delegate.cpp:44-45` |
| a parent is accepted only when its kind is `review_thread` | `src/adapters/qt/src/github_conversation_model.cpp:317-326` |
| a review's `commit_id` is required on read, then dropped | `src/adapters/github/src/github_read_contract.cpp:1214-1241`, `src/application/src/github_projection_mapping.cpp:145-156` |
| the git-graph request, and who narrowed it | `docs/issues/60-done/BPR-0191-shell-becomes-the-application-window.md:92-93,108-110,120-122` |
| the BetterPullReview Light palette values | `src/adapters/qt/src/appearance_profile.cpp:76-103` |

## The conversation content is synthesized

No recorded pull request in this repository carries a conversation.
`tests/fixtures/github/pull-requests/directxmath-336/recording.json` and its
`uiforetw-74` sibling both have top-level keys `commits, files, pull_request,
recorded_from, recorded_on, repository` and nothing else. The conversation
content that does exist is hand-written, in three contract fixtures —
`tests/fixtures/github/contracts/v1/{github-com,ghes-3.18,uiforetw-74}/responses/review_threads.json`.
The first two carry the same three threads of one, two, and one comments with
exactly one `replyTo` edge; the third carries none. Maximum depth anywhere is
one reply.

So the deep thread in `03-treeverse` is invented, and the page says so on the
subject rather than in a footnote. That is [[BPR-0211]]'s own unmet
precondition — "a conversation deep or wide enough to be a problem is
recorded" — and the owner should weigh the alternative knowing it has never
been seen against real content.
