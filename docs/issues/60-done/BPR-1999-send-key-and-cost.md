---
schema: "implement-rfc.card.v1"
id: "BPR-1999"
title: "Send on Return and show cost per turn and per session"
status: "Done"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0011-send-key-and-cost.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0101 AC03","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Return sends, Shift+Return adds a line, IME composition and an empty field send nothing, Ctrl/Cmd+Return also sends; Return alone in a comment composer does nothing","command":"ctest --preset dev -R 'qt_workbench_agent_send_refusal|qt_conversation_cards_view'"}},{"criterion":"RFC-BPR-0101 AC04","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"0.4123567 -> $0.41, 0.004 -> under $0.01, 12 -> $12.00; no reported cost shows none","command":"ctest --preset dev -R 'core_agent_cost|qt_workbench_agent_send_refusal'"}},{"criterion":"RFC-BPR-0101 AC05","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Three native turns with installed Claude Code 2.1.319: reported 0.0127856, 0.01533501, 0.01702213 (cumulative); footers $0.01, under $0.01, under $0.01; session line $0.02 = sum = last figure; interpretation recorded in docs/verification/RFC-BPR-0101.md#cost-how-claudes-figure-accrues","command":"QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/better_pull_review_agent_chat_composition_public_path live-claude /opt/homebrew/bin/claude"}},{"criterion":"RFC-BPR-0101 AC06","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Tour side chat ending line and session line from the same accounted figures (tour_thread_cost); no installed tour run was made","command":"ctest --preset dev -R 'qt_tour_side_chat|core_tour_agent'"}}]
---

# Send on Return and show cost per turn and per session

## Scope and acceptance

Implements [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md).

Make Return send and Shift+Return add a line in the Agent tab and the
selected-lines side chat, with the input-method and empty-composer exceptions;
hold a turn cost and a session total in the core; establish against the
installed Claude how the runtime's figure maps to a turn's cost and record it;
format and place both as the RFC states, in the Agent tab and the tour side
chat.

Covers AC03 to AC06. Composers that save or publish text keep their keys.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
