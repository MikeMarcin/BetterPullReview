---
schema: "implement-rfc.card.v1"
id: "BPR-2300"
title: "Threads rows show a clean one-line excerpt, read as Discuss reads a closed note"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","reservation":"docs/issues/_reservations/BPR-2300--BPR-2300--ffe15f19a70d.json","origin":"owner chat 2026-11-19 (\"We did quite a bit of work to make discuss look better recently but I feel like threads is now lagging behind. I think it has a lot of the same issue of all the markdown getting squashed onto a single line that we fixed in discuss. I still do want each thread to be one line or so.\")","related_cards":["BPR-1293","BPR-1341","BPR-1544","BPR-1584","BPR-2171"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2300: Threads rows show a clean one-line excerpt

## The owner's words (2026-11-19, verbatim)

"We did quite a bit of work to make discuss look better recently but I feel
like threads is now lagging behind. I think it has a lot of the same issue of
all the markdown getting squashed onto a single line that we fixed in
discuss. I still do want each thread to be one line or so."

## What was wrong

A Threads row (`conversation_finder.cpp`) drew its first post's body after
its Conventional Comments header with every line break and space collapsed
(`QString::simplified`) and only backticks read. A heading's `#`, a fence's
```` ``` ````, a suggestion's code, list markers, `>` quote prefixes, HTML
comments, image and link markup were all printed on the one line.
[[BPR-2171]] had fixed exactly this for Discuss's closed notes.

## What changed

- A row's words are the post's excerpt from Discuss's own preview
  (`detail::preview_of` in `conversation_preview.cpp`, which reads the body
  through the same GitHub-dialect import the open card uses). No second
  Markdown reader was written. The excerpt is the first paragraph (a leading
  heading in semibold, then its paragraph; a leading list's first item),
  emphasis and link marks removed and their words kept, inline code in the
  code font on a faint ground, and one quiet token per kind of block that is
  not words (List, Quote, Image, Table, Details, Suggestion, or the code's
  language), drawn as Discuss draws them. A body that is only a block shows
  only its token, so a lone suggested change reads "Suggestion".
- The row stays one line (its height is unchanged). The words are cut at a
  whole word with an ellipsis, measured by advance, never inside inline code
  unless the code opens the line, and the ellipsis takes the place of a last
  stop. Tokens are never cut; the words give way to them. A body with more
  paragraphs after the first ends in an ellipsis even when its first fits.
- The cutting and the token chips are shared in `conversation_preview`
  (`preview_runs`, `fit_preview_line`, `preview_tokens_width`,
  `paint_preview_tokens`) for any one-line surface. Discuss's own wrapping
  `clamp_lines` was left in place: Discuss's width code is being changed by
  another card at the same time.
- The excerpt is made the first time a row is drawn and kept while the post's
  words are the same, so a large pull request does not read every body when
  Threads is built.
- The search still washes its first match, now in the words shown.
- The row's accessible text reads the excerpt, not the Markdown source.
- Discuss's preview fast path (a one-line body with no Markdown character)
  now sends a body holding a `:name:` emoji through the GitHub import, so a
  one-line "Ship it :tada:" shows its emoji in Discuss and in Threads alike.

Core and Qt: the excerpt is decided in the Qt adapter, as [[BPR-2171]]
decided it for Discuss, because it reads Qt's GitHub-dialect Markdown import.
Moving it into core would need a Markdown parser in core, which
`src/core/AGENTS.md` asks the owner about first. No core change was made.

## Before and after (invented text)

| Body | Before | After |
|---|---|---|
| `## Backoff` / paragraph with `**doubles**` and a link / a list | `## Backoff The delay **doubles** each time, see [the notes](https://…). - one - two` | **Backoff** The delay doubles each time, see the notes. `[List]` |
| A lone ```` ```suggestion ```` fence | ```` ```suggestion const auto delay = std::min(aDelay * 2, aCap); ``` ```` | `[Suggestion]` |
| `> Is the cap ever zero?` then a reply with `` `retry_cap()` `` | `> Is the cap ever zero? No, retry_cap() is at least one.` | No, `retry_cap()` is at least one. `[Quote]` |
| An HTML comment, an image, then "settle :tada:" | `<!-- bot: keep --> ![chart](https://…) The chart shows it settle :tada:` | The chart shows it settle 🎉 `[Image]` |
| A long `nitpick:` paragraph | cut mid-word by `elidedText` | Prefer `std::chrono::milliseconds` over a bare integer here, … whether the delay is counted in… |

## Acceptance

- AC1 Excerpt: Threads rows read the first post as Discuss's closed note
  does: no `#`, fences, list markers, `>` prefixes, HTML comments, image or
  link markup; code spans in the code font; block tokens; white space
  collapsed; emoji names drawn.
- AC2 One line: a row with a rich Markdown body is as tall as a plain one, cut
  at a whole word with "…", inline code whole.
- AC3 Discuss unchanged but for the emoji fast path; ordinary Threads rows,
  search, and the window that hosts Threads pass as before.
- AC4 Gates: format and tidy clean on the changed files; documentation gate.

## Evidence (2026-11-19)

- `qt_conversation_preview`: `test_threads_excerpts` (heading and its
  paragraph's lines joined, heading alone, lone suggestion, leading fence by
  language, bullet and number markers, lone quote, link text and autolink,
  HTML comment, plain-line emoji, image alone, white space, inline code and
  emphasis, runs split at code) and `test_fit_one_line` (fits whole, omitted
  words, whole-word cut, last stop, never inside code, code opening the line).
- `qt_conversation_finder_scale_100`, `_125`, `_200`:
  `test_rows_show_one_clean_line` builds five rich-Markdown threads and a
  plain one and checks, at each scale, the words each row shows
  (`row_words`), its accessible text, its height against the plain row's, and
  the whole-word cut at 1000 and 640 pixels. Renders
  `conversation-finder-<scale>-markdown-rows.png` were looked at at 1.0 and
  2.0. The rest of the suite (search, filters, renders) passes unchanged.
- `qt_conversation_cards_view` (Discuss), `qt_workbench_window_scale_100` and
  `_200` pass.
- Not run: `application_conversation_read_public_path` and the full
  application. The disk had about 1 GB free after the targeted build.
- Format (clang-format 22) and clang-tidy 22 on the six changed C++ files:
  clean. `scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.

## Owner look

None was prepared. A frozen fixture build needs a full application build,
and the disk could not hold one this round. If the owner wants to judge it,
the next round can freeze a tests-on build with the fixture agent and a
fixture holding rich-Markdown threads. Ask one Accept/Reject question: "Threads
rows read like Discuss's closed notes, one line each".

## Not changed

The hover preview of one post in Threads (its popup) still wraps the body's
source text. It is a separate surface and was out of this card's scope.
