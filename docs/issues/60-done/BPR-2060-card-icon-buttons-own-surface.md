---
schema: "implement-rfc.card.v1"
id: "BPR-2060"
title: "Open card icon buttons paint their own surface in the native style"
status: "Done"
rfcs: []
depends_on: ["BPR-2056"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2060--BPR-2060--707a9bb02e94.json","source_commit":"510a53c52"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"510a53c52","evidence":{"kind":"command","detail":"Cocoa macos style, Dark and HC Dark on light system: card tool icons 1.11-1.14 to at least 12.33 in every state; all four themes and both system appearances at least 12.33; Fusion now draws keyboard focus."}},{"criterion":"AC2","outcome":"pass","revision":"510a53c52","evidence":{"kind":"command","detail":"test_line_numbers_read_on_the_open_card measures each tool against its painted face at rest, hover, pressed and focus; qt_conversation_cards_view, qt_diff_workbench_scale_*, qt_workbench_window_scale_100, qt_composed_github_scale_100, architecture_boundaries, format and tidy pass."}}]
---

# Open card icon buttons paint their own surface in the native style

Follow-up to BPR-2056. `quiet_tool_button` now paints icon-only buttons itself
on a named palette surface (`set_surface`), with hover and press tints from that
surface and the standard keyboard focus ring. The open card's tools, Ask agent,
foot buttons, trash buttons and defect badge use it.

## Acceptance

- AC1. Card icon buttons meet 3:1 non-text contrast against their painted face
  under the native macOS style with light and dark system appearance.
- AC2. Accessible names, tooltips and keyboard activation are unchanged.

Not fixed: `make_drawer_tool` (code block Wrap/Copy, agent tab tools), the
label menu popup, and tool buttons on other surfaces.
