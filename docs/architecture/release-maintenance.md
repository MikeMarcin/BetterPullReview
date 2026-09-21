# Release maintenance policy

RFC-BPR-0014 M3, BPR-0953, implements the deterministic part of G5/G6 from
the [release allocation](release-readiness.md). The public core API is
`src/core/include/bpr/core/release_maintenance.hpp`; its implementation and
direct course are `src/core/src/release_maintenance.cpp` and
`tests/core/release_maintenance_tests.cpp`. This is a core contract. M4 owns
native observations/effects, M5 the controls, and M6 their public composition.
It is not evidence that a diagnostic export or profile purge is already shipped.

## Authority and state

`release_maintenance_authority` identifies a BPR profile, a host runtime session
and an opaque admitted root identity. These are separate from GitHub profiles,
accounts and PRs. The host supplies the root identity only after native admission
and changes it on root replacement. A numeric root ID, path, label or mtime
alone never grants a filesystem or vault capability.

State retains one pending operation and one prepared selection descriptor;
the normal transition never retains both simultaneously. The record is trivially
copyable and compile-time bounded to 512 bytes. It retains no snapshot bytes,
inventory entries, native handles, file paths, provider content or credentials.
Every request/completion carries authority, request ID, generation, operation
and the complete selection descriptor. Equality and refusal work are fixed-size.
Request and generation counters never wrap or restart during a state lifetime.
The host must use a fresh session identity for a new runtime.

Capabilities default to unavailable. Local diagnostic operation needs no GitHub
account; composition supplies its local capabilities independently of sign-in.
The seven command projections use the same admission policy as transitions.
`StatusChanged` asks the host to update status and the seven command projections;
`SelectionChanged` and `AuthorityChanged` identify their separate bounded domains.
No update requires rebuilding review state or scanning history.

## Prepare, confirm and complete

| Gesture | Request / accepted observation | Next explicit gesture |
|---|---|---|
| Preview diagnostics | `preview_diagnostics` → `release_maintenance_prepared` with immutable snapshot descriptor | Export that exact preview |
| Prepare clear | `prepare_clear` → prepared complete diagnostic inventory | Confirm clear for that exact inventory |
| Prepare purge | `prepare_purge` → prepared complete profile inventory | Confirm purge for that exact inventory |

Preparation performs no export or deletion. Export/clear/purge require the
entire accepted prepared context as the intent's `Target`, including its
originating request and generation. Execution consumes that selection and
issues a fresh identity carrying the same descriptor. A second execution,
another profile/root, changed revision/digest/count/category, or mismatched
preparation kind refuses. A failed export needs a new preview. Destructive
confirmation remains a Qt responsibility with Cancel as default/Escape in M5.

Cancel also names an exact `Target`: the pending context or the prepared
context being dismissed. A delayed cancellation cannot cancel a newer operation
in the same profile. Pending cancellation is advisory and retains the owed
terminal. A preparation arriving after cancellation cannot publish a preview.
Host source/selection invalidation and authority replacement advance generation,
retire old selections and request exact cancellation when still possible.
Retired/duplicate/mismatched events change no core state and request no effects.

For export and deletion, the host serializes `release_maintenance_commit_started`
before the durable terminal. This reports the irreversible boundary; it is not
a new authority grant. The host must check cancellation, current selection and
its admitted capability before crossing that boundary. Cancellation that races
it may still produce actual success. After that observation, cancellation is
refused; only durable completion or typed failure can retire current work.
Success without the boundary observation is a protocol error. A destructive
failure after the boundary, or invalidation while irreversible deletion is
pending, requires recovery and prevents further admission. The host rebinds
only after actually reconciling recovery. Successful profile purge stops this
runtime permanently; a new profile lifetime needs fresh admission/state.

`Accepted` means the event was handled, not that its operation succeeded:
malformed prepared payloads are consumed as reason-coded failure terminals.
Shutdown closes admission and retires identity. The host still owns every
retired effect and must drain/recover it before releasing its native lease;
core invalidation is not proof of physical cancellation or quiescence.

## Diagnostic privacy and limits

The export policy admits four classified field classes: fixed operation,
fixed outcome, typed failure code and numeric counts. The default mask contains
only those classes. Summary prose, identities/paths, source/comment bodies,
private notes/drafts/agent text, credentials, crash memory and unknown fields
are excluded even if a producer labels a summary sanitized. This deliberately
does not copy the existing readable local log into a support bundle.

