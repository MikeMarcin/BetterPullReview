# Source audit and implementation seams

Basis: `better_pull_review_project_source.md`, generated 2026-09-22 at
23:21:38 America/Los_Angeles. The dump SHA-256 and selected embedded-file hashes
are in SOURCE-BASELINE.json. This is a bounded root-source inspection, not a live
checkout, compilation or whole-repository review. The dump includes stale
`.claude/worktrees/...` trees; those were excluded from this audit.

| Observed root seam | Evidence and integration consequence |
| --- | --- |
| `docs/rfcs/` | Root RFCs end at 0038; this dump predates the two new tour packs. Do not claim those packs' runtime is in this snapshot. |
| `AGENTS.md`, `.mask/skill-config/implement-rfc/project.md`, `docs/issues/README.md` | Core decides, host performs, Qt presents; allocator-issued IDs; authored plan import; per-criterion canonical closeout and focused verification. |
| `src/application/include/bpr/application/github_read_model.hpp` | Native `github_review_thread_record` carries a resolution bool and post list. `github_review_comment_group_record` deliberately omits resolution for anonymous REST grouping. Comments include ReplyTo/Deleted; issue comments and review bodies are separate. Normalize these differences, never default unknown to unresolved. |
| `src/application/include/bpr/application/pull_request_overview.hpp` | `overview_file_row` already has ThreadCount and UnresolvedThreadCount. Extend this projection; the new columns are a breakdown of the included population, not additional independent totals. |
| `src/application/src/pull_request_overview.cpp` | `project_pull_request_overview` traverses native threads or REST groups into factsByPath. REST increments total without positive unresolved evidence. Extend the result with explicit resolution/coverage distinctions before showing new counters. Existing use of Comments.front() for author facts is not a license to infer a root from arbitrary page order. |
| `src/application/include/bpr/application/workbench_runtime_ports.hpp` | Typed `review_behaviour_settings` and existing comments filter. Add scoped convention defaults/column IDs without repurposing unread/resolved drawer preferences. |
| `src/adapters/sqlite/src/workbench_settings_store.cpp` | Existing persistence owner. Inspect its live schema/migrations at intake; do not assume a schema version or introduce a separate label settings store. |
| `src/core/include/bpr/core/review_round_trip.hpp`, `review_submission.hpp` | Local draft policy, exact mutation identity and whole-review dispositions already have owners. Preserve them. |
| `src/application/include/bpr/application/review_mutation_coordinator.hpp`, `review_submission_application_host.hpp` | Reuse existing effects and recovery rather than create a convention publisher. |
| `docs/architecture/defect-workflow.md` | Private defect projection; only explicit reviewer intent verifies. Public convention labels are additive user prose, not a substitute for that authority. |
| `src/application/src/agent_comment_workshop_host.cpp` | Existing preview proposal carries original and revised text. Add convention context/candidate inspection here through existing host boundaries; don't bypass it. |
| `docs/architecture/native-markdown.md`, `src/adapters/qt/.../markdown_body_view.*` | Shared Qt Markdown reader, raw body authority, source/copy fidelity and no remote resource loading. Badge integration must preserve this contract and existing native reader limitations. |
| `src/adapters/qt/src/{pull_request_overview_page,github_conversation_panel,conversation_cards_view,review_draft_editor,review_submission_editor,settings_window}.cpp` | Actual Overview, thread/detail, drafts, native submission and Settings surfaces. Integrate current layouts rather than revive old drawer composition. |
| RFC-BPR-0036 / 0037 | Native Markdown and first-review composition have technical implementation notes, but remain Proposed in this dump. RFC status alone is not proof of absent or completed runtime. |
| RFC-BPR-0022 / 0023 | Deferred in this dump. Reconcile their live state; integrate present placement/promotion paths without secretly implementing either whole feature. |

## Required live audit additions

Record actual inherited tour RFC IDs, the segment contract for user-authored
comment content, shared durable draft versioning, CLI entry points, runtime owner
handoff and effective hook/task/settings ownership. Those paths are not guessed
in this pack because their actual archive manifests/source were unavailable here.

## Existing verification seams to consider

- Core: `tests/core/{review_submission,defect_workflow}_tests.cpp`.
- Application: `tests/application/{pull_request_overview,agent_comment_workshop_host,review_submission_application_host,workbench_preferences_consumers}_tests.cpp`.
- Qt: `tests/adapters_qt/{pull_request_overview_page,markdown_body_view,composed_markdown,review_submission_editor,defect_card}_tests.cpp`.
- Provider/storage: `tests/github_contract/live_review_submission_provider_tests.cpp` and `tests/sqlite_contract/review_submission_recovery_storage_tests.cpp`.
- Public path: `tests/end_to_end/application_defect_composition_contract.cmake` and `review_submission_composition_public_path.cpp`.

These are known source paths, not guaranteed future test-selector names. Discover
actual CTest names/composition and run the relevant touched graph. Do not claim a
subsystem-only filter covers the whole workbench. No listed test was run during
RFC-pack authoring.
