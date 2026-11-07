---
schema: "implement-rfc.card.v1"
id: "BPR-1913"
title: "The tour session's transcript copies as a whole and leaves the tour's room, and its controls stop looking like tabs"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1878","BPR-1881","BPR-1892","BPR-1900","BPR-1912"]}
acceptance: []
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
