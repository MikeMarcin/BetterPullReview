---
schema: "implement-rfc.card.v1"
id: "BPR-2143"
title: "Skill picker: Tab inserts the highlighted or first skill"
status: "Done"
rfcs: []
depends_on: ["BPR-1916"]
priority: "P3"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2143--BPR-2143--1c2244328b34.json","source_commit":"66bd2606a"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"66bd2606a","evidence":{"kind":"command","detail":"qt_skill_completion_keys: /prefix then Tab inserts the skill, closes the list, keeps focus and caret; Down then Tab inserts the second; Tab equals Return; Shift+Tab and Tab with the list closed unchanged. qt_settings_window, qt_tour_agent_session_view, architecture_boundaries, format and tidy pass."}}]
---

# Skill picker: Tab inserts the highlighted or first skill

Owner request: Tab-complete skills in the picker. Tab already accepted the
highlighted skill offscreen since BPR-1916; this pins it with a test, makes Tab
insert the first match when nothing is highlighted, lets modified Tab pass
through, and mentions Tab in the list's description and the prompt editor hint.

## Acceptance

- AC1. With the skill list open, Tab inserts the highlighted skill (or the
  first match) like Return, in the Agent tab, tour side chat and prompt editor.

Not verified: native macOS key handling; the owner's report may come from an
older build or a native-only path.
