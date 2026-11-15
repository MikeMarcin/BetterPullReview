---
schema: "implement-rfc.card.v1"
id: "BPR-2183"
title: "Local comments: identities from the store, Save with changed words in one transaction, anchors from the host's alignment"
status: "Done"
rfcs: ["RFC-BPR-0127"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2182--BPR-2183--8556cfbfdde3.json","origin":"critical architecture review of 03aae05cb (N3, N4, N5); follows BPR-2158","criteria":["AC01","AC02","AC03"]}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"19df539a9","evidence":{"kind":"command","detail":"sqlite_private_review_storage keep_local_and_make_draft (other words without Reworded move nothing; Reworded keeps the composer's words and ends the draft one revision on in the same commit); application_local_comments_composition_public_path (Save and Keep local through the composed window)"}},{"criterion":"AC02","outcome":"pass","revision":"19df539a9","evidence":{"kind":"command","detail":"sqlite_private_review_storage (two unnamed new comments take MAX+1 and MAX+2; an unnamed edit is refused); application_review_ledger_host test_a_new_note_takes_its_identity_from_the_store"}},{"criterion":"AC03","outcome":"pass","revision":"19df539a9","evidence":{"kind":"command","detail":"application_local_comments_composition_public_path (Local comments saved at their lines and kept local with anchors from shown_alignment, the diff host's accepted publication); window_local_comments.cpp reads no current_result()"}}]
---

# BPR-2183: Local comments: identities from the store, Save with changed words in one transaction, anchors from the host's alignment

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md),
following [[BPR-2158]]. From the critical architecture review of `03aae05cb`,
all in `window_local_comments.cpp`:

- **N3.** "Save as Local with changed words" ran as two store transactions
  sequenced in composition (an edit, then Keep local), and the composer
  closed before either answered.
- **N4.** A new Local comment's identity was minted as `max + 1` from the
  window's own snapshot, so two windows could mint the same one.
- **N5.** The composition read the comparison widget's alignment to build
  persisted line anchors.

## Acceptance

- **AC01 (N3):** Save as Local with changed words is one `keep_local` store
  transaction: `private_keep_local::Reworded` lets the comment carry the
  composer's words while the draft ends; without it other words are a
  conflict and nothing moves. The composer closes once the store has
  answered, and stays open with its words when the store refuses.
- **AC02 (N4):** A new Local comment with no identity takes the next free one in
  the store transaction that writes it; two in a row are numbered in order;
  an edit still names its comment. The review ledger host lets an unnamed new
  comment through and adopts the identity the store answers.
- **AC03 (N5):** Line anchors and the shown-comparison checks for Save, Keep local
  and Make draft read the alignment of the diff host's accepted publication
  for the selected comparison, not the comparison widget.

## Progress (2026-11-14)

Delivered in `19df539a9` (lane `claude/v0-5-0-prerelease-targets-52265d`):
`private_keep_local::Reworded` and the store's identity allocation
(`private_review_store.cpp`), the host's acceptance of an unnamed new comment,
`shown_alignment()` over `AcceptedDiff` from the diff host's publication, and
the composition's single `keep_local` call. Passing so far:
`sqlite_private_review_storage`, `application_review_ledger_host` and
`application_local_comments_composition_public_path`.
`application_local_comments_found_public_path` did not link: concurrent
builds in the shared `build/dev` corrupted the Qt adapter and core archives.
It exercises no changed path and is not counted as evidence.

## Notes

The reviewer's selected line, a gesture, is still read from the comparison
surface for Make draft on line N. Nothing the owner sees changes on success;
on a refused Save the composer now stays open with its words.
