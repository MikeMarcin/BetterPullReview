---
schema: "implement-rfc.card.v1"
id: "BPR-1082"
title: "Compose convention edits through shared durable draft services"
status: "Done"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1077", "BPR-1081"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-08","milestone":"authoring","acceptance":["R09","R10","R11","R41","R42"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9ab462544664eeb11dc969f31e54c984ff07550d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -R 'core_feedback_draft_edit|application_feedback_draft_store' (tests/core/feedback_draft_edit_tests.cpp, tests/sqlite_contract/feedback_draft_store_tests.cpp)","detail":"Human-only compose/save/restart with no agent, provider or publication port: a durable inline draft in the real SQLite private review store is written, labelled issue (blocking) through plan_feedback_draft_edit (volatile apply refused durable_required), committed by change_draft and adopted to two windows; after the store is reopened restore gives the exact bytes and the label is read from them; Plain restores the original; a hand-typed label survives a second restart. private_draft columns unchanged; no publication record or agent run. Label first: an empty draft takes 'issue: ' and 528 header-only relabels before a subject. macOS arm64 Debug -Werror; FEEDBACK_DRAFT_EDIT_OK, FEEDBACK_DRAFT_STORE_OK."}},{"criterion":"AC2","outcome":"pass","revision":"9ab462544664eeb11dc969f31e54c984ff07550d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -R 'core_feedback_draft_edit|application_feedback_draft_store'","detail":"A label planned before another window's typing or another store connection's write is refused by the existing revision CAS; the newer text is kept byte for byte and replanning reports stale_draft with the current body (the conflict candidate); no publication record. Seven drafts saved before any label edit reopen byte for byte and take labels except the malformed and quoted bodies; no schema, column or record field was added. A tour stop labelled by hand in a schema-1 store with a codec-7 payload loads byte for byte through review_tour_host; a terminal-origin relabel retransmitted applies once (Replayed); the desktop's stale plan is a conflict; one undo restores the stop, kept after reopen."}},{"criterion":"AC3","outcome":"pass","revision":"9ab462544664eeb11dc969f31e54c984ff07550d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -R 'application_review_mutation_application_host|application_feedback_draft_store|core_feedback_draft_edit'","detail":"A publishing draft is not_editable in the core plan, refused by the reducer and by storage, and stays publishing with the identical proposal across a restart. In the real review_mutation_application_host: after a lost answer the plan is refused and recovery prepares the identical request (same body and idempotency key); after a definite failure the relabelled request is bound to a newer draft revision, the earlier grant queues no effect, and a new grant publishes under a new key. Negative control: removing the drafting check fails in all three rows."}}]
---

# BPR-1082: Compose convention edits through shared durable draft services

Pack alias: **PACK-CC-08** (live `BPR-1082`, allocated at import).

## Outcome and work

Bind pure prefix edits to current revisioned draft/host/store services, including inherited desktop/CLI ownership. Preserve optimistic conflict candidates, existing save semantics and mutation proposal identity. Do not persist public classification as an independent editable authority.

## Acceptance

- Human-only compose/save/restart works with no agent configured.
- Conflicting edits do not lose data or publish; existing old-schema drafts load intact.
- A changed label after preparation invalidates the exact affected action without bypassing recovery.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation evidence, 2026-10-05

Product commit `9ab462544664eeb11dc969f31e54c984ff07550d` adds
`feedback_draft_edit.hpp`/`.cpp` to the Qt-free core, and three test rows:
`core_feedback_draft_edit`, `application_feedback_draft_store`, and a new case in
`application_review_mutation_application_host`. Application, adapter and `src/app`
sources are unchanged. The [execution ledger](../../verification/RFC-BPR-0047-0051.md)
records the decisions and the full evidence. The
[architecture note](../../architecture/review-round-trip.md) and the
[UX contract](../../ux/conventional-comments-feedback.md) record the binding and the
label-first rule.

- **Binding.** `plan_feedback_draft_edit` turns one header edit into the ordinary
  whole-body `edit` the comment-draft owner already accepts. It carries the shown
  revision and no composition link, so it travels the path a typed edit takes:
  `change_draft` then `adopt` for a durable draft, and `apply` for a volatile one.
  `plan_feedback_tour_stop_edit` does the same for the tour owner's `edit_stop`,
  which the desktop and `bpr tour` share. No label is stored beside the
  Markdown.
- **Human-only compose, save, restart.** This uses the real SQLite private
  review store, with no agent, provider or publication port. A draft is
  written, labelled `issue (blocking)`, shared to two windows, reopened, made
  plain and relabelled by hand. Every step keeps exact bytes across restarts.
  The draft table has no new column, and the store holds no publication or
  agent run.
- **Label first.** A blank comment takes the header alone (`issue: `). The
  header can be redecorated, relabelled or removed before the subject is
  typed: 528 relabel cases.
- **Conflicts and existing drafts.**
  - A label planned before another window's typing, or before another store
    connection's write, is refused by the existing revision check. The newer
    text is kept byte for byte, and replanning reports `stale_draft` with the
    current body.
  - Seven drafts saved before any label edit reopen byte for byte and take
    labels, except the malformed one and the quote.
  - A tour stop labelled by hand loads byte for byte from a schema-1 store with
    a pre-journal payload.
  - A terminal-origin relabel sent twice applies once. The desktop's stale
    plan is a `conflict`. One undo restores the stop.
  - Nothing publishes.
- **Changed label after preparation.**
  - A publishing draft is `not_editable` in the core, in storage and across a
    restart.
  - After a lost answer, recovery prepares the identical request, with the
    same body and idempotency key.
  - After a definite failure, a relabel is bound to a newer draft revision.
    The earlier grant cannot carry it, and a new gesture publishes it under a
    new key.
- **Negative controls.** Five reverted mutations each failed at the expected
  check. They are listed in the ledger.
- **Checks.**
  - `dev`: the whole tree built, and 24 of 24 focused, boundary and
    network-isolation rows passed.
  - `core-only`: the whole tree built, and 9 of 9 passed.
  - `coding-format-check` passed, and clang-format 22.1.8 and 23.1.1 agree.
    Tidy reports nothing in the changed files.

Limitations:

- **Platforms.** These are macOS arm64 Debug runs only, and the Linux and
  Windows station runs are owed.
- **Qt and CLI.** The Qt controls are BPR-1083, and the ordinary comment
  draft's headless command is BPR-1092.
- **Owner channel.** The tour test drives `review_tour_host::execute` with a
  terminal origin, not over the owner channel's socket.
- **Requirement shares.** R09–R11, R41 and R42 are met for this card's share
  only. The closeout cards hold the rest.

**Note: Linux run at `597f9edf7` (holds `a736600dd`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, the `dev` preset, `build/bpr1082`).** The full tree built
with no GCC warning or error, and the 23 matching rows passed. They include
`application_feedback_draft_store`, `application_private_review_host`
(5.3 s), `application_review_mutation_application_host`,
`application_shared_draft_application_host`, `application_review_tour_host`,
`sqlite_review_tour_storage`, the five `core_feedback_*` rows, the eight
`test_network_isolation_*` rows, `core_dependency_boundary`,
`core_boundary_contract`, `architecture_boundaries` and
`architecture_boundary_contract` (181.2 s). Nothing needed a fix.
`build/bpr1082` was removed. Done is not reopened.

**Note: Windows run at `fcd0aebea` (holds `a736600dd`; station `bpr-main-win`,
MSVC 14.51, Qt 6.11.2, `windows-dev` in `build/bpr1082`, Debug, whole tree;
no remote mutation).** The whole tree built with no `warning C` or
`warning LNK` line. With two pins added, the named selection passes 23 of 23.
That count includes [[BPR-1083]]'s `application_label_authoring_contract`
(10.2 s), which the manifest named. `architecture_boundary_contract` took
366.3 s.

- **Two pins.** `application_feedback_draft_store` and
  `application_label_authoring_contract` were registered
  `WINDOWS_UNCONTAINED` but not pinned, so `test_network_isolation_manifest`
  failed. Measured contained through the sandbox launcher:
  - the draft store's fixture stops at its first private-review store
    (`feedback_draft_store_tests.cpp:51`). That `std::expected` is
    unchecked, so the run ends with "operator*() called on a std::expected
    that contains an error" (exit `0xC0000409`) after 0.13 s;
  - the label authoring application stops at startup with
    `STARTUP_PROFILE_REFUSAL:registry_unavailable`.

  Both pass uncontained. They are now pinned `dos-volume-name` in
  `scripts/verify_test_network_isolation.py`, with the measured lines.
- **Interpreter.** In the fresh tree's first run, `test_network_isolation_child`
  failed with `WinError 5`. CMake had chosen the per-user
  `pythoncore-3.14-64` interpreter, whose descriptor has no ALL APPLICATION
  PACKAGES entry, so the contained Python could not start itself again.
  Configured with `C:/Program Files/Python314/python.exe`, the interpreter
  `build/windows-dev` uses, it passes. That is station configuration, not a
  code change.
