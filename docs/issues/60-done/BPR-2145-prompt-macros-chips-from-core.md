---
schema: "implement-rfc.card.v1"
id: "BPR-2145"
title: "Suggested prompt macros: chips come from the core"
status: "Done"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2144"]
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips on the lane after the merge (8ba69de50): built-in chips and requests equal today's in the Agent tab, a conversation and the tour chats"}},{"criterion":"AC06","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips: expansion sent, Shift-click drafts, {cursor}"}},{"criterion":"AC07","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips: tooltip and description, turn name, fold past six lines, kept after restart"}},{"criterion":"AC08","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips: five chips and More prompts (n)"}},{"criterion":"AC09","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips: no skill or command from values"}},{"criterion":"AC10","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"qt_prompt_macro_chips: a drafted macro keeps its subject and kind behind a removable chip"}}]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC05-AC10","base":"branch rfc0125-m2-on-lane off claude/v0-5-0-prerelease-targets-52265d (owner choice 2026-11-12)"}
---

# Suggested prompt macros: chips come from the core

The Agent tab and tour chats show the core's offers; the hard-coded lists in the widget and `tour_chat_prompts` go. Expansion tooltip and accessible description, the sent turn under the macro's name folded past six lines, More prompts (n)..., values cannot command, and the drafted macro's removable chip (Q2). Load frontend-design first. Blocked: needs a base holding the lane's RFC-BPR-0102 and RFC-BPR-0107 code (`claude/v0-5-0-prerelease-targets-52265d`).

## Acceptance

RFC-BPR-0125 AC05-AC10.
