---
schema: "implement-rfc.card.v1"
id: "BPR-0829"
title: "The anonymous public preview course's pass expression omits its epoch line history"
status: "Ready"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-verify new defect 1","found_at":"3b968fcc0027c774187c11489f4020c2463bcd2f","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0829: The anonymous public preview course's pass expression omits its epoch line history

## Defect

`application_anonymous_public_preview_public_path` fails on Linux, and the
station reports that a revert A/B test fails the same way. Its pass
expression (`tests/end_to_end/CMakeLists.txt`, ~2179) runs straight from
`PUBLIC_PREVIEW_HISTORY_INCOMPLETE:true` to
`PUBLIC_PREVIEW_SMART_ACQUISITIONS:1`. Since `26fd7f7c` (2026-09-12,
RFC-BPR-0029 composed epoch gaps), `src/app/main.cpp` has printed
`PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:<text>` between those two lines on every
run.

The station also saw `PUBLIC_PREVIEW_SMART_ACQUISITIONS` vary between 1 and 2
across Linux runs. A varying count is a separate question: either a real
duplicate acquisition, or a course that counts an operation whose number
depends on timing.

## Fix and scope

- Name the history line by shape (`PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:[^\r\n]*`).
- Do not loosen `SMART_ACQUISITIONS:1` until the variance is explained. If
  macOS repeats hold at 1, the Linux variance goes to the Linux station to
  diagnose, and this card records what it finds.

## Acceptance

- The course passes on macOS, repeated.
- The Linux station either passes it with `SMART_ACQUISITIONS:1` or names
  the cause of the second acquisition.
