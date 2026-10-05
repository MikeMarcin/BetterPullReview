# Storage, diagnostics and removal

These instructions describe the composed RFC-BPR-0014 maintenance controls.
The [native qualification ledger](../verification/RFC-BPR-0014.md) still has
open clean-account, credential-vault, crash-privacy and complete removal courses.
An implemented control or passing fixture does not certify every platform.

## Locate the selected profile

The application uses Qt's `AppLocalDataLocation` with organization and application
both named `BetterPullReview`. Typical base locations are:

| Platform | Base location |
|---|---|
| macOS | `~/Library/Application Support/BetterPullReview/BetterPullReview` |
| Windows | `%LOCALAPPDATA%\BetterPullReview\BetterPullReview` |
| Linux | `$XDG_DATA_HOME/BetterPullReview/BetterPullReview`, normally `~/.local/share/BetterPullReview/BetterPullReview` |

OS redirection and managed environments can change the base. The legacy/default
profile uses that base; isolated profiles use `profiles/<opaque root identity>`
under it. A profile's display name or numeric CLI ID is not that directory name.
Confirm the selected profile in Settings before maintenance. Root admission
rejects unsafe permissions and symlink/reparse substitutions; it does not make
an arbitrary path safe because it resembles one of these examples.

Native private directories/files use owner-only modes on POSIX and validated
private ACLs/handles on Windows. A kernel-held exclusive lease prevents two
writers from owning the same profile. Same-user/admin access, backups and disk
encryption are separate OS controls. Do not copy or edit live databases, remove
locks, or treat a root directory as a recursive deletion target.

### When the status bar says agent workspaces are unavailable

Disposable agent workspaces live beside the profile root, in
`<profile-root-leaf>-agent-workspaces-v1`. Suppose startup recovery refuses
that area. Then every window of the profile adds "Disposable agent workspaces
are unavailable: their folder needs repair or has reached its limit." to its
status bar ([[BPR-0817]]). A launch from a terminal also attempts to print
`AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:<reason>` to standard
error. The refusal does not change the exit code: an orderly quit still exits 0.

The usual reason is `repair-required`. It means one of two things:

- The folder that holds the area can be written by another user or group.
- An entry in the area is not the one BetterPullReview recorded.

BetterPullReview never changes the mode, owner or contents of a folder it did
not create.

On Linux and macOS, the common case is an earlier install that ran under umask
`0002` and left the folder that holds the Default profile group-writable. On
Linux that folder is normally `~/.local/share/BetterPullReview`. Current
versions create missing folders owner-only, but they do not repair one that
already exists. If `ls -ld` shows group or other write on that folder, remove
it with `chmod go-w`, then start BetterPullReview again. On Windows, remove
any write permission that another principal holds on that folder. Do not
repair an entry inside the area by hand. Instead, keep it for diagnosis:
recovery refuses anything whose recorded identity it cannot prove.

## Inventory and retention

The profile's `review-ledger.sqlite3` stores RFC-BPR-0023 private threads,
immutable turns, run provenance, exact composition sources, local drafts and
publication lineage. Schema 7 migrates supported v1–v6 ledgers atomically;
old notes retain their bytes and become `legacy_unknown` revisions. Pages and
scope quotas are bounded; an oversized admission refuses the whole transaction.
The [ledger contract](../architecture/review-ledger.md) lists exact page, body
and scope limits. A killed writer leaves the last committed state; remote success
with a failed local lineage commit is reconciled from the existing request and
server truth without a second POST. Profile purge removes this private store
through the admitted inventory, not a GitHub comment or shared updater state.
Diagnostic output uses identities and digests rather than private bodies or
credentials. See the [operator workflow](../ux/private-review-workflow.md).

