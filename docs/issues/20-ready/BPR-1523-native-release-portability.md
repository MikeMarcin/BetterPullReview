---
schema: "implement-rfc.card.v1"
id: "BPR-1523"
title: "Repair native compiler portability exposed by release preflight"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "linux", "mac"]
metadata: {"source":"0.2.1 Windows and Ubuntu native release preflight","assigned_implementer":"/root/release021_native_readiness","baseline":"219b62e98b571e640f726592065a3c798bb712f8","milestone_plan":{"id":"bpr-1523-native-release-portability","path":"docs/rfcs/plans/bpr-1523-native-release-portability/milestone-plan.json"}}
---

# BPR-1523: Repair native compiler portability exposed by release preflight

The patch release must share one committed source across macOS, Windows and Ubuntu 26.04. Historical native retry logs expose compiler failures in four test sources and the source-search model. Reproduce the current source before correcting them; an old log is a lead rather than current evidence. The coordinator has paused the Mac release gate until the bounded native repair is integrated.

## Acceptance

- AC1: Record current native compiler failures and distinguish them from historical logs and producer launch prerequisites.
- AC2: Correct native compiler portability with unchanged public behavior, assertions, warning gates and selected build policy. Keep changes to the assigned five source files unless the coordinator explicitly expands scope.
- AC3: Build the affected current native targets, run focused tests on usable native paths and the existing Mac producer, run format22 and documentation, and hand back the exact clean candidate. Full normal release gates, signing, packaging and publication remain coordinator obligations.

## Ownership and limits

Complete leaf engineering ownership belongs to `/root/release021_native_readiness`, under the owner's instruction that implementers perform release fixes. Assigned paths are `tests/git_contract/git_process_contract_tests.cpp`, `tests/github_contract/staged_review_session_tests.cpp`, `tests/core/source_matcher_tests.cpp`, `tests/end_to_end/source_pr_search_performance_tests.cpp`, and `src/adapters/qt/src/source_search_panel.cpp`. Preserve all unrelated work and historical Done acceptance.

Native source roots are `C:/Users/mikem/bpr-v021` and `/home/mmarcin/bpr-v021`; Mac focused verification may use `/Users/jaeger/Documents/Codex/releases/v0.2.1-macos`. Windows and WSL compile/test commands are serialized on their shared PC. Existing producer roots, packages, credentials, authentication policies and installed applications are untouched. No signing, package or release approval follows from this card.

The PC's console currently has no signed-in user, which prevents Interactive Limited readiness tasks from starting. That independent Windows desktop prerequisite is carried by the prepared wizard; it is not evidence of Azure session expiration or a request to re-enter the stored Linux passphrase. The release qualification gaps in RFC-BPR-0038 remain open.
