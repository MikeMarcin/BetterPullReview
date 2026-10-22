# Release maintenance policy

RFC-BPR-0014 M3, BPR-0953, implements the deterministic part of G5/G6 from
the [release allocation](release-readiness.md). The public core API is
`src/core/include/bpr/core/release_maintenance.hpp`; its implementation and
direct course are `src/core/src/release_maintenance.cpp` and
`tests/core/release_maintenance_tests.cpp`. M4, BPR-0954, adds the host/native
contract below; M5, BPR-0955, mounts the diagnostic path and verifies the
maintenance presentation. M6, BPR-0956, supplies the retained-lease destructive
lifecycle and tests-disabled package composition described at the end. The M3–M5
sections retain their stage-specific obligations; M5's disabled destructive
capabilities are historical, not the current composition. Native vault, crash
privacy and full package removal remain M8/M9 obligations. Current user steps
and residuals are in [Storage, diagnostics and removal](../development/storage-and-diagnostics.md).

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

The host prepares at most 16,384 records and 16 MiB of immutable serialized
metadata ([[BPR-1722]]; 1,024 records and 256 KiB before it). The snapshot carries a nonzero revision, a nonzero 32-byte digest,
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

## M4 host and native effects

The public `release_maintenance_application_host.hpp` consumes the M3 core
protocol. Its storage, lifecycle, projection and local-output ports contain no
Qt or provider types. `release_maintenance_files.hpp` implements the native
storage/output capability under an existing, genuine `profile_runtime_lease`.
It retains native root authority, not merely the core's opaque numeric label.
Construction/recovery are effect-lane work before a new host is admitted.

### Decision lane and lifecycle

The host retains one physical job and one immutable preparation. A cancelled
or invalidated core request still owns its physical job and terminal delivery;
new work cannot overtake it. A bounded 32-entry ingress queue serializes
reentrant projection callbacks. Shutdown/invalidation takes priority. Projection
exceptions do not strand completions. Effect callbacks post to the existing
serialized decision executor; they never directly mutate core state.

The commit gate checks cancellation/revocation under the job's mutex. A terminal
that overtakes its queued commit notification first publishes the same exact
commit context. This preserves the core's irreversible-boundary protocol under
reordered delivery. `is_quiescent()` includes queued decision callbacks, not
only the apparent core status. Keep ports, effect/decision executors, the
independent vault worker and the root lease alive until shutdown drains.

Destructive preparation first calls the lifecycle barrier. Composition must
close ingress, invalidate affected window generations, drain producers and
existing Git/workspace/MCP recovery, and close stores/logger while retaining the
lease. It then permits exact inventory capture. Cancellation does not silently
restart those producers; continuing that profile may require an explicit reopen.
M5/M6 must wire this barrier to actual application ownership. A generic callback
or passing manual-executor test is not evidence of composed desktop shutdown.

### Metadata snapshot and output

`release_diagnostic_snapshot.cpp` classifies the legacy Quill v1 prefix and
BPR-1215/BPR-1217's canonical flat JSONL v2/v3 records into
closed operation/outcome codes and numeric HTTP status/dropped counts. It omits
timestamps, request/session identities and all free text. In particular,
`detail=` is an unframed tail: even a later apparent `failure=` token can be
producer prose, so the default export omits the entire tail. This is a stricter
subset of M3's allowed fields, not a claim that arbitrary local prose is safe.
Unknown, malformed and incomplete records are omitted and counted.

Input is bounded to 32 MiB and 131,072 records (16 MiB and 65,536 before
[[BPR-1722]]). The v1 line bound is 8,192
bytes; v2/v3 are at most 16 KiB including the newline. The JSONL reader requires the
writer's exact field order, bounded ASCII strings with quote/backslash escapes,
and unsigned decimal integers. Unknown keys, duplicate keys, unsupported
encodings, malformed syntax and partial tails are omitted without pass-through. Output is
bounded to 16,384 records and 16 MiB. Input/work overflow refuses rather than
silently scanning unbounded history. The host verifies the exact prepared
bytes, count and SHA-256 against the descriptor. Preview and export share the
same frozen bytes, with no raw log, source/comment/secret, database or crash file.

