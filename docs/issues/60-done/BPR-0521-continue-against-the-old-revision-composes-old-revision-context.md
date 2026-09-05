---
id: BPR-0521
title: "Host: continuing against the old revision composes the old revision's context"
status: Done
rfc: RFC-BPR-0012
depends_on: [BPR-0517]
---

# Host: continuing against the old revision composes the old revision's context

## Why

RFC-BPR-0012's stale continuation `continue_against_old_revision` means the
reviewer keeps talking about the revision the session was bound to.
[[BPR-0514]] and [[BPR-0517]] implemented it in the session sense only: the
chat session host keeps the session's old binding, but the launch path in
`src/app/src/github_application.cpp` composes the frozen snapshot and the
supplement from the epoch host's *current* accepted revision, then admits a
session bound to the old one. The agent therefore reads new-revision
context under an old-revision session, which is the mismatch the
continuation exists to prevent. Recorded by both slices as not theirs to
change and confirmed by the [[BPR-0506]] scout.

## Required final state

- When the chosen continuation is `continue_against_old_revision`, the
  frozen snapshot and the supplement are composed from the session's bound
  revision, not the epoch host's current one; `start_new_session` keeps
  composing from the current revision.
- The binding the helper pins, the snapshot's binding record, and the
  session's binding agree in that turn; a disagreement is refused by name.
- `application_agent_chat_continued_contract` asserts the continued turn's
  snapshot binding equals the kept session binding and differs from the
  observed head; deleting the old-revision composition fails it by name.

## Non-goals

- The continuation choice and its presentation — [[BPR-0499]], [[BPR-0502]].

## What this slice delivered — 2026-09-02

**Where the old revision's material comes from.** The binding, which is the
whole of what the design retains about it. `core::agent_session::Binding` on
the session the chat kept carries the host, repository, pull request, base and
head OIDs, and the epoch pair, and it is the same value
`core::offer_agent_continuations` names as `ContinueAgainst`; the composition
reads it off the turn request and nothing reconstructs anything. The old
revision's *file bytes* then follow for free: `agent_invocation_host` runs the
continued turn under that session, and `materialize_agent_workspace` pins the
disposable workspace to `aSession.Binding.HeadOid` out of the Git cache, so the
agent reads the revision it is being asked about. The old revision's
*alignment* cannot be supplied and no storage was invented for it:
`core::epoch_state` holds exactly one `AcceptedRevision` and one
`AcceptedComparison`, and the workbench holds one alignment, so once the head
moves the old comparison's digest, line counts, segments, groups, and moves are
retained nowhere. Sending the current comparison's identities beneath an older
binding is the silent re-anchoring this continuation exists to prevent, so the
continued turn freezes no per-path identities and carries no file receipt and
no semantic selection — the documented *empty is a complete answer* case, in
which every finding the turn returns arrives unanchored. Re-opening the old
comparison would need retained epoch history or a re-alignment request against
the binding's epoch pair; that is an owner decision and is recorded in
`docs/verification/RFC-BPR-0012.md`.

`agent_invocation_request` gained one field, `ContinuedSession`. Engaged, the
turn runs under exactly that session: no identity is allocated, and the frozen
snapshot's `Binding` is read out of it — so *the snapshot's binding is the
session's binding* stays a property of the construction rather than of a branch,
while letting the session be an older one. `Context.Binding` must equal that
session's own `Binding`; a turn whose two revisions disagree is refused
`agent_continuation_binding_mismatch` and runs nothing. Absent is the ordinary
case and the only one that reaches the catalog, unchanged.

`github_application.cpp` is digest-pinned; both values were repinned in two
passes, the second drift hidden until the first was fixed.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The continued turn's snapshot binding equals the kept session binding and differs from the observed head | `ctest --preset dev -R '^application_agent_chat_continued_contract$' --output-on-failure` | Passed, 24.90 s. Immediately after the second send and before the run, the binding the MCP helper is pinned to, the binding record the frozen snapshot carries on disk, and the kept session's own binding are byte-equal in `adapters::mcp::encode_revision_binding` spelling and are not the head the reviewer moved to; the freeze count rose by one, so the record is that turn's and not the first turn's. After the turn the chat still holds the kept session — same id, same binding, `SessionsAdopted` unmoved — so admission no longer retargets it | this commit |
| Deleting the old-revision composition fails it by name | delete `turnBinding = aTurnSession->Binding;` (`src/app/src/github_application.cpp:5173`), rebuild (exit 0), rerun | Failed as required: `agent-chat-composition-failure:continued-old-revision-composition`, with `pinned:` and `recorded:` printed as the moved head `6458d96b…` against `kept:` `f9108913…`, `freezes:1->2`. Restored byte-exact, md5 `f2df57bb9bc597101d5cb8d196bb7ad8` before and after | this commit |
| `start_new_session` and every other course still compose from the current revision | `ctest --preset dev -R '^(application_agent_chat_[a-z_]*contract\|application_agent_finding_[a-z_]*contract\|application_agent_invocation_composition_contract\|application_mcp_context_helper\|composed_application_hosts\|application_review_round_trip_composition_contract)$' --output-on-failure` | 15 tests, all passed, 235.86 s | this commit |
| The two app-source digest pins are repinned | two passes over `application_review_round_trip_composition_contract` | `bpr_expected_full_application_digest` → `c7560dfced46883c190b09a606c3139dd60f01e0c01d383066108e4d6be5f162`; the second drift was hidden until the first was repinned, and `bpr_expected_production_app_manifest_digest` → `51adae13d153d41875f6351ef6f02f890b3df03070e361ab22f7ab0dc0c3cf70` | this commit |

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0), `check_boundaries` exit 0, and ran the four
chat contracts, the seven finding contracts, the invocation contract, the
MCP helper, `composed_application_hosts`, the round-trip contract,
`application_composition_contract` (117.32 s), and the two MSVC-shape
gates: 18/18. `clang-format` clean by exit code on the four touched C++
files. The limit the slice states is accepted as fact: the old revision's
binding and file bytes are recoverable and its alignment is not, so a
continued turn freezes no per-path identities; whether to retain epoch
history or re-align the old pair is the owner's, recorded in the ledger.

## Owner decision — 2026-09-02

The alignment question this slice left open is decided: the owner accepted
that a turn continued against an old revision returns its findings
unanchored, and that this is the product behaviour rather than a gap to
close. No epoch history is retained and no re-alignment request is added.
The alternative — retaining epoch history so continued turns can anchor —
is recorded as [[BPR-0532]] in backlog for whenever anchored continuations
are wanted, and is not part of RFC-BPR-0012's closeout. [[BPR-0256]] no
longer waits on this card.
