---
schema: "implement-rfc.card.v1"
id: "BPR-1802"
title: "Blob line permalinks in the pull request's repository show their lines as a snippet"
status: "Done"
rfcs: []
depends_on: ["BPR-1801"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1801--BPR-1802--7c41d29e0b85.json","kind":"feature","found_by":"owner, 2026-10-28","related_cards":["BPR-1801","BPR-1562","BPR-1806"],"related_rfcs":["RFC-BPR-0084"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "8e481c776ed6f6a96d9f1fa3725d2a562af4531c", "evidence": {"kind": "command", "detail": "qt_markdown_blob_snippet_view, through markdown_blob_snippet_host over a fixture file and the packaged classifier: a standing permalink to src/frame.cpp#L3-L7 becomes one box headed 'widgets/src/frame.cpp' then 'Lines 3 to 7 in 0123456', the title linking the permalink and the short commit linking https://github.com/octo-org/widgets/commit/<sha>; five rows numbered 3..7 holding the file's lines with syntax format ranges; the prose after stays and original_body is unchanged. core_markdown_blob_snippet: titles, 'Line 9' and 'Lines 3 to 5', seven-digit commit, commit page on the permalink's own scheme, host and port, labels with and without the owner."}}, {"criterion": "AC2", "outcome": "pass", "revision": "8e481c776ed6f6a96d9f1fa3725d2a562af4531c", "evidence": {"kind": "command", "detail": "core_markdown_blob_snippet: 17 places refused (in a sentence, list, quote, indented, fenced, ~~~ fence, HTML comment, <div>, <pre>, setext underlines, authored link, branch blob, no anchor, a body holding the private mark); a range past the file's end (GitHub keeps the link, checked on BPR-Test), binary, ill-formed UTF-8, oversized file or lines and an over-bound range give no lines. application_markdown_blob_snippet_host: another repository, another host and an over-bound range are refused with no read; absent files are remembered, unavailable ones asked again. qt_markdown_blob_snippet_view: another repository's and a mid-sentence permalink keep the compact link; the own repository's compact link drops its owner; without a repository nothing is asked or shown."}}, {"criterion": "AC3", "outcome": "pass", "revision": "8e481c776ed6f6a96d9f1fa3725d2a562af4531c", "evidence": {"kind": "command", "detail": "application_markdown_blob_snippet_host: one read per file shared by its ranges, a held file answering through a posted task, concurrency and waiting bounds with the oldest giving way, a byte-bounded cache, a replaced scope stopping its reads and dropping their late answers, reset and shutdown publishing nothing and leaving the host quiescent. qt_markdown_blob_snippet_view: an answer for other lines and a late answer after the body is replaced are ignored. The composed source reads through epoch_application_host::resolve_permalink_source on the review's cache actor (retained cache, or a sparse review's chain store and OID-checked lease read); application_review_round_trip_composition_contract passes with the resealed digests."}}, {"criterion": "AC4", "outcome": "pass", "revision": "8e481c776ed6f6a96d9f1fa3725d2a562af4531c", "evidence": {"kind": "artifact", "detail": "With BPR_PERMALINK_SNIPPET_RENDER_DIR set, qt_markdown_blob_snippet_view writes permalink-snippet-box- and permalink-snippet-link-{light,dark,high-contrast-light,high-contrast-dark}.png in place at 480 px; qt_tour_stop_card writes tour-stop-pinned-{theme}.png. Copies kept outside the repository in the session scratchpad (permalink-snippet-renders). The owner's own look is not part of this card's acceptance and is handed to the coordinator."}}]
---

# BPR-1802: Blob line permalinks show their lines as a snippet

Follow-up to [BPR-1801](BPR-1801-blob-line-permalinks-read-compactly-in-comments.md),
which made a bare blob line permalink read as one compact link
(`owner/repo/path#L10-L20 at 1a2b3c4`). GitHub goes further and embeds the
referenced lines.

## GitHub's rule (Markdown API, `gfm`, context BPR-Test, 2026-10-28)

- Bare and `<…>` permalinks with a full commit and an `#L` anchor, in the
  context repository on the same host, become a box: header link
  `repo/path` (no owner), then "Line 1" or "Lines 1 to 2", "in", the
  seven-digit commit linking the commit page; then a table of line numbers
  and lines. Mid-paragraph, the paragraph is split around the box.
- Another repository's permalink, an authored `[text](permalink)` and a blob
  address without a line anchor stay plain links.
- Checked again for this card: all 80 lines of an 80-line range were shown,
  and a range running past the file's end (`#L75-L90` of 80 lines) stays a
  plain link.

## Acceptance

- AC1. A same-repository permalink with lines on hand shows GitHub's box:
  header, range, short commit, numbered lines, syntax colours.
- AC2. Another repository, another host, an unreadable commit or path, or
  an over-bound range keeps the compact link; nothing is fetched out of
  scope.
- AC3. The fetch is bounded, cancellable and cached, and a late answer for a
  replaced body or context is dropped.
- AC4. Offscreen renders in every built-in theme exist outside the
  repository. (The draft's attended Accept/Reject is handed to the
  coordinator, as BPR-1806's was.)

## Design

Loaded `frontend-design:frontend-design`. The box follows GitHub's and the
reader's own Suggested change box (BPR-1564): the same 6 px rounding and
hairline, a header strip on the code background with the title semibold in
the link ink and "Lines A to B in" muted with the commit in the code face,
then the lines on the page's own ground with muted, right-aligned numbers
and no sign column, coloured as fenced code is.

## Built

- Core `markdown_blob_snippet`: `markdown_standalone_permalinks` (a
  permalink alone at the first column of a top-level line, outside fences
  and HTML blocks, not underlined), `markdown_blob_snippet_lines` and its
  bounds (100 lines, 1 MiB file, 64 KiB of lines, 8 boxes a body), header
  words, the commit page address, the owner-free label, and
  `markdown_with_blob_snippets`, which replaces each standing line with a
  fence whose info is a private-use mark and the box's ordinal; a body
  holding the mark shows no box.
- Application `markdown_blob_snippet_host` and `markdown_blob_source_port`;
  `epoch_application_host::resolve_permalink_source` reads a file at a commit
  from the review's own objects (retained cache, or a sparse review's chain
  store and lease), never fetching a commit or tree it does not hold.
  `github_pull_request_projection::Hostname` carries the accepted host.
- Qt: `markdown_body_view::set_permalink_repository`,
  `set_permalink_dispatch`, `apply_permalink_snippet`, `set_rendered_lead`;
  the code store draws snippet fences; the window, panel, cards and open
  card route requests and answers to the description and every post. A
  published tour stop's pinned source shows as its box between the plate
  and the narrative once read, and nothing before.
- Composition: the host over the epoch read, scoped by the accepted
  handoff's host, owner, repository, account and head; readers ask again on
  each commit-filter refresh. Round-trip composition digests resealed.

## Not done

- A commit the review's objects do not hold (a permalink to a commit outside
  the pull request's fetched history) keeps the compact link: there is no
  bounded GitHub contents read yet. Follow-up for the coordinator to file.
- A permalink inside a sentence, a list item or a quote keeps the compact
  link; GitHub splits the paragraph there.
- The tour authoring read view (BPR-1805) and the agent tab do not ask for
  snippets.

## Owner review

Owner accepted the look from the offscreen dark renders (snippet box and tour-stop pinned source) sent in the coordinator session, 2026-10-29: "Screenshots enough". No attended session was opened.
