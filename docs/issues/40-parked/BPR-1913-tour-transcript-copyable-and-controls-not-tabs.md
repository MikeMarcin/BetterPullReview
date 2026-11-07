---
schema: "implement-rfc.card.v1"
id: "BPR-1913"
title: "The tour session's transcript copies as a whole and leaves the tour's room, and its controls stop looking like tabs"
status: "Parked"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900"]
parked_on: "human-review"
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1878","BPR-1881","BPR-1892","BPR-1900","BPR-1912"]}
acceptance: [{"criterion": "AC1 The transcript no longer takes space in the Tour tab's editing column, and opens in one click from the Tour tab and from a failure notice", "outcome": "pass", "revision": "840b1c629", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Transcript (1) opens a separate window (isWindow) and the editing column holds no transcript; qt_review_tour_agent_failure: Show the run opens the window at the failed run. Offscreen look of the frozen build opens it from Show the run.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#verification"}}, {"criterion": "AC2 One action copies a whole run (and the whole session) as plain text, including a failed run's reason; a Qt test reads the clipboard", "outcome": "pass", "revision": "840b1c629", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view reads the clipboard after Copy run (the failed run with its reason and a not-run note) and Copy all runs (oldest first, with a ran-outside note). The frozen build's look prints ATTENDED_TOUR_COPIED with the whole failed run and its failure log.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#verification"}}, {"criterion": "AC3 Text selection spans entries", "outcome": "pass", "revision": "840b1c629", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: select all in the run view covers the first entry through the failure reason, and the view's Copy takes it.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#verification"}}, {"criterion": "AC4 No control on the Tour tab that isn't a tab is styled like a tab", "outcome": "pass", "revision": "840b1c629", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: Draft the tour is a bordered button at its own width; Transcript is flat, not checkable, with a glyph; no button of the session is checkable. Offscreen pictures of the frozen build, light and dark.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#implementation-2026-11-06"}}, {"criterion": "AC5 Owner look: an attended Accept/Reject on a frozen build that shows a failed run's transcript being copied, and the new controls", "outcome": "pending", "revision": "840b1c629", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/bpr1913-tour-transcript-v002.49fwr_6f of 840b1c629; new short session 3ffa1d56ef7825c2b6820979e01edd99, candidate BPR-1913-tour-transcript (revision 840b1c629-v002), one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#attended-review-ac5"}}, {"criterion": "AC6 The RFC-BPR-0092 T2 amendment is updated, and the documentation gate passes", "outcome": "pass", "revision": "840b1c629", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1900 T2 now records the transcript window and why a window; review-tours.md updated; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1913-tour-transcript-copyable-and-controls-not-tabs.md#implementation-2026-11-06"}}]
---

# BPR-1913: The tour session's transcript copies as a whole and leaves the tour's room, and its controls stop looking like tabs

## Owner report (2026-11-06)

"I don't like the way the transcript is in the tour window, when it failed I
couldn't copy it to the clipboard, except for one response at a time. And the
button at the top looks like a tab, except they're not tabs."

## Change

Load `frontend-design:frontend-design` first.

1. **The transcript leaves the Tour tab's main column.** BPR-1900 (decision
   T2) put the session's transcript in a panel in the Tour tab, and the owner
   doesn't like it there. Move it somewhere that keeps the tour's editing room
   clear and is still one click from the Tour tab, for example its own window
   or sheet, or a separate drawer surface. Recommend one, and amend
   RFC-BPR-0092's T2.
2. **Copy the whole transcript.** One action copies a whole run, or the
   whole session, to the clipboard as plain text (Markdown is fine): each
   step's kind, its command or path, and the answer, in order. It works
   especially when a run failed. That run's failure reason and its
   `outside_list` notes (BPR-1912) are included. Normal text selection
   across entries works as well, not one response at a time.
3. **The controls at the top stop looking like tabs.** The Draft the tour,
   Transcript and related controls read as buttons or actions, not as a tab
   bar. They keep the BPR-1881 agent accent for agentic actions. Nothing that
   isn't a tab should look like one.
4. BPR-1892's "Show the run" opens the run in its new place.

## Acceptance

- AC1. The transcript no longer takes space in the Tour tab's editing
  column, and it opens in one click from the Tour tab and from a failure
  notice.
- AC2. One action copies a whole run (and the whole session) as plain text,
  including a failed run's reason. A Qt test reads the clipboard.
- AC3. Text selection spans entries.
- AC4. No control on the Tour tab that isn't a tab is styled like a tab.
- AC5. Owner look: an attended Accept/Reject on a frozen build that shows a
  failed run's transcript being copied, and the new controls.
- AC6. The RFC-BPR-0092 T2 amendment is updated, and the documentation gate
  passes.

## Implementation (2026-11-06)

`frontend-design:frontend-design` was loaded first, as the owner asked, and
chose the transcript's new home and the controls.

- **Where the transcript went: its own window.** A separate, non-modal
  **Tour session transcript** window, opened by **Transcript (n)** on the Tour
  tab, by BPR-1892's **Show the run** at the failed run, and by BPR-1912's
  "See the transcript" at the first command that ran outside the list. A
  sheet would block the tour while open, and a drawer surface would take
  room from the editing column again; a window sits beside the main window
  while the author edits. The runs are a list on the left (newest first, a
  failed run in the heavier weight); the chosen run is one read-only
  `QTextBrowser` document on the right, so a selection runs across its
  entries and the platform's Copy takes it. The window wears the main
  window's appearance (every palette role of the Tour tab's ground, as the
  Settings window does).
