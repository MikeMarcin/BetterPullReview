---
schema: "implement-rfc.card.v1"
id: "BPR-1523"
title: "Repair native compiler portability exposed by release preflight"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "linux", "mac"]
metadata: {"source":"0.2.1 Windows and Ubuntu native release preflight","assigned_implementer":"/root/release021_native_readiness","baseline":"219b62e98b571e640f726592065a3c798bb712f8","milestone_plan":{"id":"bpr-1523-native-release-portability","path":"docs/rfcs/plans/bpr-1523-native-release-portability/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dcc5ace22d16583f32b48fe71a5f1842c214d2d0","evidence":{"kind":"observation","detail":"Current Windows4test compiler failures and Ubuntu index sign-conversion failure reproduced at exact219b; app-only current Windows pass and historical logs separately retained. Windows sign-in/Azure prerequisites distinguished from source failures.","artifact":"/Users/jaeger/.codex/evidence/bpr1523-native-portability-jsim8_uo/manifest.json"}},{"criterion":"AC2","outcome":"pass","revision":"dcc5ace22d16583f32b48fe71a5f1842c214d2d0","evidence":{"kind":"observation","detail":"Minimal corrections only five assigned files; all assertions, thresholds, warning gates and4px inset preserved. Actual native glyph widths restore fixed badge geometry. Existing performance JSON moved before unchanged assertions only. Frontend-design loaded; offscreen native renders inspected; self-review recorded.","artifact":"/Users/jaeger/.codex/evidence/bpr1523-native-portability-jsim8_uo/manifest.json"}},{"criterion":"AC3","outcome":"pass","revision":"dcc5ace22d16583f32b48fe71a5f1842c214d2d0","evidence":{"kind":"observation","detail":"Affected native targets build, Windows6focused compiler/geometry behavior checks pass in Interactive Limited session1; Ubuntu7/7 and Mac5portability+3geometry+1diagnostic checks pass with relevant reuse. Format22, focused Qt tidy and documentation pass. Exact source handed back with separately failed Windows performance course explicit; full release gates remain coordinator obligations.","artifact":"/Users/jaeger/.codex/evidence/bpr1523-native-portability-jsim8_uo/manifest.json"}}]
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


## Completed engineering and evidence

Current baseline `219b62e98b571e640f726592065a3c798bb712f8` reproduced all four Windows test compiler failures and Ubuntu's source-search index sign-conversion failure. The application compiled on Windows before repairs. Historical logs remain separate from these current negatives.

Corrections are limited to the five assigned files: explicit UTF-8 matcher bytes, the direct QCoreApplication include, a namespace-scope recording sink compatible with MSVC, Windows header ordering, a common unsigned index type, unsigned side-mask operands, and fixed L/R badge slots sized for the actual native glyphs. The four-pixel left inset and all existing geometry assertions remain. The frontend-design skill was loaded for the native font conformance repair; inspected offscreen Mac and Ubuntu renders confirm the existing design. No acceptance criterion changed. The performance course now prints its existing JSON measurements before its unchanged assertions so native failures retain diagnostics.

At `12a4987527589efe8db67ab9b12cb7aeec9d5e58`, Windows app and affected targets build; matcher, staged session/restart, Git contract and both Qt model scales pass in the supported signed-in Interactive Limited session. Ubuntu app/affected targets and all seven focused courses pass (14.39 seconds). Mac's four test portability targets and five courses pass (32.44 seconds), then the final model/performance courses pass (3/3, 8.87 seconds), with focused Qt tidy passing. At `dcc5ace22d16583f32b48fe71a5f1842c214d2d0`, the diagnostic-only report reorder is compiled and Mac performance passes again (7.91 seconds). Format22 and documentation pass.

A separate release performance blocker remains: Windows public PR Search navigation takes 3188 milliseconds, then 3142 milliseconds on one quiescent repeat, against the unchanged 2000-millisecond limit. All other performance budgets pass. The course opens `bulk/063.cpp` through Files while the cold 68-file corpus is active and waits for the selected diff and completed geometry. This is retained as a failed release check, not a portability acceptance or release approval. The coordinator assigned separate implementer diagnosis; no broader scheduling source changes were made here.

Windows sign-in is resolved by observed Active console session 1 as mikem. The original dedicated Azure profile fails supported CLI DPAPI decryption; that is not evidence of expiration. A separate fresh-profile device-code wizard and visible console remain pending exact approved-identity verification. The old profile is preserved; no signing, package, publication, key transfer or passphrase enrollment occurred.

Durable evidence manifest: `/Users/jaeger/.codex/evidence/bpr1523-native-portability-jsim8_uo/manifest.json`. This bounded compiler/geometry milestone is complete. Full normal release gates, the Windows performance blocker, fresh Azure profile verification, and RFC-BPR-0038 native/vault/crash/vulnerability qualification gaps remain release obligations.
