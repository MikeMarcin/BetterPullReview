# Remembered CLI connections continuation

Mandate `remember-cli-r1`; implementer `/root/remember_cli_m2` on `main`.
RFC baseline `46eb49b312b35ef37e69ca4ace7354c04295a6dd`. M1
`connection-metadata` and M2 `shared-profile-restoration` are accepted.
RFC-BPR-0071 remains In Progress; complete qualification and closeout are M3.

## M2 candidate and evidence

- Claim: `65191bb9bd924f13c267d6fa143405afdb669521`.
- Runtime source: `268b87049592ca4643e5303ab77114e116e7b5f3`.
- Final scoped retirement source: `a09ff90c704f534c7e13c809d50b113042bd262a`.
- M2 acceptance/bookkeeping: `352b67968b2d2b6d150abc0b16c1d7f721d68e9d`.
  BPR-1409 is Done; BPR-1410 is Ready.
- [M2 acceptance](m2-acceptance.json), [verification](m2-verification.json),
  [exact final-source checks](m2-focused-evidence.json),
  [earlier exact-source checks](m2-initial-focused-evidence.json),
  [binding](m2-binding.json), [observation](m2-observation.json) and
  [continuation check](m2-check.json).
- The serialized writer and real composed app lifetimes cover prompt save,
  empty-inventory restart, pinned numeric refusal, retained-choice retry,
  Sign out, GHES Forget/reopen and Quit drain. Writer fault courses cover
  failed remember followed by disable/Forget, failed disable and retry,
  refused load and queued late saves. Three composed regression rows pass;
  provider/profile, architecture/store/isolation, format 22, focused tidy and
  documentation checks pass. Details and corrected failures are recorded.

## Implemented runtime boundary

Startup loads the private metadata store before authentication selection or
identity allocation. Saved host/profile identity is reconstructed; transient
and replacement identities allocate above retained counters. Retained startup
navigation uses its exact server choice; without a destination the latest
enabled choice is tried. CLI validation uses the saved host/login and rejects
numeric account mismatch before accepting an authenticated session. It needs
no inventory or repeated read-trust prompt. The consent wording states reuse.

The shared profile observes accepted authenticated sessions and saves promptly,
independently of layout. The writer separates logical session state from the
last committed snapshot and rebases each serialized operation on the latter;
a failed save cannot skip the store's required revision. Only the latest
queued operation owns the visible result. Sign out disables reuse. Forget
removes the row and retires same-server setup in every sibling window, pending
device work and active authority. Successful native replacement receives a
fresh profile identity; manual tokens remain session-only. Refused metadata
starts signed out without overwriting data. Once metadata has recorded a
decision, absent rows also suppress legacy device fallback. Orderly teardown
closes ingress and drains admitted writes before releasing effects and storage.

## Next milestone: restart-qualification-and-closeout

Claim the Ready BPR-1410 card with the shared helper. Read the live RFC, authored plan, both immutable
inputs and these evidence records. Preserve the full AC01–AC06 boundary:
fresh-process restart and explicit last-window closure, multiple named accounts
and servers, exact-host private restoration and sibling connection work,
missing/refused/mismatched CLI retry, older device grants, native replacement
and late completions, durable save/refusal/cancellation/crash, maintenance,
tokenless execution, global CLI state and independent publication consent.
The new test restarts composed app lifetimes inside one test process; it is
not the required new-process qualification. M1/M2 evidence may be reused where
unchanged but does not close the RFC. Add discriminating composed courses and
baseline comparison before closeout. No human/native/independent acceptance is
inferred. Finish every agent-doable check before parking owner-only judgment.

BPR-1373 remains separately parked. No owner desktop was driven/captured;
no personal app state was read; no live GitHub calls or global gh account
switches were made. Frontend-design was loaded before the consent wording
change; the real offscreen consent dialog was inspected. No attended candidate,
review session, protection or server was created for this milestone.

## Ownership and command state

All task-owned commands are complete at handoff; no task-owned server remains.
Shared build/dev and build/core-only trees are retained. Existing release
ctest PID 97841 belongs to another writer and was left alone. External Windows
title-row and C++ identifier-context commits and their staging were preserved;
a transient mid-edit external compilation failure was not repaired or reverted.
The index is reconciled before each explicit-path commit. Original dirty
`.gitignore`, `.mask.json`, `.mask.lock.json` and untracked `docs/rfcs/drafts/`
remain excluded. Task-owned temporary edit scripts were removed. Local build/test logs and the
offscreen inspection image under `/tmp/bpr-m2-*` contain fixture evidence only;
durable results are recorded here.

The durable observation precedes its own bookkeeping commit. A fresh exact-HEAD
check is returned to the coordinator. Product acceptance binds the final source
revision above; subsequent task changes are bookkeeping only.

## Prior accepted milestone

## Candidate and evidence

- Adoption: `3670a65e3`; claim: `a42c8de9f`.
- Product source: `44a909e721ab461bbdfe986515a66d285f1fc5f6`.
- Test containment registry: `6ebd33e57479a24aed91b930624bab030a436045`.
- [Acceptance records](m1-acceptance.json), [verification details](m1-verification.json),
  [exact product-source test output](m1-focused-evidence.json),
  [assigned binding](m1-binding.json), [milestone observation](m1-observation.json),
  [continuation check](m1-check.json) and [pinned strategy selection](selection.json).
- Four factory/core/maintenance tests pass against the product commit. Seven
  integration checks pass, including both architecture gates and isolation
  manifest. Registry integration has a subsequent passing manifest check.
  Format 22 and focused tidy pass; documentation gate and its 67-test suite pass.
- A deliberate early COMMIT causes the cancellation rollback assertion to fail;
  restored production source rebuilds and all focused tests pass.

## Implemented capability

`src/core/include/bpr/core/remembered_connection.hpp` defines one canonical-host
row per BPR profile, complete snapshots with revision and retained identity/order
high-water marks, exact-host/latest selection, remember, disable and Forget.
Metadata has no credential, reading authority or publication consent. Stable
profile/host IDs cannot be silently rebound to different authority. Disabled
identity-free sentinel rows suppress older device grants even when sign-out
precedes the first successful remembered selection. Forget retains counters.

`src/application/include/bpr/application/remembered_connection_store.hpp` is a
synchronous narrow port for a serialized effect lane. The SQLite factory binds
the profile and its own `state/v1/remembered-connections.sqlite3`, uses exact
versioned schema validation and bounded reads, and commits metadata plus all
rows together with a revision comparison. Refused malformed/newer data is
preserved. The established private-directory/open helpers and connection policy
are reused. Full profile purge includes the metadata and its SQLite sidecars.
The hostname grammar moved into core without changing its public application
aliases or grammar.