Local output admission retains a canonical, owner-controlled parent and refuses
existing destinations. A private temporary file is published exclusively after
the commit gate: a hard link on POSIX, a no-replace handle rename on Windows.
Native identities protect temporary cleanup and changed-parent refusal. No
overwrite or upload API exists. If publication succeeds but a final durability
check fails, an error can leave that user-selected file; callers must not claim
that every failure rolls back a published export.

RFC-BPR-0064 adds a read-only initial-directory observation. On each Save,
composition's effect worker asks platform standard-folder discovery for Downloads,
Documents and home, in that order. The output adapter shares its native parent
admission with this observation and requires current write/search access. No
folder is created, enumerated or chmodded; no filename is admitted or saved yet.
Empty, missing, unsafe, unwritable and volume-root defaults are refused. Qt
receives only an admitted initial path or typed unavailability and sets the native
chooser directory explicitly before opening, with the existing filename/suffix.
An unavailable observation preserves the preview and enables a new Save retry.
Completion is scoped to the still-current prepared preview; cancellation or
replacement cannot reopen its chooser. Help/Settings and both privacy modes use
this same path. Each invocation rediscovers the locations, independent of cwd or
dialog history. The separately selected filename still goes through ordinary
output admission at write time; this observation supplies no write authority.

### Native inventory, vault ordering and recovery

Both POSIX and Windows enforce native private-storage admission, no symlink/
reparse traversal, regular-file single-link checks, exact object identities and
same-volume containment. POSIX identity includes device/inode/size/mtime;
Windows retains the volume and full 128-bit file identity, checks protected
private ACLs and holds no-delete-sharing handles. Windows lease validation now
verifies the genuine process-owned native lease rather than accepting a path
or a user-constructed lease value. Shared private-storage primitives are in
`src/adapters/native/`; they add no core dependency. The Windows owner-private
descriptor they create and check objects with is the header-only platform
target's (`src/platform/`), the same one the agent and Git workspace objects
use ([[BPR-0825]]).

The closed inventory admits known private databases and their exact sidecars,
the one diagnostic log, and complete manifest-identified managed Git caches.
It bounds depth (16), relative path bytes (1,024) and total native/vault targets
(4,096). Unknown entries, unsafe objects, changed identities, partial incoming
spools/quarantines, pending original-owner recovery, or populated adjacent agent
workspaces refuse preparation. They must be reconciled by their current owner;
there is no recursive-delete fallback. `profiles/`, `updates/`, shared profile
registry/updater databases and the live profile lease remain excluded without
traversing them.

`release_profile_credentials` inspects at most 64 remembered bindings, including
tombstones, through the existing device-binding store. It closes that store
before native inventory. The digest covers inert exact binding identities;
cleanup deletes both token slots through `github_credential_vault_port` without
reading a token or enumerating a namespace. Every callback must match its exact
request. An absent slot is idempotent success; any failure preserves metadata
and the recovery journal. The vault must run on an independent live worker.

Before deletion, `.bpr-maintenance-v1` persists the bounded exact inventory and
its SHA-256 as `BPRMNT03`. After exact credential cleanup, a separate
`BPRCRED1` receipt bound to that inventory digest is appended and flushed before
any binding metadata is removed. A torn receipt safely retries the same known
keys. Recovery accepts only missing members of that exact inventory; new or
replaced objects, changed root/profile authority, corrupt inventory or foreign
receipt refuse cleanup. A torn initial journal has not authorized native file
deletion and requires explicit repair; it is not permission to guess targets.
Exact regular files are removed before empty directories, and the journal is
removed last. The host remains recovery-required after irreversible failure
until actual recovery and a fresh runtime admission.

Mandate r3 prescription correction: ordinary epoch retention/cache deletion
continues using its existing catalog transactions, tombstones and recovery.
Whole-profile purge instead journals the closed, manifest-validated managed
cache objects together with the catalog and other profile artifacts. A catalog
that is itself being removed needs no second per-cache transaction history.
This avoids competing recovery owners while retaining exact authority, bounded
inventory, pre-deletion durability and restart refusal for changed objects.
Unresolved original-owner state still blocks the purge; this is not a waiver
of recovery or an alternate retention policy.