- **Copy.** **Copy run** (the one bordered action, with the copy glyph) and
  **Copy all runs** (bare) put plain text that reads as Markdown on the
  clipboard and say "Copied run N." / "Copied all runs.": the run's request,
  how it ended, then `- Kind: command or path (note)` per step in order, and
  the answer or, for a failed run, the reason with the failure's own log
  (`agent_launch_panel::failure_log`: what went wrong, exit code, byte
  counts, the first line the app could not read, the tail of its errors). A
  failed run's transcript now keeps that log (`github_application`), not only
  the headline.
- **The controls.** Draft the tour is the row's one bordered action at its
  own width, on the left; Transcript (n) is a bare action with a page glyph
  at the far end, never checkable. Before, the two sat side by side at equal
  width, the checked Transcript raised like a selected segment.
- **Look.** `--attended-tour-drafting` with a `claude` in
  `attended-drafting-failing-claude` (the stub reads, runs `awk` outside the
  list, then asks for WebFetch, which ends the run as `tool_refused`): the
  look stops at the failure notice (`ATTENDED_TOUR_FAILURE_TRANSCRIPT_READY`)
  and, when drawing, presses Show the run and Copy run and prints the
  clipboard (`ATTENDED_TOUR_COPIED`).
- RFC-BPR-0092 Amendment BPR-1900 T2 amended; `review-tours.md`.

## Verification

Focused, owner: "dont wait for tests". On `840b1c629`:

- `qt_tour_agent_session_view`: AC1, AC2 (clipboard read for Copy run and
  Copy all runs), AC3 (selection across entries and Copy), AC4 (no
  checkable segment, Draft at its own width, Transcript bare), the window
  following a dark ground.
- `qt_review_tour_agent_failure`: Show the run opens the window at the failed
  run.
- `core_tour_agent_session`, `application_agent_launch`,
  `architecture_boundaries` pass (with [[BPR-1912]]); a full dev build
  compiled on `63718408a`.
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.

## Attended review (AC5)

- v001 (`63718408a`) was frozen and checked offscreen; its pictures showed
  the transcript window light in the dark appearance (a separate window
  inherits no palette), so it was removed before review and fixed in
  `840b1c629`.
- v002: frozen build of `840b1c629` at
  `/Users/jaeger/bpr1913-tour-transcript-v002.49fwr_6f` (executable SHA-256
  `3f300ca66dabb1ec639596212c773e5c56c282d54e680b6f3e237be2a99dc6be`),
  fixture `conventional-comments-v1` on `src/engine/renderer.cpp`, ready line
  `ATTENDED_TOUR_FAILURE_TRANSCRIPT_READY`; the launcher's own offscreen check
  reached it. Launch:
  `python3 /Users/jaeger/bpr1913-tour-transcript-v002.49fwr_6f/launch.py`.
  Pictures from that executable, offscreen, light and dark: the failure
  notice with the new top controls, the transcript window after Copy run, and
  a selection across the run's entries.
- New short session `3ffa1d56ef7825c2b6820979e01edd99`, state
  `/Users/jaeger/.codex/attended-review/bpr1913-v002-A1lc7w/state`, repo root
  the implementing worktree, host started with `serve --tailscale`. One
  candidate, `BPR-1913-tour-transcript` (revision `840b1c629-v002`), one
  Accept/Reject question. Status: ready, awaiting the owner.

