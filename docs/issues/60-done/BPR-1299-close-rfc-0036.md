---
schema: "implement-rfc.card.v1"
id: "BPR-1299"
title: "Verify and close RFC-BPR-0036"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-0917"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0036"]
metadata: {"kind": "status-only closeout", "predecessor_closeout": "BPR-0917", "milestone_plan": {"id": "rfc0036-native-markdown", "path": "docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}, "allocated": "2026-10-06"}
acceptance: [{"criterion":"AC-01","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"Overview and every open-thread post render the declared subset through the shipped composition: 17/17 focused rows at e16f78366 (qt_composed_markdown_* with the conversation structure assertions restored in 0a7f72068, failing-before control recorded on BPR-1290) and native Cocoa captures conversation-1/-2/-4/-5 and overview-native inspected. Closed-note one-line summaries are the navigation-summary contract accepted at a3ab46ee."}},{"criterion":"AC-02","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"original_body(), View source and Copy source per post and in the overview (qt_composed_markdown_*, qt_markdown_body_*); explicit fallback in source-fallback capture."}},{"criterion":"AC-03","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"Link identity through the window's admission per post and overview; hostile HTML and file: image inert in every capture; application_device_authorization_browser_address and application_setup_destination_public_path pass."}},{"criterion":"AC-04","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"macOS: Tab from the notes into each post's reader, reading to the end, source switching, link Enter (qt_composed_markdown_* at 1.0/1.25/2.0); light, dark and both high-contrast captures. VoiceOver/physical input/Windows/Linux not claimed, as at BPR-0917."}},{"criterion":"AC-05","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"Native Cocoa course: 50,742-byte body 34 ms, 1,027-byte pathological fallback 12 ms at DPR 2; unchanged refresh keeps selection (qt_composed_markdown_*, qt_markdown_body_*)."}},{"criterion":"AC-06","outcome":"pass","revision":"e16f78366","evidence":{"kind":"command","detail":"overview-long-final and overview-long-final-wide reach the final paragraph; docs/architecture/native-markdown.md names surfaces, fallback, bounds and image policy (updated in 0a7f72068)."}}]
---

# Verify and close RFC-BPR-0036

RFC base: `efa4388396128698e3cd737dd48efb9c364e5441`.

## Why this card exists

[[BPR-0917]] accepted RFC-BPR-0036's technical implementation and all six
criteria at `a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f` and left the RFC
Proposed, because RFC-BPR-0008 and the canonical predecessors were not yet
Auto-Verified. Its continuation named the remaining step: "reconcile
RFC-BPR-0036 ... after RFC-BPR-0008 and all required predecessors become
Auto-Verified. Do not change status now or repeat unchanged implementation
evidence merely because a predecessor later closes." BPR-0917 is Done, which is
terminal, and no open card owned that step.
[[BPR-1226]] keeps the review-tour pack's reconciliation against 0036 and says
the original acceptance "remains owned by their existing RFCs/cards"; it does
not close 0036. Found while closing RFC-BPR-0055 under [[BPR-1188]].

The RFC's own dependencies, RFC-BPR-0005, 0006 and 0008, are all
Auto-Verified. The owner removed the global canonical-order rule on 2026-09-27
(amendment r3), so dependency closure is the gate.

## Outcome and bounded work

Reconcile each criterion against BPR-0917's recorded evidence and the
[verification ledger](../../verification/RFC-BPR-0036.md). Check whether any
input to that evidence changed since `a3ab46ee` in a way that bears on a
criterion (the shared native reader, its link and image policy, the overview
and public-conversation presentation). Rerun only the affected focused checks.
If nothing bears on a criterion, set RFC-BPR-0036 and its index row to
Auto-Verified, record the result in the ledger and run DOCUMENTATION_OK.