Successful profile purge intentionally leaves the root/runtime lock, excluded
shared containers/stores, any empty state parent containing shared entries,
and an admitted empty adjacent workspace directory. Exported files remain the
user's property. These are explicit residuals, not a full uninstall result.
All-profiles/shared-registry removal and package uninstall are M6/M8 work.

### Existing logger, migration and evidence boundary

The existing Quill sink now supports Windows using the same private ACL/handle
primitives and exposes only fixed admission-failure reasons. POSIX and Windows
open an already admitted root and create only its `logs` child; neither walks
missing ancestors into existence. Writers are exclusive. The existing queue,
16 MiB file ceiling and producer limits remain; no second logging system,
rotation policy or RFC-0027 context ring is added. Native fixture tests are
registered on Windows as well as POSIX. BPR-0885/BPR-0891 overlap this source
work but remain unclaimed; their own acceptance/native evidence is not inferred.

Store migration remains in the established per-store transactions. Focused
appearance, workbench, device-binding, epoch, queue, ledger, mutation-recovery
and restoration courses cover their existing migration/foreign/future/corrupt
and persistence behavior. The real development-app retention course continues
to exercise seed → prune → restart/cache-delete → restart/verify.

Direct new tests are `application_release_maintenance` (manual executors,
cancellation/reordering/retirement/reentrancy, typed failures and hostile
metadata) and `sqlite_release_maintenance_files` (real disposable roots,
preview/export identity, native permissions/containment, credential failures,
crash/recovery, corrupt receipt and replaced-object refusal). Logging and
profile public courses exercise the changed real sink. Cross-compilation is
only Windows source/contract evidence: MSVC linking and native Windows ACL,
vault and lifecycle execution, Linux qualification, native crash artifacts,
Qt controls and tests-disabled delivered packages remain M5–M9 obligations.

## M5 Settings presentation and bounded composition

`release_maintenance_projection.hpp` is the effect-free presentation boundary:
compact core state, seven command projections, a shared immutable preview,
the admitted operation's identity for terminal wording, and a diagnostic reason.
It exposes neither an effect request nor a storage/output/vault port. The Qt
`release_maintenance_panel` translates gestures only; it does not determine
command eligibility, read files, perform deletion, choose credential keys or
construct a native output capability.

Storage & privacy in the real profile Settings shell includes the new panel
alongside the existing epoch/cache retention controls. Those controls retain
their own policy, dispatcher and confirmation; no second retention owner was
introduced. `release_maintenance_controls` in `src/app` creates native diagnostic
storage on an owned worker under the retained runtime lease, then publishes on
the Qt decision lane. Its local capability is independent of GitHub sign-in.
The save picker supplies only a local filename and the complete accepted
`Target`; native output admission and writing happen on the worker. Existing
destinations are refused, not overwritten. Failed export needs a new preview.
The shared panel follows the profile Settings shell when its invoking window
changes; retirement drains the host and worker before releasing profile ownership.

Destructive presentation is complete against the real application host with
controlled storage/lifecycle capabilities: preparation warns that work and
stores stop, explicitly asks the user to finish/save unsaved work, and performs
no deletion. Final clear/purge confirmation identifies the profile, exact
inventory count, included categories and retained residuals. Cancel is the
default and Escape action at both stages. Confirm/cancel echo the full context
that was displayed, not a reconstructed current target. Authority, pending or
prepared-selection changes dismiss stale dialogs. Settings/page dismissal
cancels the exact current target; cancelling a save chooser alone retains the
preview. Status distinguishes actual success, refusal, advisory cancellation,
stopped runtime and recovery-required partial cleanup, without promising a
rollback or silent producer restart.

The live M5 composition deliberately supplies `Clear=false` and `Purge=false`,
and its storage wrapper refuses non-diagnostic preparation/deletion. The real
multi-window retirement, ingress/generation invalidation, Git/workspace/MCP
recovery, store/logger closure and independently retained vault worker are M6
obligations. The existing general shutdown path is not proof that those owners
have closed while the maintenance host and root lease remain usable. Enabling
destructive commands before that barrier exists would violate the M4 contract.
This preserves the authored M5 presentation/M6 composition boundary, rather
than treating a widget test as production purge acceptance. Native all-profile
removal, uninstall, credential continuity and delivered packages remain open.

