# Integration map

Paths below are verified against the supplied top-level snapshot and are **existing seams**. New filenames suggested here are candidates, not evidence that code exists. Intake resolves renamed symbols once and records the mapping rather than creating a parallel subsystem.

| Concern | Existing paths | Intended extension |
|---|---|---|
| Draft semantics | `src/core/include/bpr/core/review_round_trip.hpp`, `review_submission.hpp`, `revisioned_review_draft.hpp` and corresponding `.cpp` files | Add bounded tour/stop identity, ordering, exact fields, range binding and readiness. Keep ordinary summary/source-line commands compatible. |
| Publication host | `src/application/include/bpr/application/review_submission_application_host.hpp`, `review_submission_port.hpp`, `review_submission_recovery_port.hpp`; `src/application/src/review_submission_application_host.cpp` | Stage/verify/submit and maintenance plans through typed effects on the current decision/effect lanes. |
| Existing session/journal | `src/adapters/github/src/review_submission_session.cpp`, `live_review_submission_provider.cpp`; `src/adapters/sqlite/src/review_mutation_recovery_store.cpp`, `src/adapters/sqlite/include/bpr/adapters/sqlite/review_submission_recovery_store.hpp` | Add pending review/comment membership and per-step witnesses to versioned recovery; preserve source-line/whole-summary rows and migrations. Do not choose a schema number without inspecting the live database contract. |
| Provider compilation | `src/adapters/github/src/github_mutation_contract.cpp`, `github_read_contract.cpp`, `github_paged_read.cpp`, `github_graphql_operation_policy.hpp`, `gh_cli_transport.cpp` | Finite new reads/body edits/pending batch/submit operations; shared native/CLI DTO normalization and capability evidence. |
| Source identity | Existing `source_line_ref`, Git/epoch catalog, RFC-0022 anchor/permalink rules and RFC-0029 update identity | Add a narrow contiguous range/source-reference form; keep original versus mapped source explicit. |
| Markdown | `src/adapters/qt/include/bpr/qt/markdown_body_view.hpp`, `src/adapters/qt/src/markdown_body_view.cpp`, `markdown_document.hpp`; `src/core/include/bpr/core/markdown_navigation.hpp` | Reuse read-only preview and link admission. Add a bounded raw-source section/list mapper only where needed; no second rich-text renderer. |
| Authoring editor | `src/adapters/qt/include/bpr/qt/review_draft_editor.hpp`, `review_submission_editor.hpp`; their source files | Extract/reuse appropriate ordinary editor mechanics; new tour editors get a scoped in-place undo transaction, not existing before/after workshop widgets. |
| AI host | `src/application/include/bpr/application/agent_comment_workshop_host.hpp`, `agent_invocation_host.hpp`; current agent/session/context-supplement paths | Add explicit scoped assist operations with bounded immutable context, result validation and no provider mutation capability. |
| Settings | `src/adapters/qt/src/settings_window.cpp`, current profile settings authority/store and existing AI profile records | Four text hook overrides with immutable defaults, effective version, reset and explicitly invoked test. Do not build a second settings store. |
| Real composition | `src/app/src/review_submission_workflow.cpp`, `src/app/src/profile_desktop.cpp`, current application/window composition | Mount Prepare tour and Tour navigation in the current Overview/Review/Discuss/Compose lifecycle. No isolated demo application. |
| Tests | Existing `tests/core`, `tests/application`, `tests/github_contract`, `tests/sqlite`, `tests/qt`, `tests/scripts` and current CTest paths | Add focused production-port tests, guarded fake servers and targeted native/website courses; reuse isolation facilities. |

## Narrow documentation amendments

1. Product contract: add author preparation and explicit PR **body-only** editing. Retain all unrelated PR-management non-goals.
2. AI boundary: allow an explicit magic-button action to replace one captured **local** editor span as one undoable transaction. Nothing grants publishing or changes non-tour workshop acceptance.
3. Provider catalog/CLI contract: define exact new methods, response bounds and pending/membership observations; declare the grouped tour-plan authority described in [AUTHORITY-AND-STATE](AUTHORITY-AND-STATE.md).
4. Domain/epoch model: add tour/stop and source-range references; do not use URL strings or painted rows as core identity.
5. Storage/privacy: document private authored text plus temporary/editable remote baselines and staged-operation recovery. Include profile isolation, retention, schema migration, backup/purge and no source/prompt logging.
6. UX/operator docs: current placement, normal GitHub output, local versus server-pending states, undo behavior, compatibility partial publication, website conflicts, native preview limits and unobserved transport/platform capabilities.

## Avoid these regressions

Do not reintroduce the old “CLI is entirely unavailable” state from embedded older copies. Do not claim Windows/Linux CLI became qualified by this feature. Do not reintroduce a narrow drawer that RFC-0037 moved away from. Do not believe the older workshop header's statement that there is no Markdown renderer. Do not mark pending reviews as successful because the existing submitted-only matcher was bypassed. Do not make formal predecessor closeout or the whole private-thread RFC a reason to avoid implementing independent manual paths; record precise missing dependencies and preserve their final gates.