The host prepares at most 1,024 records and 256 KiB of immutable serialized
metadata. The snapshot carries a nonzero revision, a nonzero 32-byte digest,
allowed field mask, exact record/byte counts and omitted-record count. An empty
record set can still have a nonempty format header. A malformed identity, unknown
field, zero byte representation or exceeded limit fails before preview/export.
The limits are conservative admission ceilings, not measured latency promises.

M4 must parse known log vocabulary into typed metadata before serialization;
omit unclassifiable legacy prose with truthful counts or refuse. It must bound
input work and count arithmetic as well as output, compute/verify the actual
snapshot digest, retain exactly the previewed bytes, and use an independently
admitted local output capability. The core accepts no content bytes and therefore
cannot prove a producer classified them correctly. Neither a digest nor a field
mask is a redaction certificate. No request contains an upload operation.
Canary inspection of actual files and external OS crash reports remains M7/M9.

## Purge scope and host obligations

Clear admits only diagnostic artifacts. Profile purge admits exactly these six
categories together: diagnostics, private state stores, retained history,
agent workspaces, MCP snapshots and exact credential bindings. Omitting a
category is refused, preventing state/binding metadata from being selected
independently of credential cleanup. Absent categories still appear in the
complete inventory with zero native targets. A zero-entry complete inventory
is a legal no-op; an incomplete inventory is not.

The descriptor binds a nonzero revision/digest and at most 4,096 exact native
entries. Each sidecar and vault binding counts; the adapter must bound inventory
work and refuse overflow rather than truncate. Unknown entries in the selected
owned scope block destructive confirmation. A changed native target requires
fresh preparation. Profile registry, updater state/transactions, sibling
profiles, developer worktrees, exported files and shared GitHub state are
outside the permitted mask. Classifying a known sibling as excluded is not
permission to traverse/delete it or to call an unknown owned file safe.

M4 must retain and revalidate the exact native inventory behind the digest;
never recursively delete an application-data root. In particular, the default
root cannot consume its nested `profiles/` tree. Stop ingress, invalidate window
generations, drain producers, close stores/logger, retain root ownership, and
then perform exact cleanup. Delete known credential bindings through their
existing owner before deleting binding metadata. Failures need explicit,
recoverable dispositions for journals, tombstones, WAL/SHM files and changed
objects. Persistent recovery belongs to the host/adapters, not this ephemeral
core state. Shared registry/updater and all-profiles uninstall courses remain
separate obligations.

## Reused core regions

| Region | Existing source and direct test evidence | M3 disposition |
|---|---|---|
| Retained epochs/cache deletion | `epoch_model.hpp`, `epoch_transition.cpp`: exact catalog/cache/epoch identity, bounded target digest, cancellation and durable terminals; `core_epoch_policy`, especially `test_core_authorized_epoch_maintenance` | Reused unchanged; no second retention state machine or automatic pruning policy |
| Migration value semantics | `appearance_model.cpp` typography/Dif validation and `theme_exchange.cpp` bounded theme parser; `core_appearance_policy`, `core_theme_exchange` | Reused unchanged; schema versions, transactions and foreign/future-store refusal remain each SQLite adapter's authority |
| Profile lifecycle | `profile_window.hpp/.cpp` bounded stable IDs, window generation, restore/refusal and retained/live ownership; `core_profile_window` | Reused unchanged; purge does not delete the registry or replace its allocator |
| Update policy | `release_update.hpp/.cpp` stable version/asset selection and lifecycle; `core_release_update` | No impact; updater state/installation is outside profile purge |
| Credential sanitization and migration effects | Credential-owning GitHub adapter, vault/device-binding hosts and store-specific SQLite implementations | No core effect added; M4 qualifies actual lifecycle/migration and M9 actual privacy/native artifacts |

`appearance_settings_database.cpp::read_legacy_settings` calls the existing
core theme importer and typography validator before migration; its
`establish_schema` owns version/application identity. Workbench schema
preflight and migration likewise remain in `workbench_settings_database.cpp`.
Core tests establish semantic reuse only; they do not replace M4's transactional
old/future/foreign/corrupt-store and restart tests.

The current `publish-release` skill, release/publish scripts and presets remain
the authoritative package workflow, unchanged by M3. RFC-0027's broad verbosity,
rotation/anonymization controls and optional context ring remain deferred.
No RFC criterion, platform/native requirement or publication permission changed.
