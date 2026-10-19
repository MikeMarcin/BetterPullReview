---
schema: "implement-rfc.card.v1"
id: "BPR-1622"
title: "Evaluate vycor-cpp semantic facts and Windows/WSL portability"
status: "Ready"
rfcs: ["RFC-BPR-0001"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision": "vycor-evaluation-r1", "RFC_BASE_SHA": "ef7fa9cc6b4a8111cdaab4b5765ea425457b26a8", "reservation": "docs/issues/_reservations/BPR-1622--BPR-1622--18d808d153c3.json", "milestone_plan": {"id": "vycor-evaluation-v1", "path": "docs/rfcs/plans/vycor-evaluation-v1/milestone-plan.json"}}
---

# Evaluate vycor-cpp before designing a checker

Owner mandate, 2026-10-18: “Let's try vycor-cpp against those four cases and
BPR's actual compilation database before building our own checker. Also check
its portability for our windows and wsl2 ubuntu use cases”. Mandate
`vycor-evaluation-r1`, bounded follow-up to [[BPR-1621]].

## Acceptance

- **AC1:** Pin upstream revision, licence and toolchain; exercise stock megascope,
  anneal and queries against path versus file_status overloads, real clocks
  through macros/aliases versus unrelated now(), worker construction versus
  harmless references, and a cross-file filesystem wrapper. Include negative
  controls and distinguish exposed facts, unavailable identity and analysis gaps.
- **AC2:** Exercise BPR's actual compilation database with explicit TU coverage,
  parse failures, timing/memory where observed and every adaptation. Attempt
  native Windows and WSL2 Ubuntu using isolated roots; record observed support
  and precise blockers without treating missing execution as acceptance.
- **AC3:** Commit a reproducible small evaluation and grounded recommendation
  against a possible custom clang-tidy check. Preserve all existing gates and
  dependency/toolchain approval. Run focused script/documentation verification,
  self-review and commit explicit paths; leave a durable quiescent handoff.

Evaluation completion may establish an unsupported or incomplete capability;
it does not mean tool adoption, semantic determinism proof or platform approval.