Out of scope: new product work, VoiceOver certification, physical input,
Windows or Linux native qualification, and live services. BPR-0917 recorded
those as limits, not as failed criteria. Broader failures already owned by
[[BPR-0940]] stay with it.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links. | BPR-0917's recorded course; focused rerun only if the reader or its composition changed | Pass at `e16f78366`: overview and every open-thread post render the subset (native captures); closed notes stay navigation summaries as at `a3ab46ee` | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |
| AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback. | As above | Pass at `e16f78366`: `original_body()`, View source and Copy source per post and in the overview; fallback capture | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |
| AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path. | As above | Pass at `e16f78366`: link identity through admission per post; hostile HTML and file images inert in every capture | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |
| AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable. | As above | Pass at `e16f78366` on macOS: Tab from the notes into each post's reader, reading to the end, source switching; four appearances, three scales | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |
| AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content. | As above | Pass at `e16f78366`: 50,742 bytes in 34 ms, pathological 1,027 bytes in 12 ms (Cocoa, DPR 2); unchanged-refresh selection kept | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |
| AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy. | As above | Pass at `e16f78366`: `overview-long-final*` reach the final paragraph; `native-markdown.md` names the surfaces, fallback, bounds and image policy | `a3ab46ee`; product reconciled at `0a7f72068` (`e16f78366` tree) |

## Reconciliation result (2026-10-06): stopped at AC-01

Promoted at `5cbac5a6b`, claimed at `c012afeaa` over `origin/main`
`34c4b9d49`. The dependencies are met: RFC-BPR-0005, 0006 and 0008 are
Auto-Verified and BPR-0917 is Done. The reader and its composition changed
after `a3ab46ee`, so BPR-0917's focused course was run again on macOS
(`build/dev`, Debug). It passes 17 of 17, and so does the native Cocoa
course.

