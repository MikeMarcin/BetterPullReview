---
schema: "implement-rfc.card.v1"
id: "BPR-2144"
title: "Suggested prompt macros: the macro and its rules in core"
status: "Done"
rfcs: ["RFC-BPR-0125"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC01-AC04"}
acceptance: [{"criterion":"AC01-AC04","outcome":"pass","revision":"4bdf03b33","evidence":{"kind":"command","detail":"core_prompt_macro passes AC01-AC04 on the core-only preset; architecture_boundaries, format, tidy and documentation gates pass."}}]
---

# Suggested prompt macros: the macro and its rules in core

Pure core: the macro value and bounds, the closed-variable template scan and expansion, facts and conditions, five chips then More, validation, the merge of the shipped set with stored differences, and the 22 built-ins proved equal to today's lists. Evidence: commit 4bdf03b33, `core_prompt_macro` (AC01-AC04 pass, core-only preset), architecture boundaries, format, tidy and documentation gates.

## Acceptance

RFC-BPR-0125 AC01-AC04.