Projection updates touch seven actions and the small status/summary surfaces.
An unchanged preview pointer does not rewrite its document, selection or scroll;
equivalent command projections do not disable/re-enable focused controls.
Status changes emit one polite accessibility announcement. Labels, profile names
and the preview are plain text. There is no stylesheet, custom font or platform
style override. No comparison, review, navigation or history model is reset.

Direct courses are `qt_release_maintenance_scale_100/125/200` (rendered real
Settings/widget hierarchy, typed host, manual lanes and controlled effects) and
`application_release_maintenance_public_path` (actual profile desktop, native
private roots, keyboard preview, exact UTF-8 local export, existing-file refusal,
shared Settings ownership and shutdown). Cocoa execution supplies native macOS
widget/effect evidence but uses a deterministic Qt save picker. It is neither
an attended VoiceOver/native-picker verdict nor tests-disabled package acceptance.

## M6 retained-lease maintenance composition

The desktop now supplies a concrete retirement callback to the same maintenance
host. Destructive capabilities exist only with that callback; purge additionally
requires the native vault capability. Preparation transfers the invoking
profile's Settings shell and one maintenance panel to a desktop-owned holder,
unmounts borrowed window pages, refuses profile ingress/activation, and retires
every window through the existing generation/registry lane. Original Git,
workspace, MCP, credential and shutdown owners drain their work. The desktop
destroys the complete original runtime only after shutdown completes: its
stores, ports and Quill sink cannot keep writing behind a successful barrier.
Cleanup failure returns recovery-required, never a successful preparation.

The maintenance holder retains the original exclusive root lease, its own
effect worker and an independently constructed native vault/worker. Binding
stores are opened lazily after retirement, so exact credential cleanup precedes
binding metadata removal. No old window, provider or runtime is restarted by
cancel, clear, purge or retry. The maintenance-only Settings shell remains
usable for explicit re-preparation; closing it drains the maintenance host and
releases the lease. Another profile's runtime is not purged or retired.

Fresh startup executes the existing native maintenance recovery under the
exclusive lease before profile stores, logger or new producers open. An invalid
or unrecoverable receipt refuses admission and preserves evidence. Unknown
objects still fail closed: the expanded inventory recognizes only the real
Git isolation files, empty isolation-directory shells and fixed 16-hex epoch
ref grammar, not arbitrary descendants left by a producer.

The direct public course starts an actual fixture PR and `review.cpp`, opens a
second window, retires both window surfaces, and proves the
original profile owner expires while the native lease stays held. It covers
cancellation without restart, changed-inventory refusal, explicit retry,
diagnostic-only clear, full populated-profile purge, preserved external export,
lease release and corrupt-receipt startup refusal. The native vault is composed;
real persistent credential/crash continuity and all-target acceptance remain
M8/M9 evidence, not inferred from the fixture's synthetic sign-in.

## RFC-BPR-0027 session retention integration

BPR-1216 replaces the fixed-file sink with coordinated per-session segments.
The application contract owns their closed leaf-name grammar so the logging and
maintenance adapters agree without an adapter dependency. Preview enumerates
only recognized private files under its existing 16 MiB/65,536-record bounds;
unknown entries fail closed. An incomplete tail is made unparseable before the
next file is considered and contributes one omission. The exact frozen metadata
preview and local export authority remain unchanged.

Clear/purge captures all recognized segments and the legacy file after producer
retirement. It preserves `logs/.retention.lock` and the containing directory so
another cooperating owner cannot acquire a replacement coordination inode.
Regular session retention protects active current files, rechecks private native
identities and bounds directory inspection to 128 entries. Tests cover separate
processes, native write failure, rotation obstruction, age, capacity, public
startup cancellation and ordinary shutdown. These Mac results do not supply
Windows/Linux native acceptance or crash durability.

