---
schema: "implement-rfc.card.v1"
id: "BPR-0250"
title: "Persist agent sessions and select private context explicitly"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: ["BPR-0229","BPR-0238","BPR-0247"]
---

# Persist agent sessions and select private context explicitly

BPR-0247 composes the portable invocation path and deliberately supplies zero
private notes and zero drafts. Its session identity is process-local: every
restart begins at one, and no durable record says which exact revision and
profile an accepted invocation was pinned to. BPR-0232 cannot close while the
native Windows lifecycle remains deferred, so this card owns the independent
available-target remainder without claiming Windows launch.

## Required final state

- Persist a bounded catalog of immutable agent session metadata inside the
  exact review-ledger scope: session ID, profile ID, host, repository, pull
  request, base/head OIDs, and left/right epochs. Session IDs never repeat
  after restart, even when bounded retention removes older catalog rows.
- Migrate the review-ledger database forward from canonical schema version 3,
  validate the exact new schema before mutation, preserve all prior records,
  refuse stamped/tampered or wrong-application files byte-for-byte, and keep
  review/account/repository/PR scope isolation.
- Validate and durably admit the session before any workspace, executable, or
  child-process effect. Storage refusal is typed, visible, and leaves every
  downstream effect counter at zero. Reopen loads without writing.
- Present the latest retained session as a non-sensitive, revision-pinned
  record and classify it current, head-moved, epoch-changed, or different
  review through the existing core policy; never retarget an old session.
- Offer the current bounded private-note identities and the current unpublished
  review draft as explicit, unchecked-by-default launch choices. Starting an
  invocation carries only the chosen note IDs and the chosen current draft.
- Revalidate the choices against the exact private observation and current
  draft at dispatch. Missing, duplicate, stale, over-limit, or invalid choices
  refuse before persistence/workspace/process effects. An unselected note or
  draft has no path into the supplement.
- Extend the real selected-diff executable course to seed two distinguishable
  private notes and one current draft, select exactly one note plus that draft,
  compare the child's complete schema-v2 stdin against independently captured
  facts, prove the unselected prose absent, then reopen and prove the exact
  immutable session record, monotonic next ID, and zero reopen writes.
- Preserve every BPR-0247 construction/gesture/cancellation/staleness/cleanup
  cut and every BPR-0240 credential, argv, protocol, bounds, and Windows
  pre-start-refusal claim.

## Persistence boundary

The durable record contains metadata only. Do not persist agent output,
terminal results, credentials, environment values, argv, workspace paths,
private-note text, draft text, source text, presentation coordinates, or live
scroll state. Findings, chat transcripts, and result promotion remain
RFC-BPR-0012 scope.

## Focused verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_agent_session|application_agent_launch|application_agent_invocation_host|application_review_ledger_host|sqlite_review_ledger_storage|qt_agent_launch_panel|application_agent_invocation_(composition|construction_cut|gesture_cut|cancellation|staleness|cleanup_retry)_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Non-goals

- Do not launch a live or installed agent, call a model API, or add credentials.
- Do not persist arbitrary profiles, selected private prose, agent output, or
  workspace state.
- Do not alter Windows launch. BPR-0240 remains responsible for suspended
  creation, descendant containment, and target-native proof at the deferred
  native stage.
- Do not move BPR-0232 or RFC-BPR-0011 to terminal status from this card.

## Implemented state

The review-ledger schema is now version four. It retains at most 128 immutable
metadata-only agent-session rows per exact review scope while a separate
non-pruned high-water sequence prevents identity reuse after retention,
restart, or simultaneous admissions from two open stores. Canonical version
three migrates without losing receipts, notes, reviewed heads, or defects.
Existing wrong-application, future, and schema-tampered main/WAL/SHM images are
validated through an owner-private clone before the source can be opened for
mutation; stable refusals and current-schema reopen/load preserve the physical
triplet exactly. Detected namespace movement refuses. Clone cleanup sanitizes
retained main/WAL authority, deletes only bounded known children, preserves
substitutes and unknown children, and surfaces incomplete cleanup. Atomic
protection against a malicious same-user process racing the final pathname use
is deliberately outside this card's filesystem threat model.

The invocation host now performs core/profile and exact-context validation,
revalidates checked note IDs plus the chosen current draft identity/revision,
and atomically admits the durable session before workspace, environment,
executable, or child-process work. Typed store refusal leaves every downstream
effect counter at zero. Qt offers bounded note identities and the current draft
unchecked by default, clears stale choices, and presents the latest immutable
session as current, head-moved, epoch-pair-changed, or different-review without
retargeting its binding.

The real two-process selected-diff contract selects note 246 of two plus the
current draft, compares the child's complete schema-v2 input against
independently captured facts, and proves the unchecked hostile prose and
credential canary absent. A fresh process reopens the same data root and proves
latest session 1, next identity 2, current classification, and zero admission.
The wrapper requires distinct child-authored seed and reopen markers, guards
both source branches, and rejects `/usr/bin/true`; CTest relies on exit status.

The literal focused matrix passed 12/12. The final correction boundary passed
SQLite storage and the public two-process contract 2/2. Scoped formatting and
SDK-aware clang-tidy, documentation validation, and `git diff --check` pass.
Independent critical review reproduced physical sidecar mutation and a vacuous
wrapper, drove both corrections plus bounded namespace/cleanup counterexamples,
and returned CLEAR with no remaining blocking or high-risk finding in card
scope. Native Windows launch remains deferred to BPR-0240 and BPR-0232.