| Data | Location relative to the admitted profile root | Removal behavior |
|---|---|---|
| Preferences, queue, private ledger, window destinations, device-binding metadata, publication recovery and epochs | `state/v1/`: `appearance-settings.sqlite3`, `workbench-settings.sqlite3`, `diagnostic-preferences.sqlite3`, `review-queue.sqlite3`, `review-ledger.sqlite3`, `review-tours.sqlite3`, `review-cycles.sqlite3`, `window-restoration.sqlite3`, `device-bindings.sqlite3`, `review-mutation-recovery.sqlite3`, `epoch-catalog.sqlite3`, plus admitted sidecars | Explicit profile purge includes the exact known files. Notes, drafts and receipts are private data, not merely a reconstructible cache. |
| Retained Git data | Managed repository/cache/catalog layout and its identity manifests | No automatic TTL/LRU pruning. Exact epoch pruning leaves tombstones and does not promise immediate byte reclamation. Explicit repository-cache deletion removes that cache's bytes; never-observed history cannot be recovered. |
| Agent and MCP material | Adjacent `<profile-root-leaf>-agent-workspaces-v1` ownership area; profile `agent-workspace-ownership-v1` and `mcp-snapshot` | Original owners reconcile active/interrupted work before purge. Populated or unknown workspace state can block it; no broad sweep is used. Existing developer checkouts are outside purge. |
| Diagnostics | `logs/session-<opaque-session>-<segment>.jsonl`; legacy `logs/diagnostics.log` is read for compatibility | New files rotate before 5 MiB. Coordinated retention bounds the profile to 50 MiB and 64 files, pruning inactive history older than 30 days and oldest inactive files under pressure. Explicit cleanup removes the prepared logs; `logs/.retention.lock` remains. |
| Shared/excluded state | `profiles/`, `updates/`, `state/v1/profile-registry.sqlite3`, `release-update.sqlite3`, `update-draft-recovery.sqlite3` and their admitted sidecars | Current-profile purge excludes them. Updater draft recovery may contain private text even when stored within a profile. There is no general all-profiles/updater-storage removal UI. |
| Credentials | Native Keychain/Credential Manager/Secret Service; tokens are outside SQLite | Sign out deletes exact access/refresh slots. Profile purge deletes known remembered bindings through their existing owner before removing binding metadata. |

Review tours retain private authored Markdown, editable raw remote baselines and
outline provenance (supplied anchor identifier, private reason/question and visit state),
bounded to 64 KiB per field, 4 MiB per encoded draft and 128 drafts per profile.
There is no automatic eviction. The exact profile/account/host/repository/PR key
is retained; a save conflict keeps both copies. These drafts are not server-backed.
Profile purge removes the admitted tour store and sidecars without changing any
GitHub comment. See the [tour storage contract](../architecture/review-tours.md).

Review Cycles keep everything in `state/v1/review-cycles.sqlite3`: each
cycle's binding, captured revisions, notes and suggestions with every wording,
round drafts, frozen rounds, delivery observations, responses, verifications,
grants, the disclosure policy, publication plans and journals, and pull request
links. Captured bytes and frozen payloads are content-addressed objects in the
same store, pinned while a revision or round names them. A captured file is at
most 128 MiB (a larger one is listed as too large), and retained objects are
held within a 768 MiB budget: an install that would exceed it is refused with
the store's inventory rather than evicting pinned evidence, and reclaiming
removes only objects nothing references. Removing a cycle needs a preview whose token still
matches and loses its history irrecoverably; GitHub cannot restore it. Rounds the
reviewer saved or exported, and `.bpr-review.zip` bundles, are the reviewer's
own files and stay where they were written. None of this is encrypted by
BetterPullReview. Publishing a cycle (Share notes > Publish as Pull Request…)
also makes the profile's `publication-git-v1/`, a BPR-owned Git store for the
candidate's objects. Profile purge includes that folder by its exact layout
(the installed Git isolation files, empty work and cache directories, and the
`publication/v1` bare object stores); anything else inside it blocks the purge
as an incomplete inventory ([[BPR-1283]]). The [domain model](../architecture/domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053)
gives the store's recovery and pin rules.

Manual-token sessions are session-only and remove their exact slots on orderly
quit. Settled device grants survive orderly quit and are revalidated on restart.
Sign out also disables the resumable binding; replacing a device grant with a
manual token removes the previous grant. A crash or missing binding metadata
can leave unresumable vault material. Neither purge nor uninstall enumerates a
whole credential namespace. External `gh` and AI credentials belong to those
applications and are not removed by BetterPullReview.

Retention runs at logger startup and before backend writes. Each active session
holds its current file; completed segments may be pruned while that session
continues. Idle files can remain past the age limit until the next retention
pass. If active files exhaust capacity, recording reports full or startup busy
without deleting them. Unsafe links, unknown entries and changed directory/file
identities refuse logging. These bounds apply to cooperating application writers;
external modification is not a disk quota. A crash may leave a partial final
record. Restart creates a new session file and never appends to that tail.

## Diagnostic controls

Open **Help → Diagnostics**, or profile **Settings → Storage & privacy**.
The controls remain available while signed out and are shared by the profile's
windows; other profiles have independent preferences and logs.

- **Record local diagnostics** defaults to On. Off stops new file and recent
  context capture after acknowledgement; it keeps older files.
- **Detail** defaults to Standard. Verbose adds bounded intermediate provider
  steps and their timestamps. Warnings/errors are recorded in either mode.
- **Identity privacy** defaults to Anonymized and is independent of detail.
  Request/window joins use opaque typed labels. **Include locally** asks for
  confirmation before also retaining their numeric identities. Current producers
  do not admit account/repository names, paths or review bodies in either mode.
