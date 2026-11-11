---
schema: "implement-rfc.card.v1"
id: "BPR-2002"
title: "Give each agent question a subject and replace the scope list with the attachment"
status: "Done"
rfcs: ["RFC-BPR-0102"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0014-question-subject-and-attachment.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0102 AC01","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"From Overview with nothing selected, Review this pull request sends: subject=pull_request, run_ai_review, selection=0, files=0, every changed file in subject.file.*; docs/verification/RFC-BPR-0102.md","command":"ctest --preset dev -R application_agent_chat_subject_contract"}},{"criterion":"RFC-BPR-0102 AC02","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"With lines selected and the chip shown, the same request; the diff selection stays","command":"ctest --preset dev -R application_agent_chat_subject_contract"}},{"criterion":"RFC-BPR-0102 AC03","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Installed Claude Code 2.1.319 (Haiku) on nested-folders#21 (17 files) with README.md open: the review named 13 other changed files","command":"QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/better_pull_review_agent_chat_composition_public_path live-claude /opt/homebrew/bin/claude"}},{"criterion":"RFC-BPR-0102 AC04","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Typed question carries subject=lines with path and lines; after the chip's close the next is about the pull request and the selection remains","command":"ctest --preset dev -R application_agent_chat_subject_contract"}},{"criterion":"RFC-BPR-0102 AC05","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Escape with the find bar open closes it and keeps the selection; Escape again clears selection and chip","command":"ctest --preset dev -R application_agent_chat_subject_contract"}},{"criterion":"RFC-BPR-0102 AC06","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"No scope list in the Agent tab; no enabled control says Select the lines; no refusal across five sends; qt_workbench_window_scale_100 chip checks","command":"ctest --preset dev -R application_agent_chat_subject_contract"}},{"criterion":"RFC-BPR-0102 AC07","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"A typed question with nothing selected is sent and answered","command":"ctest --preset dev -R application_agent_chat_subject_contract"}}]
---

# Give each agent question a subject and replace the scope list with the attachment

## Scope and acceptance

Implements
[RFC-BPR-0102](../../rfcs/RFC-BPR-0102-agent-questions-say-what-they-point-at.md).

Have the core decide a request's subject; send where the reviewer is looking
separately from it; remove the scope list and show selected lines as a
detachable chip; make suggestions carry their subject and entry point so each
one sends from any mode; send a typed question with no selection; clear the
diff's selection on Escape under the RFC's precedence.

Covers AC01 to AC07. AC03 needs an installed agent on a fixture pull request
of at least five changed files. Load frontend-design before the Qt work.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
