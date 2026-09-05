# Current source allocation

This inventory records the exact source seams visible in the 2026-09-04 project
snapshot. Reconcile every path against the live tree before editing.

## Existing semantic anchor and draft policy

- `src/core/include/bpr/core/review_round_trip.hpp`
  - `review_comment_draft` carries `source_line_ref Anchor`.
  - `review_publication_proposal` carries the same semantic anchor, body, and
    disposition.
  - target revalidation distinguishes target change, head move, and unknown
    head.
- `src/core/src/review_round_trip.cpp`
  - owns provider-free draft and publication policy.

RFC-BPR-0022 should extend this vocabulary with placement/capability/
representation decisions. It should not replace `source_line_ref` with a URL,
GitHub line number, or Qt row.

## Existing exact live binding

- `src/application/include/bpr/application/review_mutation_port.hpp`
  - `review_mutation_anchor_binding` binds semantic anchor, path, and commit.
  - `review_mutation_live_binding` binds target, acting identity, and anchor.
  - `review_mutation_request` carries proposal, target, actor, idempotency key,
    and optional live binding.
  - `review_provider_capabilities` currently covers resolve and viewed-state
    operations only.

The new write capability should be narrow and repository/operation scoped. The
current broad provider capability record is not sufficient evidence by itself.

## Existing coordinator

- `src/application/include/bpr/application/review_mutation_coordinator.hpp`
- `src/application/src/review_mutation_coordinator.cpp`

The coordinator fixes one designated target, observes it twice, derives the
publication idempotency key, and calls one provider submission. Extend this
single serialized path with one logical publication and representation child
attempts. Do not add a second fallback coordinator.

## Existing GitHub wire contract

- `src/adapters/github/src/github_mutation_contract.hpp`
- `src/adapters/github/src/github_mutation_contract.cpp`

The current request body always emits:

- `body`
- `commit_id`
- `line` (semantic line + 1)
- `path`
- `side`

The current create-response parser requires status 201 and exact path, commit,
side, line, body, and acting identity. It has no file-subject response variant.

RFC-BPR-0022 adds explicit `native_line` and `file_comment_with_permalink`
wire shapes plus typed remote subject. It must preserve the current exact-
match and authority checks.

## Existing durable recovery

- `src/application/include/bpr/application/review_mutation_recovery_port.hpp`
- `src/adapters/sqlite/include/bpr/adapters/sqlite/review_mutation_recovery_store.hpp`
- `src/adapters/sqlite/src/review_mutation_recovery_store.cpp`

Schema v2 has `recovery_scope`, one `recovery_attempt` per idempotency key, and
one `recovery_baseline`. Transmission states are baseline-incomplete,
prepared, and possibly-transmitted; logical state is unresolved or succeeded.
The provider marks possibly transmitted through a one-shot callback and then
uses GET-only reconciliation.

A native body and a fallback body are different exact writes. The migration
must introduce a parent logical publication and deterministic child attempts,
not overwrite the existing row or treat fallback as an ordinary retry.

## Existing capability cache

- `src/application/include/bpr/application/github_capability_cache.hpp`
- `src/application/src/github_capability_cache.cpp`
- `src/adapters/github/src/github_capability_probe.hpp`
- `src/adapters/github/src/github_capability_probe.cpp`

The authority key is profile/profile revision, host, account, server contract,
and probe revision. Facts are read/schema capabilities. It lacks repository and
write-operation identity.

Prefer a narrow publication-capability cache or a coherent explicit extension;
do not silently turn every existing capability into a per-repository fact.

## Existing read normalization and thread mapping

- `src/adapters/github/src/github_read_normalizer.cpp`
- `src/application/include/bpr/application/github_projection.hpp`
- `src/application/include/bpr/application/review_synchronization.hpp`
- `src/adapters/qt/include/bpr/qt/github_thread_map.hpp`
- `src/adapters/qt/src/github_thread_map.cpp`

GitHub IDs are canonical and the Qt thread map already projects conversations
against source lines. Add honest remote subject, semantic fallback anchor, and
representation evidence to the normalized path. Keep comments in the
conversation surface and retain only a line marker in the diff panes.

RFC-BPR-0021 separately records that arbitrary conversation nodes currently
lack general revision and side identity. RFC-BPR-0022 may carry the exact
current-head revision encoded by its own publication but must not claim the
general historical join is solved.

## Existing Qt draft path

- `src/adapters/qt/include/bpr/qt/review_draft_editor.hpp`
- `src/adapters/qt/src/review_draft_editor.cpp`
- `src/application/include/bpr/application/review_mutation_application_host.hpp`
- `src/application/src/review_mutation_application_host.cpp`
- `src/app/src/github_application.cpp`

The existing path binds the diff workbench selection to the asynchronous
mutation host. Keep the editor representation-blind. Eligibility and fallback
selection belong to core/host; Qt presents typed state and an optional support
diagnostic.

## Tests to extend

- `tests/core/review_round_trip_tests.cpp`
- `tests/application/review_mutation_tests.cpp`
- `tests/github_contract/github_mutation_contract_tests.cpp`
- `tests/sqlite_contract/review_mutation_recovery_storage_tests.cpp`
- `tests/adapters_qt/review_draft_editor_tests.cpp`
- `tests/adapters_qt/github_thread_map_tests.cpp`
- `tests/adapters_qt/composed_github_tests.cpp`
- `tests/scripts/review_mutation_restart_recovery_tests.py`
- mutation isolation and composed public-path tests under `tests/end_to_end/`

Selector names must be confirmed from the live CTest inventory.