BPR-1217 adds `diagnostic-preferences.sqlite3` and its exact known sidecars to
profile purge. Its live logger Clear uses the existing retention transaction and
native identity checks, refusing another active session before deleting anything.
It invalidates queued generations, clears transient context, removes owned log
segments and opens a fresh segment before acknowledging success. A deletion or
reopen failure is explicitly partial and disables the sink. The stopped-profile
prepared inventory and purge authority above are unchanged. Metadata preview
recognizes canonical v3 but continues to omit all identities and prose; richer
privacy-aware export is described below.

## RFC-BPR-0027 frozen diagnostic exports (BPR-1218)

The legacy metadata classifier and API above remain supported. The shipped panel
now requests the richer `bpr-diagnostics-v2` JSONL document. Core options bind
Anonymized/Identifying, retained/current sessions and Include recent to the exact
request and descriptor. A mismatched prepared policy is refused. Identifying save
requires explicit confirmation through the identifying preview’s Save action,
and consumes that exact preview.
Generic raw summary/identity field flags remain forbidden; rich fields have
separate classified-detail, correlation and captured-identity classes.

The profile composition retains its logger control port alongside native storage.
Preparation runs on the maintenance effect worker: the logger mutex admits a flush,
session sequence cutoff and optional bounded sanitized context; current-session
file selection filters the native inventory before reads. A later same-session
append beyond the cutoff is excluded. Other retained sessions are read into bounded
immutable input; files are never appended to, rewritten or attached to exports.
Malformed/partial records have counts. The 32 MiB/131,072 input and
16,384-record/16 MiB output ceilings apply (raised by [[BPR-1722]]); the optional ring has its independent
512-event/2 MiB ceiling. Duplicate session/sequence pairs are counted once.

The application classifier rechecks the canonical schema, fixed field vocabularies,
framing, times, build and identity grammar. It retains only recognized closed
capability explanations/public symbols, omitting other prose explicitly. A native
fresh random namespace supplies per-export opaque session/request/window joins;
no unsalted identity hash is used. Identifying output includes only already
captured numeric identities and the session. Both policies preserve capture privacy
and disclose irreversible omissions. Version-one input retains metadata only.
The header and projection report scope, range, categories, bytes/counts and loss.
A prepared recent-context snapshot is deliberate preview state until consumed or
cancelled; no new events enter it. Scope changes and hiding the panel cancel it.

[[BPR-1717]] makes both ceilings keep the newest records. Session files are
named for a random session identity, so name order is not age order. Until this
change, files were read in name order and the first records read were kept. An
hours-long running session whose identity sorted after older sessions' was then
left out of the export. The reader now takes files newest first by modification
time while they fit the 16 MiB input. Once one does not fit, it and every older
file are left unread and counted as `unread_files`, rather than refusing the
export. The files are then joined oldest first. The rich classifier orders every
admitted record by time, then session and sequence, and treats version-one lines
as the oldest. It keeps the newest run that fits 1,024 records and 256 KiB less
a 4 KiB reserve for the header and the Insights section, and writes that run
oldest first. The metadata classifier keeps its newest 1,024 records the same
way. The header separates `unclassified_records` from `limit_omitted_records`,
states `limit_keeps: newest`, and `omitted_records` stays their sum. The
snapshot carries the over-limit count and the unread-file count, and the panel
says how many older records and files were left out. A record is about 800
bytes, so under the limits of that change a retained-sessions export held
roughly the newest 300.

### Limits sized for a day of heavy use ([[BPR-1722]])

The owner asked for limits that make an export useful for diagnosis. They are
sized from the fixture courses' own diagnostics files:

- **Record size.** A record on disk is 706 to 867 bytes, 783 to 795 on average
  across the Home discovery and logging courses. An exported record is about
  the same; the owner's two exports averaged 801.
