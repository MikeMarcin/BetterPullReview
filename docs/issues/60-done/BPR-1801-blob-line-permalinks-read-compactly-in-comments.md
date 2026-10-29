---
schema: "implement-rfc.card.v1"
id: "BPR-1801"
title: "Blob line permalinks read compactly in comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1801--BPR-1802--7c41d29e0b85.json","kind":"bug","found_by":"owner, 2026-10-28","related_cards":["BPR-1796","BPR-1802"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "2378ee8bdaf6cf7278b5b42d860c9f893ff114af", "evidence": {"kind": "command", "detail": "core_markdown_navigation: one-line github.com permalink read part by part; Enterprise host with port, ?plain=1, columns, %20 path and reversed range put in order; 64-digit commit; 30 non-permalinks refused (branch, short commit, tree/raw, no or malformed anchor, dot, empty or encoded-separator segments, bad UTF-8, credential query, other schemes, userinfo, over-long); in-repository check case-insensitive and host-bound."}}, {"criterion": "AC2", "outcome": "pass", "revision": "2378ee8bdaf6cf7278b5b42d860c9f893ff114af", "evidence": {"kind": "command", "detail": "qt_markdown_github_1_0, _1_25, _2_0 check_permalinks: bare and <...> permalinks read 'MikeMarcin/BPR-Test/src/widget/frame.cpp#L93 at 0123456' and 'team/tools/docs/guide book.md#L10-L20 at 89abcde' with their full addresses as hrefs; an authored labelled link, a main-branch blob link and a code span unchanged; original body unchanged. qt_markdown_body, qt_markdown_github_html, qt_markdown_cmark_spec, qt_composed_markdown and qt_discuss_comment_bodies still pass; architecture_boundaries passes."}}, {"criterion": "AC3", "outcome": "pass", "revision": "2378ee8bdaf6cf7278b5b42d860c9f893ff114af", "evidence": {"kind": "artifact", "detail": "With BPR_MARKDOWN_RENDER_DIR set, qt_markdown_github writes in-place-permalinks-{light,dark,high-contrast-light,high-contrast-dark}.png at each scale; copies kept outside the repository in the session scratchpad (permalink-renders). The owner's own look remains with BPR-1802's snippet review."}}]
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
[BPR-1802](BPR-1802-blob-line-permalinks-show-their-lines-as-a-snippet.md).

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
  every built-in theme, read in place as the Comments drawer reads a post,
  outside the repository.

## Built

- `core::recognize_markdown_blob_permalink`, `markdown_blob_permalink_label`
  and `markdown_blob_permalink_in_repository` in `markdown_navigation`. The
  label keeps the owner because, without the pull request's repository, a
  permalink to another repository must not read as this one's; BPR-1802
  brings that scope and may drop it for same-repository links, as GitHub's
  snippet header does.
- The reader's GFM autolink pass (BPR-1796) gives a recognised bare
  permalink its label as link text and turns a `<permalink>` into the same
  labelled link. The destination, tooltip, admission and copied source are
  unchanged.
- Known gap, not changed here: `admit_markdown_link` refuses an address
  holding `%20`, so a permalink to a file name with a space reads compactly
  but does not open.