- **Open logs folder** requests the OS handler for this profile's logs directory.
  A successful handoff does not prove a file manager displayed it; refusal is shown.
- **Clear local diagnostics** confirms this profile's scope, invalidates pending
  records, resets recent context and removes admitted logs. It preserves exports,
  preferences and other profile data. A fresh empty session segment and the
  coordination lock remain. Another active session returns Busy; unsafe storage
  and partial deletion have explicit failure results.

The accepted selection is shown until the effect and writer barrier complete.
Preferences live in the owner-private `state/v1/diagnostic-preferences.sqlite3`.
Restart loads them before recording starts. Unknown detail/privacy values use
Standard/Anonymized and preserve a valid Off choice; an unreadable store disables
capture and editing, with visible safe-default status. Explicit choices can
repair readable compatible rows. Failed storage does not appear as accepted.

Recent sanitized intermediate context remains in memory in either detail mode:
at most three minutes, 512 events and 2 MiB. Expired entries cannot be read and
idle backend maintenance retires them. Off, detail/privacy changes, Clear,
Sign out and logger retirement clear transient context; privacy changes also
reset identity joins. Off collects no hidden ring. Export includes this context
only when its separate recent-context checkbox is selected. Historical logs are never silently reprocessed
when a preference changes. Sanitized explanation text can still be contextual.

Status shows availability, cumulative loss, recent context size and control
failures. Loss includes contention, capacity and invalid events; saturation or
failure can prevent an event reaching disk. Normal ingress uses up to 3,584
events/7 MiB with reserved warning/error room inside 4,096 events/8 MiB. Smaller
per-thread Quill queues can drop earlier. These are bounds, not delivery promises.

## Preview and export diagnostics

1. Choose **Help → Export anonymized diagnostics…**, available while signed out.
   The same action is in **Settings → Storage & privacy**. It automatically
   freezes a bounded selection of retained logs for this profile and opens the
   preview. Session, request and window joins receive fresh opaque labels.
2. Inspect the scope, time range, categories, size and omissions. **Show records**
   expands the exact file text. Recognized sanitized explanations and operational
   facts remain; these can still reveal context.
3. Choose **Save…** and a new local filename, or **Cancel**. Existing files are
   refused. Later appends are excluded; original files remain intact.
4. **Export options** offers current-session selection and optional recent
   sanitized context (excluded by default). Changing an option cancels the old
   snapshot; **Update preview** freezes the revised selection before Save enables.
5. **Help → Export with identifying details…** uses the same short flow with
   an explicit disclosure and **Save with identifying details…** confirmation.
   Cancel is the default. Export cannot recover identities removed during
   recording. Credentials and review content are excluded in either mode.

Every export is a local write. No upload, support message, repository attachment
or transmission to an agent occurs. Closing the export dialog or its Settings window cancels its preview. A
frozen preview can contain explicitly selected recent context until cancelled or
consumed; it receives no new records after preparation.

Input work across the selected directory is bounded to 16 MiB and 65,536 records; a larger retained set reports a limit instead of silently truncating. Each incomplete file tail is counted as one omitted record, independently of the next file. Legacy v1 lines admit at
most 8,192 bytes; canonical v2/v3 JSONL records admit at most 16 KiB including the
newline. The JSONL parser accepts the writer's exact field order and bounded ASCII
scalar grammar, and omits unsupported encodings or extra fields. Every rich
field is classified again; legacy v1 retains only its small metadata prefix.
Unrecognized detail is explicitly omitted. Output is at most 1,024 records and
256 KiB; excess records are counted as omissions. Duplicate file/ring events
are counted once. Unknown/incomplete records are omitted and counted. An unsafe source, changed target or limit refusal is not a successful export.
An empty admitted selection explicitly reports zero records. After an export failure, preview again and choose a new filename.
If publication succeeded before a final durability failure, the selected file
may remain; inspect it before retrying. Cancel before the commit boundary is
advisory, and an operation already committed may still report success.

The readable log is not the export. New capability-error details use classified
explanations and public query symbols with explicit unknown-content omissions.
Existing retained files can contain earlier sanitized provider prose and identity
information; this change does not rewrite historical records. Do not send raw logs, databases, MCP snapshots
or OS crash reports as if they had passed the preview's policy. BetterPullReview
does not automatically upload them; OS crash collection has its own settings.
Current crash evidence does not establish redaction of resident source/comments/
tokens across the supported matrix.

