---
schema: "implement-rfc.card.v1"
id: "BPR-1801"
title: "Blob line permalinks read compactly in comments"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1801--BPR-1802--7c41d29e0b85.json","kind":"bug","found_by":"owner, 2026-10-28","related_cards":["BPR-1796","BPR-1802"]}
acceptance: []
---

# BPR-1801: Blob line permalinks read compactly in comments

Owner, 2026-10-28: a review comment whose body starts with a bare blob
permalink to a line (`https://<host>/<owner>/<repo>/blob/<40-hex>/<path>#L93`,
or `#L10-L20`) shows on GitHub as an embedded code snippet, but in the app as
a long, wrapped raw address. The host may be GitHub Enterprise.

GitHub's own rule, read from the Markdown API in `gfm` mode with a repository
context on BPR-Test (2026-10-28): a bare or `<…>` permalink with a full
commit and a line anchor, in the context repository, becomes a snippet box
headed `repo/path` and "Line 1" / "Lines 1 to 2" "in" the seven-digit
commit, even mid-paragraph. A permalink to another repository, an authored
`[text](permalink)` and a blob address without a line anchor stay plain
links.

This card lands the compact link. The fetched snippet is
[BPR-1802](../10-backlog/BPR-1802-blob-line-permalinks-show-their-lines-as-a-snippet.md).

## Acceptance

- AC1. Core reads a bare address as a blob line permalink only when it has a
  safe host (github.com or an Enterprise host, any port), owner, repository,
  `blob`, a full 40 or 64 digit commit, a non-empty path of safe segments,
  an optional `?plain=1` and an `#L<n>` or `#L<n>-L<m>` anchor (columns
  dropped, a reversed range put in order). Branch blob links, short
  commits, other views, missing or malformed anchors, dot or encoded
  separator segments, credential-bearing queries and other schemes are not
  permalinks. Core also answers whether a permalink is in a given
  repository on a given host, without regard to case.
- AC2. The reader shows a bare or `<…>` permalink as one link reading
  `owner/repo/path#L10-L20 at 1a2b3c4`, its destination still the whole
  address (activation unchanged through `admit_markdown_link`). Authored
  link text, branch links and code spans are untouched; the source is
  unchanged.
- AC3. Offscreen renders of a comment that starts with a permalink exist in
  every built-in theme, standalone and in place, outside the repository.
