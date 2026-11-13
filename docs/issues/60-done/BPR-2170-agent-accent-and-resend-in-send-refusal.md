---
schema: "implement-rfc.card.v1"
id: "BPR-2170"
title: "Explain this file wears a changed Agent accent, and Resend sends a typed question again"
status: "Done"
rfcs: ["RFC-BPR-0101", "RFC-BPR-0117"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2170--BPR-2170--716ee9aa825f.json","origin":"found 2026-11-13 on lane claude/v0-5-0-prerelease-targets-52265d: qt_workbench_agent_send_refusal failed at 1c7c5cd1f and later","fix":"807d8f5be"}
---

# BPR-2170: Explain this file wears a changed Agent accent, and Resend sends a typed question again

## Defects

`qt_workbench_agent_send_refusal` failed on the lane for two reasons, the second hidden by the first.

1. **Accent.** RFC-BPR-0101 AC09 (BPR-2000): a changed accent reaches every accented surface. Explain this file (RFC-BPR-0117, BPR-2076) wears the accent as a set palette, made once when the header is built, so after Appearance changed the accent it kept the shipped value (`run without accent: explainFileButton`).
2. **Resend.** BPR-1817: Resend on a failed typed question sends it again. Since RFC-BPR-0125 M2 (`7ff279c29`) `send_prompt` only sends a suggestion that the list offers, and a typed question is offered by no list, so Resend did nothing.

## Fix

Lane commit `807d8f5be`. The document tab strip wears the accent again on its trailing run controls when its palette changes. `agent_tab_view::recover` resends the question as it was asked and restores whatever the reviewer had typed.

## Acceptance

- `qt_workbench_agent_send_refusal` passes: the accent inventory holds with the shipped accent, a changed accent and after Reset, and Resend sends the same message in the same scope.

## Evidence

2026-11-13, dev Debug, macOS, at `807d8f5be`: `qt_workbench_agent_send_refusal`, `qt_agent_footer`, `qt_agent_finding_cards_retained` and `qt_explain_file_card_scale_100` pass. `qt_document_tabs_scale_100` fails in the same way with and without this change (`activation reveals the entire active tab including after restoration`). That failure predates this card and is not part of it.
