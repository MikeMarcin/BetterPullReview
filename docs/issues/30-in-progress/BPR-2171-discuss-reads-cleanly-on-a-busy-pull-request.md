---
schema: "implement-rfc.card.v1"
id: "BPR-2171"
title: "Discuss reads cleanly on a busy pull request"
status: "In Progress"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0085"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2171--BPR-2171--2de3ab5edceb.json","design":"docs/design/discuss-busy-pr-v1/index.html","origin":"owner-directed redesign, 2026-11-13; option B of the Discuss mockups with the owner's decisions"}
---

# BPR-2171: Discuss reads cleanly on a busy pull request

Amends the Discuss modality of [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and the closed note of [RFC-BPR-0085](../../rfcs/RFC-BPR-0085-drawer-design-system.md). Accepted design: [discuss-busy-pr-v1](../../design/discuss-busy-pr-v1/index.html), option B with the decisions below.

## The owner's words (2026-11-13, verbatim)

- "Can we update the design of the discussion tab a little. It's just a bit noisy on a busy pr."
- "it's only the collapsed cards with formatting issues, on a full screen window it just looks like a mess"
- Choosing B: "I think I like B although I dunno how you detect bots, and I think treating bot comments differently should be a quick filter at the top not always true. Do we need the ability to mark a user as a bot and store it as a repo setting?"
- Column width: "should be a setting, Centered I think."
- Threads under a review keep their author name and avatar: "yes it needs them".

Decisions recorded with them: bots are detected from GitHub's author type (a GitHub App's `[bot]` login) automatically, and any author can be marked or unmarked as a bot, stored as a per-repository local setting. Bot folding is a quick filter at the top, off by default, beside Hide resolved (which it does not replace). An open card may grow wider than the column for code blocks and suggested changes. The preview names a block with a word (List, Quote, Image, Table, the code's language, Suggestion). Review's Comments and Overview's Comments share the new preview. The "Pull request | File" choice becomes one "Only this file" toggle.

## Acceptance

- AC1 Collapsed preview: a closed note shows its first paragraph as plain prose, Markdown removed (headings, lists, emphasis, links, HTML, tables, images), code fences handled, one quiet word token per kind of non-prose block in order of appearance, an ellipsis only when words were left out, measured by advance. Discuss, Review's Comments and Overview's Comments share it.
- AC2 Readable width: Discuss's list is a centred column (820 by default) whose width is a setting; the header's chips align to the column's edges; reply, new and resolved marks keep fixed slots; an open card holding code or a suggested change may grow wider than the column.
- AC3 Each file is named once over its threads, each thread carries its line number, and threads under a review keep their author name and avatar.
- AC4 Folding: a file's resolved threads fold into a count on its row; runs of bot comments fold to one row when the Fold bots quick filter is on (off by default, beside Hide resolved). Bots: `[bot]` logins automatically, plus a per-author Mark as bot / Not a bot kept per repository on this computer.
- AC5 "Only this file" is one toggle replacing the "Pull request | File" choice.
- AC6 Native renders at 1280x800, 1728x1117 and 2560x1440, light and dark, on `conversations-v1`; one attended look, one Accept/Reject question per item 1 to 5.

## Design notes

- The collapsed preview is presentation (`conversation_preview.cpp`): Qt reads the body through the same GitHub-dialect import the open card uses (`import_github_markdown`), so no Markdown parser was written. A body with no Markdown character takes a fast path that only sets inline code. Code tokens use core's Linguist names (`markdown_code_language_of`). The preview is kept per card while its body is unchanged, not per width.
- The core timeline names each file once (`timeline_filters::Files`, `file` rows) and folds (`FoldResolved`, `FoldBots`, `Unfolded`) after the filters. A thread with a post not yet read, and a kept one, never fold. A thread asked for by name (a link, Threads' Discuss button, a key) opens its fold.
- Resolved threads fold in Discuss only; Overview's Comments names files once but folds nothing. Fold bots is a per-window quick filter, off by default and not remembered across pull requests.
- Bots: `[bot]` logins (GitHub's App accounts, as the read holds them) plus per-repository marks, kept in `state/v1/discuss.sqlite3` with the column width (Settings > Review > Reading > Discuss column: Narrow 680, Column 820, Wide 1100, Full width). A mark that repeats what GitHub says is removed. The thread menu offers "Mark <login> as a Bot" or "Mark <login> as Not a Bot".
- "Only this file" is one toggle; the file chooser appears beside it while it is on.
- A review whose own comment comes first carries its verdict and date on that comment's name line; its separate line is not drawn (its rail stays). A comment outside a review shows its date. Threads keep author and avatar and show their line beside the author.

## Progress (2026-11-13)

Frontend-design was loaded first. Lane commits `2037d395b` (allocation and design), `783017414` (core), `b83372963` (preview), `be6e66fbd` (timeline, panel, store and host), `fe748bd57` (composition, overlay, course), then the integration commit that records this section.

Evidence:
- Core: `core_conversation_timeline` (files and folds), `core_discuss_preferences`.
- Store and host: `sqlite_discuss_preferences_store`, `application_discuss_preferences_host`, `sqlite_profile_data_reset_files`.
- Qt: `qt_conversation_preview` (the design's six bodies), `qt_conversation_cards_view`, `qt_github_conversation_panel`, `qt_composed_markdown_1_0`, `qt_workbench_window_scale_100`, `qt_settings_window`, `qt_view_filter_memory_window`.
- Composed: `application_discuss_busy_public_path` serves conversations-v1 as a busy pull request (`src/app/src/discuss_busy_fixture.hpp`: four reviews, two runs of bot reports, a person to mark as a bot, a quoted reply) and checks the 820 column at three window sizes, file rows with folded resolved counts, the review's verdict and date on its own comment, dates on lone comments, Only this file, Fold bots (two rows, counts unchanged), Mark as Bot through the thread menu (a third row), and an open card with code wider than the column. `application_conversation_read_public_path` and `architecture_boundaries` pass.
- Native renders (Cocoa, `BPR_NATIVE_LOOK=1`, light and dark) at 1280x800, 1728x1117 and 2560x1440 from that course, kept outside the repository.

## Owner look (filed 2026-11-13, not waited on)

Session `277831a56c39ae71bdd20451783d4e33`, state `/Users/jaeger/.attended-review-state/discuss-busy-pr-v001-2026-11-13/state` (URL in `serve.log` beside it), cursor 15, host PID 26008 (Tailscale child 26018). Frozen root `/Users/jaeger/discuss-busy-pr-v001.tj2qtseq` from `c4a8137a55ee`. One Accept/Reject question per item: AC1 preview, AC2 column, AC3 files once, AC4 folding and bots, AC5 Only this file. AC6's renders are done; its look waits on these verdicts.

## Owner verdicts (session 277831a5, handed back 2026-11-13T22:59:49Z)

Verified in `session.json`, every decision by the human actor:

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `ac1-collapsed-preview` | **Accept** | 22:52:33Z | "Can switching to discuss from review with a comment open show that comment in disscuss?" |
| `ac2-readable-column` | **Accept** | 22:52:47Z | none |
| `ac3-files-once` | **Accept** | 22:53:32Z | none |
| `ac4-folding-and-bots` | **Accept** | 22:53:56Z | none |
| `ac5-only-this-file` | **Reject** | 22:59:48Z | "\"Only this file\" breaks the column formatting." (one screenshot: with the toggle on, the list and the file's name ran full width from the left edge) |

Host 26008 (Tailscale 26018) was stopped with the attended-review `stop`. The frozen root `/Users/jaeger/discuss-busy-pr-v001.tj2qtseq` is **kept**: the owner's app (PID 28776) was still running from it. Remove it with `freeze_attended_build.py remove` once that process has ended.

## Rework (2026-11-13)

Lane commit `8b16fbefb`.
- **(b) Only this file keeps the column.** The cause: the column applied only to the whole pull request's time-ordered list, so Only this file fell back to full width. Discuss's list is now a centred column in both. The column is centred on the whole list area whether or not a scrollbar shows, so a short list that loses its scrollbar no longer moves the column, or the band above it, by half a scrollbar. The file's name is drawn on the column's edge.
- **(a) Discuss from Review shows the open comment.** Choosing Discuss (the segment or ⌘3) while Review holds a published thread open opens that thread in Discuss, in view, the way Show Thread in Discuss does. Drafts and Local comments are not Discuss's and are left alone.
- Evidence: `application_discuss_busy_public_path` adds, at 1280x800, 1728x1117 and 2560x1440, Only this file on: the column equal to the whole pull request's (820, the same left edge), the band's toggle on the column's edge and unmoved (`ONLY_THIS_FILE 1728x1117 column=454+820 whole=454+820 toggle=454 was=454 edge=454`); then a thread open in Review is open and in view after choosing Discuss (`DISCUSS_CARRIED review-thread:PRRT_cv_t430`). The owner's screenshot shows (b)'s fault; the check's first run, with the column applied but centred on the narrowed viewport, still failed on the scrollbar shift (`column=230+820 whole=221+820`). (a)'s check was written with its fix.
- One question each for (a) and (b) is in the round's look session (see the [RFC-BPR-0127 ledger](../../verification/RFC-BPR-0127.md)). If both are accepted, this card can finish.