**What is missing.** Public conversations no longer render Markdown. Since
[[BPR-1290]] (`201c96f21`, the owner's "I like it" on the inline drawer), each
post is read in place "as written". The native captures `conversation-1`, `-2`,
`-4` and `-5` show `- [x]`, backtick fences, `| --- |` and `**…**` as typed.
That commit also removed `qt_composed_markdown`'s conversation structure,
source and keyboard-reader assertions, so the rows pass without checking it.
The overview still renders the subset.

- AC-01 fails for public conversations.
- AC-04's conversation half is no longer demonstrated: the conversation
  reader it was proven on is gone.
- AC-02's conversation half is moot while AC-01 fails.
- AC-03, AC-05 and AC-06 pass.

The details are in the
[ledger](../../verification/RFC-BPR-0036.md#status-only-close-under-bpr-1299-stopped-at-ac-01-2026-10-06).

**Owner question (one, for AC-01):** "Since BPR-1290, open-thread posts show
Markdown as written, so RFC-0036's 'public conversations render the declared
Markdown subset' fails. Choose one:
- **Render posts (Recommended):** render each open post with the shared
  reader, keeping the label marks and link host, in BPR-1290's next candidate.
- **Amend and close:** amend AC-01 and AC-04 to the overview and close."

The recommendation follows from the RFC's opening requirement: review
conversations "should display readable formatted content instead of exposing
Markdown punctuation as plain prose". BPR-1290 is already parked for the
owner's next look, so the fix can ride its next candidate. Once rendering is
back, rerun the focused selection and the native Cocoa course, inspect the
conversation captures, then close RFC-BPR-0036.

Parked on `human-review`. RFC-BPR-0036 stays Proposed. No product file changed.

## The fix landed in BPR-1290 v005 (2026-10-06)

The coordinator chose the first option, "Render posts", on 2026-10-06, since
the regression breaks an accepted RFC. [[BPR-1290]]'s candidate `7fb9997b1`
(v005) carries it:

- Every post in an open thread, in the drawer and in Discuss, reads through
  `markdown_body_view` in place (`set_in_place`). BPR-1083's marks, rails and
  semibold label header are kept. View source and Copy source are in the
  post's name row. Links go through the window's admission with the post's
  identity. Closed notes stay one-line summaries.
- `qt_composed_markdown`'s conversation half is restored against the drawer:
  `structures()`, no Markdown punctuation shown as prose, `original_body()`,
  Tab from the notes into each post's reader, reading to the end with the
  final line kept in the drawer, link identity, and View source and Copy
  source per post. Against the pre-fix product the rows fail at "every public
  body and reply reads in place through the shared Markdown reader".
- `docs/architecture/native-markdown.md` names the open-thread posts as a
  reader surface again.

Still open here, unchanged: AC-01 to AC-06 still have to be reconciled on the
published commit. **To rerun:** the focused selection from the reconciliation
above (17 rows, Debug) and the native Cocoa course
(`QT_QPA_PLATFORM=cocoa BPR_AUTOMATED_TEST_BOUNDARY=1`,
`better_pull_review_composed_markdown_tests` with `BPR_MARKDOWN_RENDER_DIR`).
Then inspect `conversation-1`, `-2`, `-4` and `-5` for rendered tasks, code,
tables and links, and close RFC-BPR-0036 if AC-01, AC-02 and AC-04 now pass.
This card stays parked until that run. BPR-1290's own owner look (v005) is
separate and does not gate this close.

## Reconciliation on the published fix, 2026-10-06: RFC-BPR-0036 closed

[[BPR-1290]] is Done; its v005 (`0a7f72068`) is on `origin/main`. Rerun in a
fresh worktree at `origin/main` `e16f78366` (the product tree; the commits
after it touch only `docs/issues`), macOS 26.6.2 arm64, `build/dev` (Debug),
with the shared CPM cache:

- The focused selection from the reconciliation above,
  `ctest --test-dir build/dev -j 6 -R '^(core_markdown_navigation|application_github_window_read_host|qt_pull_request_overview_page|qt_github_conversation_panel|qt_conversation_cards_view|qt_markdown_body_.*|qt_composed_markdown_.*|qt_public_preview_conversation|application_device_authorization_browser_address|application_setup_destination_public_path|core_dependency_boundary|architecture_boundaries|architecture_boundary_contract)$'`,
  passes 17 of 17. `qt_composed_markdown_*` now carry the conversation
  checks that `0a7f72068` restored (structures, no Markdown punctuation as
  prose, `original_body()`, Tab into each post's reader, reading to the end,
  link identity, View source and Copy source per post); BPR-1290 recorded
  their failing-before control on the pre-fix product.
- The native Cocoa course
  (`QT_QPA_PLATFORM=cocoa BPR_AUTOMATED_TEST_BOUNDARY=1 BPR_MARKDOWN_RENDER_DIR=<dir>`,
  `build/dev/tests/adapters_qt/better_pull_review_composed_markdown_tests`,
  SHA-256 `732a9d31077dd205…`) prints `composed-markdown-tests:pass`. The
  50,742-byte body lays out in 34 ms at DPR 2 and the 1,027-byte
  pathological fallback in 12 ms.
- **Captures inspected.** `conversation-1` (light), `-2` (dark), `-4` (high
  contrast light) and `-5` (high contrast dark) show the open thread's post
  rendered: heading, emphasis, strong, strikethrough and inline code, the
  quote, the ordered list with its nested bullet, checked and unchecked tasks,
  the code block, the bordered table with **Readable** in bold, the rule, and
  the links (web, repository, the two refusals and both image links as link
  text). `<script>` and the `file:` image show as inert text. View source
  and Copy source are in the post's name row. `overview-native` still renders
  the subset and `source-fallback` shows the explanation, the final line and
  a keyboard selection.
- **Not a failure.** In Discuss the two pull-request conversations above the
  file group are closed notes, and their one-line summaries show the first
  post's text as written (`**strong**`, `- [x]`, `| --- |`). That is the
  navigation-summary contract RFC-BPR-0036 was accepted under at `a3ab46ee`
  ("Compact entries remain navigation summaries; the selected detail owns the
  complete public body"), kept by BPR-1290 v005 ("Closed notes stay one-line
  summaries") and stated in
  [`native-markdown.md`](../../architecture/native-markdown.md#initial-surface-inventory-and-integration-contract).
  Opening one reads it through the shared reader.

AC-01 to AC-06 pass (table above). RFC-BPR-0036 and its index row are set to
Auto-Verified, and the [ledger](../../verification/RFC-BPR-0036.md) records the
run. Limits are unchanged from BPR-0917: no VoiceOver certification, no
physical input, no Windows or Linux native qualification, no live service.