New records carry an opaque logger `session`, an increasing `sequence`, UTC and
monotonic time, stable event/category/severity and source/toolchain build identity.
The [local logging guide](build-and-test.md#basic-local-diagnostics) defines these
fields and their bounds. Each newly constructed profile logger creates a fresh
session; older records remain unchanged. These diagnostic identities grant no
authority. Anonymized export remaps their joins independently for each preview;
identifying export retains the session and captured numeric IDs.

Epoch records carry originating `window_label` and `request_label` joins.
Include locally additionally retains numeric `window_session` and epoch-local
`request` (`0` during catalog opening before request allocation).
CLI Git acquisition refusals also carry a closed `git_step`, separating tool
version probes/qualification, identity probes, clone, object checks, local
store/pack work and pack reading. Other transports leave it `none`. These
fields record where a refusal occurred, not its unobserved underlying cause:
`clone` plus `unavailable` does not establish an authentication or size limit.
They contain no executable path, command output, repository identity or source.
Export applies the selected privacy policy to every classified field; the
legacy metadata-only API remains available for compatibility. Older records
are not retroactively enriched.

## Clear diagnostics or purge a profile

1. Finish or save work in every window of the selected profile. In **Storage &
   privacy**, select **Prepare diagnostic cleanup…** or **Prepare profile purge…**.
2. Read the preparation warning and choose **Stop profile and prepare**. This
   retires its windows and drains producers/stores before inventory; it deletes
   nothing. Cancel is the default. Once stopped, cancellation does not restart
   the profile automatically.
3. Inspect the prepared profile, categories and exact entry count. Use **Clear
   prepared diagnostics…** or **Purge prepared profile data…** to confirm that
   inventory. Clear removes only the prepared diagnostics. Purge includes the
   profile's private stores, retained history, managed agent/MCP material and
   exact remembered credential bindings. GitHub reviews and developer worktrees
   are unaffected.
4. Read the terminal result. Close the maintenance-only Settings shell to release
   ownership, then reopen the profile deliberately if needed. Other profiles
   have independent lifetimes.

Preparation refuses unknown/changed entries, unsafe roots, pending original-owner
recovery or more than 4,096 targets. Do not bypass a refusal with a directory
delete. Reconcile the named owning feature and prepare again. During irreversible
cleanup, cancellation cannot roll back deleted files. A failure may require
recovery and keep the profile stopped.

The `.bpr-maintenance-v1` journal binds the original inventory and credential
receipt. On restart, recovery runs under the lease before any producer/store
opens. It can finish deletion of that exact inventory; it refuses corrupt,
foreign, new or replaced objects. Preserve the journal and remaining private
files if startup refuses. Do not edit the journal, reconstruct credential keys
from labels or remove a lock to force admission. There is no general repair or
secure-erasure command; obtain an exact-scope operator diagnosis.

Purge intentionally leaves the profile root/runtime lock, excluded shared stores
and containers, sibling profiles, external exports and admitted empty workspace/
state parents. It does not erase backups or guarantee physical erasure from
SSD/WAL/journal media. Keep those residuals in a corporate removal inventory.

## Migration, update and uninstall

Each SQLite store validates its application/schema identity and performs its own
transactional migration. Future-version, foreign or corrupt stores can be
refused; optional-store failures may leave a visibly limited/session-only UI.
There is no generic migration switch and no promise that an older application
can open data written by a newer one. If preserving private state, quit every
owner and use a consistent corporate backup of the exact stores/sidecars;
copying one live `.sqlite3` file is not a backup protocol. A filesystem backup
does not back up or authorize credentials in a native vault.

For complete departure, first finish or reconcile attempted publications and
any active updater transaction, sign out of each native account, then perform
explicit profile cleanup while the app is still installed. Inspect every
profile and the excluded shared/update/export locations separately. If a native
credential deletion fails, retain its binding/recovery metadata for diagnosis.
Deleting metadata first can remove the exact key needed for cleanup.

After normal Quit, remove the macOS app from its chosen Applications folder;
use Windows Installed apps/MSI uninstall; remove a Linux per-user versioned
installation or use the corresponding DEB/RPM package-manager removal. These
remove program files and retain user state. Package removal cannot retract
reviews already published to GitHub. Keep the matching source asset available
for recipients under the [distribution policy](release-packages.md).

Windows/Linux and managed packages update manually. The separate
[macOS update/recovery guide](application-updates.md) owns the qualified helper,
healthy-start receipt, backup and sibling transaction directory. A current-profile
purge does not replace that recovery procedure or erase updater draft text.

Implementation owners are `profile_storage_root.cpp`, `release_maintenance_files.cpp`
and `profile_runtime_lease.cpp` under `src/adapters/sqlite/src/`,
`src/application/src/release_profile_cleanup.cpp`,
`src/application/src/release_diagnostic_snapshot.cpp`,
`src/app/src/profile_desktop.cpp` and
`src/adapters/qt/src/release_maintenance_panel.cpp`. The
[maintenance contract](../architecture/release-maintenance.md) records typed
authority, failure ordering, direct tests and remaining native evidence.
