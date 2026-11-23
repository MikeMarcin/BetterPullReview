---
schema: "implement-rfc.card.v1"
id: "BPR-2363"
title: "Complete skill names in the item side chat"
status: "Done"
rfcs: []
depends_on: ["BPR-1916", "BPR-2322"]
priority: "P2"
metadata: {"kind":"defect","origin":"owner, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2363--BPR-2363--f17be8200e59.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"30111586dd0b3c3c6190e7944c100e88f6ab21fe","evidence":{"kind":"command","detail":"application_follow_up_public_path, macOS arm64 Release with Qt 6.12.1, offscreen in the network sandbox. Ask follow-up on a report finding opened the item chat. Typing /fixture-item showed the two personal fixture rows and their source in the existing skill picker. The popup raster showed both names and their source. The new test against the previous product failed at the missing picker."}},{"criterion":"AC2","outcome":"pass","revision":"30111586dd0b3c3c6190e7944c100e88f6ab21fe","evidence":{"kind":"command","detail":"application_follow_up_public_path verified Down and Tab select the highlighted row and keep composer focus, Return selects without an Ask, Shift+Return adds a line with the picker closed, and the next Return sends. qt_skill_completion_keys, qt_item_side_chat and qt_side_chat_tab_and_send_scale_100 and _200 passed on the candidate."}},{"criterion":"AC3","outcome":"pass","revision":"30111586dd0b3c3c6190e7944c100e88f6ab21fe","evidence":{"kind":"command","detail":"application_follow_up_public_path received the fixture agent's acknowledgment of the selected skill's inline guidance and confirmed the unselected skill stayed out. The next question carried the transcript without inheriting the skill guidance. Draft, thread and report follow-up, explicit application of proposals, and anonymized diagnostics passed in the same course. application_follow_up_host, architecture boundaries, clang-format 22.1.8 and the documentation checks passed."}}]
---

# BPR-2363: Complete skill names in the item side chat

> The side chat composer isn't doing skill completion popups like the agent tab composer does

The item side chat opens beside a review report or beside the code when a
reviewer asks about a finding, draft or thread. Its composer should offer
the Agent tab's skill picker.

## Acceptance

- AC1. Typing a slash and part of a skill name in the item side chat shows
  matches from the same catalog as the Agent tab, with their sources.
- AC2. The picker inserts the chosen skill into the draft and leaves the
  keyboard in the composer. Return chooses while the picker is open and
  sends the question when it is closed. Shift+Return still inserts a new line
  with the picker closed.
- AC3. The existing follow-up course exercises the composed side chat with
  fixture skills and the fixture agent. Sending a completed name uses that
  skill's inline guidance in the item's ordinary Ask turn.

## Plan

Use the Agent tab's picker and catalog as-is in the item side chat. Resolve
skill references in the typed question at the existing Ask boundary, and
attach the named guidance to its tool-free request using the existing
inline fields. Check the real report's side chat with keyboard input and
the fixture agent.
