---
schema: "implement-rfc.card.v1"
id: "BPR-1477"
title: "Keep a comment's retry when its pull request is read again"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1475"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1477--BPR-1477--58607c90b7ff.json","origin":"BPR-1475 implementation finding"}
---

# Keep a comment's retry when its pull request is read again

BPR-1475 rereads a pull request after the window publishes to it. While
building that, its worker found that rereading while a comment's outcome is
unknown breaks the retry. In that state the comment's draft is held
`publishing`, locked to the body it was sent with, and offers Retry, which
reconciles instead of posting again. With an automatic reread at that moment,
`application_inline_publish_cli_consent` crashed and
`application_label_publication_recovery` failed. BPR-1475 therefore does not
reread after an unknown outcome. Refresh can still be pressed in that state.

## Acceptance

- AC1: Pressing Refresh while a new comment's publication outcome is unknown
  reads the pull request again and keeps the draft held for the same retry.
  After the read, the draft is open in the drawer, locked to its prepared
  body, says why the result is unknown, and offers Retry. Retry reconciles
  the one stored comment with no second write. An end-to-end course proves
  this and fails on the old code.
- AC2: The fix is made where the decision is taken. The core decides which
  draft a reread holds again, and the composition restores the retry under
  the new composition. A comment held by a submitted review stays that
  review's to settle.
- AC3: Once the retry is preserved, the window's automatic reread also runs
  after a comment or reply's unknown outcome whose request went out. A retry
  refused before anything was sent does not reread.
  `application_inline_publish_cli_consent` and
  `application_label_publication_recovery` pass with that reread on.
- AC4: BPR-1475's courses and the neighbouring draft and publication
  contracts still pass, and the review round-trip composition contract is
  resealed.