- **Rates.** A session start writes about 15 records (application, viewer,
  nine capability responses). Opening a pull request writes about 30 to 45:
  its page responses (13 for the fixture's pull request) and its epoch
  stages. A Home refresh writes about 5 to 10 at standard detail (start, one
  line per read, end; the fixture course's seven refreshes wrote 37), and Home
  refreshes at most every 5 minutes on its own. Heavy use, 15 pull requests
  opened and 20 Home refreshes an hour with comments and a few submissions,
  comes to about 1,000 records an hour, and 8,000 for an eight-hour day.
- **Output: 16,384 records and 16 MiB.** About 16 hours of heavy use, or two
  ordinary days. At 1 KiB a record, wider than any measured, 16,384 records
  fit 16 MiB, so records with long classified details are the ones that stop
  at the byte limit first. The newest are still kept and the rest counted.
- **Input: 32 MiB and 131,072 lines.** Files are read whole, newest first, and
  a session file is at most 5 MiB, so at least 27 MiB, about 35,000 records,
  is read whenever the logs hold that much. That is more than the output keeps,
  so the output limit, not the input, decides how far back an export reaches.
  Retention keeps up to 50 MiB (about 66,000 records, nearly three heavy days);
  reading all of it would only feed records the output drops. The line bound
  keeps the old ratio of one line per 256 input bytes.
- **Cost.** The reader releases each file once it is joined, so it holds about
  one copy of its input: at most 32 MiB of input, 16 MiB of output and an
  index of the admitted records. `application_release_maintenance`, which
  prepares an 8,000-record, a 16,394-record and a 17 MiB export, runs in 3 s
  in an unoptimised build. The preview showed a 16,384-record (13 MiB)
  document in 31 ms offscreen.

**Compression ([[BPR-1725]]).** A saved export is a standard ZIP
archive holding the text. The owner approved zlib's deflate for it on
2026-10-21, so `BprZlibProvider.cmake` now also compiles `deflate.c` and
`trees.c` from the same pinned 1.3.2 archive. No ZIP library is adopted:
LibArchive is pinned only for macOS update staging, with only its reader, and
the core already had a minimal container writer for review bundles
(`write_cycle_zip`), which now can deflate its members and stamp a time.

- **What is written.** `core::diagnostic_export_archive` gives one regular-file
  entry: the frozen text, deflated at zlib's level 6 (stored instead if
  deflate would not shrink it), with its CRC-32, compressed and uncompressed
  sizes in the local header and central directory, the UTF-8 name flag,
  owner-only mode 0600 (as the saved file and the earlier `.txt` had) and the
  local time to two seconds, then one end record. There is no
  data descriptor, extra field, comment, encryption or ZIP64; a 16 MiB text
  fits the 32-bit fields. Equal inputs give equal bytes.
- **Where.** The text is still decided by the application (classification,
  anonymisation, header, "Current session only" and the metadata policy) and
  is what the preview shows and the host digest-checks before saving. The
  file adapter's output (`release_maintenance_files`) turns that text into the
  archive at write time, on the effect lane, from the chosen file's name and
  its own local clock, and writes it with the same private temporary file,
  link and refusal of an existing destination as before.
- **Names.** The save dialog's filter and default suffix are `.zip`. The entry
  is the file's name with a final `.zip` (any case) replaced by `.txt`
  (`core::diagnostic_export_entry_name`); a name that is not UTF-8, holds a
  separator or control character, or is empty gives
  `BetterPullReview-diagnostics.txt`.
- **Size and cost.** A typical export of about 316 records (284 KiB) becomes
  an 8.5 KB archive, about 33:1, as the owner's hand-zipped 256 KiB exports
  became about 8 KB. A full 16,384-record export (14.7 MB) becomes about
  369 KB.
- **Checks.** Info-ZIP `unzip -t` and `ditto -x -k`, which Archive Utility
  uses, accept the saved file in the public path course and extract exactly
  the previewed text.

**Name and header.** The save dialog suggests
`BetterPullReview-diagnostics-YYYY-MM-DD-HHmm-VERSION.zip` (`.txt` before
[[BPR-1725]]; its entry keeps the `.txt` name) from the local
date and time (`core::diagnostic_export_file_name`): sortable by time, no
colon, and the version only when it is dotted numbers, so no host, account or
repository can reach the name. The header's first fields are `exported_at`,
the host's local time with its offset in ISO 8601
(`2026-10-21T14:32:05-07:00`), and `app_version` and `app_build`, the running
build as its records name it; each is `unknown` when not known. The metadata
(v1) export is unchanged.

**Review submission endings.** Until this change a submission was logged only
when its outcome stayed unknown ([[BPR-1540]]). A run whose outcome is known
now logs one `github.review_submission` record with the POST's HTTP status
and the line `review_submission_ending=ENDING run=RUN post=ANSWER
post_class=N refusal=REFUSAL stop=STOP comments=N duplicate=0|1`
(`core::review_submission_ending_diagnosis`). ENDING is `published`,
`refused` (a 4xx answer), `settled` ([[BPR-1715]]'s rule) or `not_sent`;
RUN is `submit` or `check`; REFUSAL is `none`, `review` or
`unplaceable_line`; STOP says why a run that sent nothing stopped. The logger
drops a record whose detail is not exactly that line, or that carries both an
ending and an unknown reason, and the export admits the line only when it
parses back. It names no host, repository, account, path or comment. A
refused or unsent review counts as an Insights publishing problem, as an
unknown one already did.

The native output admission and exclusive atomic publication above are unchanged.
Public tests traverse the actual profile Settings, native logs and save chooser,
compare preview/output bytes after later appends, confirm/cancel identifying saves,
select current/retained/ring scope and exercise content canaries. They run under
network isolation. Native Windows/Linux, packaged and human judgments remain M5
obligations in the [RFC-0027 ledger](../verification/RFC-BPR-0027.md).


### M5 owner UI amendment

The 2026-09-27 attended feedback uses AllTheThings’ segmented settings and
Help exports as the reference. Qt displays paired capture choices and direct
Help export actions. The profile owner composes the same maintenance host on
demand; Settings need not be visible. The export dialog automatically requests
a frozen preview. Expandable options invalidate that selection when changed;
Save remains disabled until a new preview is accepted. The identifying dialog
names the retained-identity risk and defaults to Cancel. Its explicit Save action
carries the existing core confirmation bit for that exact descriptor. No new
capture, classification, destination, mutation or network authority is added.

## Troubleshooting resets (BPR-1602)

`core/profile_data_reset.hpp` names four resets (caches, settings, review data,
everything) and the area each store or top-level folder belongs to. The policy
is closed: an unnamed store is kept, and the sign-in stores, logs and shared
stores are never in scope. Core admits a request only with the acknowledgement
a destructive reset needs. The maintenance adapter's purge flag is a journalled
scope (0 clear, 1 purge, 11–14 the resets), so a reset reuses the exact native
inventory, `BPRMNT03` journal and startup replay. Its scan passes everything
outside the scope unread, accepts any owner-private descendant of a cache folder
(so a damaged cache can be cleared), keeps the purge's exact layout for the
publication store, and never calls the credential port; the credential receipt
it appends is empty. An optional copy of the removed `state/v1` files goes to a
new owner-private folder before the journal, and its failure removes nothing.

The reset runs in the profile startup's lease-holding root step, after
`recover_release_profile` and before any store, logger or producer opens. The
desktop closes every window of the profile through the management host, waits
until its runtime has gone, then starts it with the confirmed request; a reset
that stops part-way refuses that start and the next start replays its journal.
Reset settings then restores Home's queue display choices through the queue
store, keeping saved repositories, servers and observer rules.

## Remembered connection metadata (RFC-BPR-0071)

The profile-private `state/v1/remembered-connections.sqlite3` snapshot and its recognized SQLite sidecars are state-store inventory/purge targets. They carry connection selection and disable facts, never CLI credentials. Profile purge removes them through the existing exact inventory; it neither changes gh authentication/configuration nor removes shared updater/registry state. Storage/policy and composed restart/refusal evidence are mapped in the [RFC-BPR-0071 ledger](../verification/RFC-BPR-0071.md); they add no uninstall or shared-state purge claim.

RFC-BPR-0086 adds `insights.sqlite3` and its sidecars to the existing profile
state inventory. Full profile purge and Troubleshooting’s Reset everything
remove this lifetime statistics store; Clear caches, Reset settings, Reset review
data and Clear local diagnostics keep it. An anonymized frozen export appends
only previously measured byte aggregates, never repository labels or a new disk
scan, and binds those extra bytes into the existing preview descriptor’s size
and digest. The identifying export includes no Insights section.
