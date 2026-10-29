---
schema: "implement-rfc.card.v1"
id: "BPR-1802"
title: "Blob line permalinks in the pull request's repository show their lines as a snippet"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1801"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1801--BPR-1802--7c41d29e0b85.json","kind":"feature","found_by":"owner, 2026-10-28","related_cards":["BPR-1801","BPR-1562"],"related_rfcs":["RFC-BPR-0084"]}
acceptance: []
---

# BPR-1802: Blob line permalinks show their lines as a snippet

Follow-up to [BPR-1801](../30-in-progress/BPR-1801-blob-line-permalinks-read-compactly-in-comments.md),
which made a bare blob line permalink read as one compact link
(`owner/repo/path#L10-L20 at 1a2b3c4`). GitHub goes further and embeds the
referenced lines. Left out of BPR-1801 because it needs a host fetch lane
and plumbing to every reader surface, not only presentation.

## GitHub's rule (Markdown API, `gfm`, context BPR-Test, 2026-10-28)

- Bare and `<…>` permalinks with a full commit and an `#L` anchor, in the
  context repository on the same host, become a box: header link
  `repo/path` (no owner), then "Line 1" or "Lines 1 to 2", "in", the
  seven-digit commit linking the commit page; then a table of line numbers
  and lines. Mid-paragraph, the paragraph is split around the box.
- Another repository's permalink, an authored `[text](permalink)` and a blob
  address without a line anchor stay plain links.

## Shape

- Core: `recognize_markdown_blob_permalink` and
  `markdown_blob_permalink_in_repository` (BPR-1801) already decide. Add a
  bound on embedded lines (GitHub's own cap, to be checked) and the
  snippet's semantic projection (header text, line numbers, lines).
- Application: a permalink lane like `markdown_image_service`: the reader
  sends recognized permalinks in document order; the host checks scope
  against the accepted pull request's host, owner and repository under its
  account lease, reads the file at the commit through the Git cache port
  (`find_blob_at_path`, `materialize_blob`) when the commit is held, or a
  bounded GitHub contents read otherwise; bounded bytes, cancellable,
  cached by host, repository, commit and path, stale-context safe.
- Qt: the body view gets the pull request's repository scope (overview,
  conversation cards, open cards, drawer) and a dispatch like
  `set_image_dispatch`; it draws a snippet block (header with `repo/path`
  and the line range in the commit, monospaced numbered lines, syntax
  colours through the existing `markdown_code_syntax` dispatch), the header
  linking the permalink. While loading, failed or out of scope, the
  BPR-1801 compact link stays. With scope known, a same-repository compact
  link may drop the owner as GitHub's header does.

## Acceptance (draft)

- AC1. A same-repository permalink with lines on hand shows GitHub's box:
  header, range, short commit, numbered lines, syntax colours.
- AC2. Another repository, another host, an unreadable commit or path, or
  an over-bound range keeps the compact link; nothing is fetched out of
  scope.
- AC3. The fetch is bounded, cancellable and cached, and a late answer for a
  replaced body or context is dropped.
- AC4. Offscreen renders in every built-in theme, and the owner's look as an
  attended Accept/Reject.
