# Remembered CLI connections continuation

Mandate `remember-cli-r1`; assigned implementer `/root/remember_cli_m1`; baseline
`46eb49b312b35ef37e69ca4ace7354c04295a6dd`. First milestone
`connection-metadata` is accepted as the policy and storage capability boundary.
RFC-BPR-0071 remains In Progress. Original drafts and amendment were adopted into
the immutable asset set; original draft files were preserved.

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

## Next milestone: shared-profile-restoration

BPR-1409 owns runtime composition and all its lifecycle changes. Claim its Ready
card with the shared helper. Read the live RFC and both immutable authored inputs
again, then exercise the shortest actual app startup/completion path early.

- Load remembered state before authentication restoration and before allocating
  IDs that could alias saved identities. Reconstruct stable profile/host identity
  and advance runtime allocation above the saved counters. Different account or
  transport needs a fresh Profile; same hostname keeps its Host. Forgotten hosts
  need a new Host above the retained high-water mark.
- Admit remember only after explicit transport choice, read consent and successful
  authenticated identity observation. On startup revalidate the exact saved
  host/login with the pinned CLI transport and require numeric Account match
  before private state is exposed. No inventory/default-account fallback or
  repeated selector/trust step. Retained navigation selects its exact host;
  without a retained destination use the most recently selected resumable row.
- Share the metadata and saves through the profile runtime across sibling
  windows. Keep in-memory session state and the last durable snapshot distinct.
  The store accepts one revision step with expected durable revision, so failed
  writes must not be followed by a skipped-revision snapshot. Rebase a later
  desired durable transition from the last durable state (or serialize/retry
  admitted transitions), while retaining independent runtime generation guards.
  Tests must cover failed remember followed by sign-out/Forget/retry.
- Sign-out disables reuse of CLI and older native device grants. A sentinel is
  available when no row exists. Forget removes selection. A successful native
  connection replaces CLI only after its own success; a manual token remains
  session-only. Cancellation, failed switching and late completions cannot
  replace or re-enable the saved selection. Guard both selection revision and
  runtime request/cancellation identity.
- Schedule saves promptly and separately from layout persistence. A refused
  remember leaves an explicit session-only connection; a failed disable is
  visible. Drain admitted saves before orderly Quit/last-window teardown.
  Malformed/newer storage starts signed out and does not get overwritten.
- Preserve tokenless CLI process behavior, unchanged global `gh` account and
  independent publication consent. No `gh auth switch`.

## Remaining qualification and review

BPR-1410 retains full composed macOS AC01–AC06, including fresh-process restart
and last-window closure, named-account order and multiple hosts/profiles,
missing/refused/mismatched CLI retry, sign-out and older device resume, native
replacement and late completion, durable save/failure/cancellation/crash,
maintenance, tokenless execution and publication consent. M1 internal policy and
real-store evidence do not substitute for those courses. Windows containment of
the new test remains unmeasured and registered as such.

BPR-1373 remains independently parked. Its card records no frozen build. The
attended-review conflict query returned no conflicts for the planned paths. No
owner review, candidate, server or native session was opened for this milestone;
no owner desktop was driven or captured and no personal browser data was read.
If runtime UX changes, load the required frontend-design skill before that work.

## Ownership and command state

All task-owned build/test/tidy commands have completed. No task-owned server or
background writer remains. Existing shared `build/core-only` is reused and is
not removed. Task scratch under `/tmp/bpr-remember-cli-run` contains only local
fixture/build evidence and checkpoint orchestration; durable evidence is above.
Original dirty `.gitignore`, `.mask.json`, `.mask.lock.json` and untracked
`docs/rfcs/drafts/` remain excluded.

An external writer committed `f999d900e` and `32065db72` while M1 was running.
Its staging was preserved. The only overlapping work was our single test
containment registry line: after its exclusion from their release candidate,
we committed that line in our explicit integration commit. No other external
edit was included or reverted. The index is empty at handoff; any later writer
must be observed by the successor before claiming work.

The durable observation is a record made before its own bookkeeping commit; a
fresh exact-HEAD observation/check is also returned to the coordinator. Product
acceptance remains bound to the product revision above, with only registry and
bookkeeping changes afterward.
