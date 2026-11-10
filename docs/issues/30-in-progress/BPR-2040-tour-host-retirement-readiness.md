---
schema: "implement-rfc.card.v1"
id: "BPR-2040"
title: "The Tour composer offers follow-ups only after host retirement"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"frozen54-functional-fixes-r1","reservation":"docs/issues/_reservations/BPR-2040--BPR-2040--9725072d7690.json","milestone_plan":{"id":"bpr-2040-tour-host-retirement-readiness","path":"docs/rfcs/plans/bpr-2040-tour-host-retirement-readiness/milestone-plan.json"}}
---

# The Tour composer offers follow-ups only after host retirement

- AC1. Retain actual frozen54 failure and identify the result-projection/cleanup readiness window without changing fixture waits or budgets.
- AC2. Project the existing tour_session_idle observation as a separate capability. Preserve early final text, Running and Stop; gate only New session, Update, Continue and non-Stop Send. Use an idempotent identity/generation/editor-lifetime-owned mechanical observation at most10Hz, stopping on idle, Closing, destruction, host replacement or new run. Add no timeout/refusal/effect/business policy.
- AC3. Deterministic test-only late cleanup gate proves old public enabled-before-retirement and candidate disabled-before/ready-after. Preserve one original follow-up/backend-memory oracle; include Stop/cancel/close/new-generation guards and original deadlines. Test gates compile out of tests-disabled distribution.
- AC4. Review/recompute only the142 content pin with inventories/full-app/mutation pins preserved; static/focused gates pass, intentionally land only owned release-fix source/tracker slice on main, and return exact source/evidence/command state without final release qualification credit.

Frozen source899056e7de7187edc0ee206a7dc499a1f5487af4. Product cutoff5fd is preserved; no foreign product intake. Parent owns running qualification and native leases. Only source/static preparation is authorized until its explicit handoff.
