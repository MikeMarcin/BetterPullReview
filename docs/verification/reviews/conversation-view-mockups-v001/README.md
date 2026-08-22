# Conversation-view mockups, v001

The deliverable [[BPR-0211]] and [[BPR-0212]] were waiting on: a rendered mockup
of each alternative, presented as one choose-exactly-one question. Owned by
[[BPR-0413]].

These are mockups for a product decision. Nothing here is implemented,
composed, or reachable from the running application, and the batch declares
that as a false downstream claim rather than leaving it to be inferred.

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
| `render_mockups.py` | the generator; every colour and string in it is cited to live source |
| `mockups/01-today.svg` | the shipped conversations drawer, the baseline both alternatives are judged against |
| `mockups/02-whole-review.svg` | alternative A: every thread in the review, grouped by file |
| `mockups/03-treeverse.svg` | alternative B: a Treeverse-style map of one deep thread |
| `mockups/04-revision-join.svg` | [[BPR-0212]]: comments placed against the epoch axis |
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
| the BetterPullReview Light palette values | `src/adapters/qt/src/appearance_profile.cpp:76-103` |

## The conversation content is synthesized

No recorded pull request in this repository carries a conversation.
`tests/fixtures/github/pull-requests/directxmath-336/recording.json` and its
`uiforetw-74` sibling both have top-level keys `commits, files, pull_request,
recorded_from, recorded_on, repository` and nothing else. The only conversation
content in the tree is
`tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json`:
three threads of one, two, and one comments, carrying exactly one `replyTo`
edge.

So the deep thread in `03-treeverse` is invented, and the page says so on the
subject rather than in a footnote. That is [[BPR-0211]]'s own unmet
precondition — "a conversation deep or wide enough to be a problem is
recorded" — and the owner should weigh the alternative knowing it has never
been seen against real content.
