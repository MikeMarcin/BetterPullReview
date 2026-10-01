---
schema: "implement-rfc.card.v1"
id: "BPR-1256"
title: "Linux: application_diff_workbench_public_path and application_appearance_store_fallback_contract never see the comparison; its splitter share is zero"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-1255","BPR-1251","BPR-0823","BPR-1102","BPR-0905"]}
---

# BPR-1256: Linux: application_diff_workbench_public_path and application_appearance_store_fallback_contract never see the comparison; its splitter share is zero

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, Qt offscreen
(`GITHUB_FIXTURE_QT_STYLE:QWindowsStyle`), `build/dev` Debug, at `db021262a`.
Reported first by bpr-main-win's BPR-0905 full suite in this checkout. A serial
rerun failed the same way.

## Observation

`application_diff_workbench_public_path` fails in 1.35 s:

```
DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED:modality=Review host_modality=Review centre=comparison offered=comparison,files_drawer,conversations_drawer withheld=repositories_drawer files_drawer=expanded host_files_drawer=expanded conversations_drawer=expanded host_conversations_drawer=expanded repositories_drawer=collapsed host_repositories_drawer=collapsed drawer_controls=hidden outstanding_requests=1 splitter_sizes=0,320,632,320
PUBLIC_PATH_FAILED
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
CONVERSATION_ANCHOR_EVIDENCE_FAILED
SYNTAX_THEME_REPAINT_FAILED
```

`application_appearance_store_fallback_contract` fails in 1.71 s with
`fallback session process failed (1)`, and its session prints the same
`DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` line.

Every drawer is in its expected state. The first splitter share is 0 pixels, so
nothing in the comparison is visible. [[BPR-1102]] (Windows, capture state) and
[[BPR-0823]] were different causes.

This is a width rule at Linux metrics, like [[BPR-1255]] and [[BPR-1251]]'s
Linux half. Measure them together once before splitting fixes.

## Scope

- Find which share is zero and why: the window's width at capture, the
  drawers' fixed widths, or the minimum width of the comparison.
- Fix it where the layout owns it.

## Acceptance

- Both rows pass on Linux.
- The card names the cause.
