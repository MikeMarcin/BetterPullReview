---
schema: "implement-rfc.card.v1"
id: "BPR-0656"
title: "Post read state through conversation receipts"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"A thread with posts after my cursor reads as unread in the cards and the gutter; Mark read advances the cursor durably and the states clear; the strip's control steps through unread threads in file order","outcome":"pass","revision":"44c11540 + this finish","evidence":{"kind":"observation","detail":"core_review_ledger pins unread_posts and mark_conversation_read; application_review_ledger_host pins one receipt load, the per-post unread projection, a Mark read that writes through the store before it is kept, a refused write that changes nothing, an edit reopening the seen post and an unchanged mark that is not a write; sqlite_review_ledger_storage round-trips receipts by scope with replacement and through the application port; qt_conversation_cards_view pins the unread flags, the count, Mark read and its dispatch, the file-ordered unread threads and the blue-mark lines; qt_workbench_window_scale_{100,125,200} pin the amber/blue gutter marks, the strip's count, stepping in file order with wrap, Mark read leaving through the conversation read dispatch, and an empty state withdrawing everything; application_review_ledger_composition_contract pins ReadStateObserved on the seed and reopen courses and its absence on the unavailable course."}},{"criterion":"core_review_ledger, application_review_ledger_host, application_review_ledger_composition_contract and the Qt selectors for the cards, strip and diff marks pass","outcome":"pass","revision":"44c11540 + this finish","evidence":{"kind":"observation","detail":"All named selectors pass at this head, with application_composition_contract, application_review_round_trip_composition_contract (digests repinned), the UIforETW public-path course and selected-diff contract, check_boundaries, strict tidy on the touched translation units, clang-format --Werror and verify_documentation.py."}}]
---

# Post read state through conversation receipts

RFC-BPR-0028's Read and Discuss surfaces draw unread posts with a blue dot
and full-ink names, step read posts back, offer Mark read, mark a new
thread's line blue in the gutter and walk the new threads from the strip's
"next new comment" control. None of that has an authority today:
`core::conversation_receipt` (the cursor of the last comment I saw in a
thread) and `core::unread(receipt, observation)` exist, and the SQLite
ledger has a `conversation_receipt` table since schema version 2, but no
port method, host or projection carries it, so [BPR-0648](BPR-0648-aligned-comment-cards-in-read.md)
draws every post in the same ink and offers no Mark read.

## Scope

- `review_ledger_store_port`: load and commit conversation receipts for the
  review's scope; the SQLite store implements them over the existing table.
- `review_ledger_application_host`: observe the conversations of the accepted
  snapshot against their receipts, decide per post whether it is newer than
  my cursor, publish a per-thread projection (unread count, first unread
  post) with the conversation projection, and accept a Mark read command
  that advances the cursor to the latest post.
- Qt: the cards' unread dot and dimming, Mark read on a card, the blue gutter
  mark for a thread whose root is unread, the strip's "next new comment"
  group with its count, and the same states in Discuss (BPR-0649).
- Owned paths: `src/application` ledger port and host, `src/adapters/sqlite`
  store, `src/app` composition, `src/adapters/qt` cards, strip and diff
  marks, their focused tests and the ledger contract.

## Acceptance

- A thread with posts after my cursor reads as unread in the cards and the
  gutter; Mark read advances the cursor durably and the states clear; the
  strip's control steps through unread threads in file order.
- `core_review_ledger`, `application_review_ledger_host`,
  `application_review_ledger_composition_contract` and the Qt selectors for
  the cards, strip and diff marks pass.

## Notes

- Slice, 2026-09-07. Core: `conversation_post_observation`, `unread_posts`
  (every post after the seen one, the seen one when edited, everything when
  nothing was seen, never a post of my own) and `mark_conversation_read`
  (the seen cursor moves to the latest post; acceptance stays). Port:
  `store_conversation_receipt` and `load_conversation_receipts` over the
  version-2 `conversation_receipt` table, bounded by
  `maximum_conversation_receipts`; every store (SQLite, the unavailable
  store, the test fakes) implements them. Host: `observe_conversations`
  loads the review's receipts once and judges each thread by the core;
  `mark_conversation_read` stores before it keeps, and an unchanged mark
  is not a write; three statistics. Composition: the ledger conversation
  identity is the durable half the defects already use for a thread, a
  post's identity is a half of its projection identity, and the edit
  version is a half of the body's SHA-256, so an edited post is a
  different version although the read model carries no edit time; my
  own posts are those whose author is the accepted snapshot's viewer.
  `publish_read_state` runs on every pull request projection and when the
  ledger opens; a card's Mark read reaches `mark_conversation_read` and
  republishes. Qt: `thread_read_state`, the cards' blue dot and full-ink
  name for an unread post with read names stepped back once a state is
  known, Mark read on a card with unread posts (a second action row when
  the card is narrow), `unread_marked_lines` for the gutter's blue mark on
  a thread whose first post is unread, and the strip's "Next new comment"
  (Alt+N, `strip_glyph::new_comment`) with its "n new" count after the
  file position, walking the unread threads of the whole pull request in
  file order from the current one and wrapping. The UIforETW probe pins
  the twenty-four-entry row; the review-ledger composition probe gains
  `ReadStateObserved` (the host loaded the receipts when the pull request
  opened), so the contract's evidence strings grew a digit.
- Not in this slice: the same states in Discuss (BPR-0649, which builds
  its list on `unread_threads_in_file_order`), threaded replies and
  resolve (BPR-0657), and an unread count on the comments rail, which
  reads the thread count until the rail is revisited with BPR-0649.
- Verification: `core_review_ledger`, `application_review_ledger_host`,
  `application_agent_invocation_host`, `sqlite_review_ledger_storage`,
  `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_github_scale_100`, `qt_diff_workbench_scale_{100,125,200}`,
  `application_review_ledger_composition_contract`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (both digests
  repinned), `application_github_public_path_uiforetw_74`,
  `application_uiforetw_selected_diff_contract`, `check_boundaries`,
  strict tidy on every touched translation unit, `clang-format --Werror`
  and `verify_documentation.py` pass.
