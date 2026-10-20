# Storage, diagnostics and removal

This guide explains what BetterPullReview stores on your computer, how long it
keeps it, and how to export diagnostics, reset or purge a profile, and
uninstall the app. It is written for people using the app and for IT staff who
need to know what is stored where and what removal leaves behind.

> [!WARNING]
> Native qualification of clean-account installs, credential-vault handling,
> crash privacy and complete removal is not finished on every platform. A
> working control does not mean every platform has been certified. The
> [qualification record](../verification/RFC-BPR-0014.md) lists what is still open.

## Find your data

BetterPullReview keeps each profile's data in a private folder under your
user account.

| Platform | Default location |
|---|---|
| macOS | `~/Library/Application Support/BetterPullReview/BetterPullReview` |
| Windows | `%LOCALAPPDATA%\BetterPullReview\BetterPullReview` |
| Linux | `$XDG_DATA_HOME/BetterPullReview/BetterPullReview`, normally `~/.local/share/BetterPullReview/BetterPullReview` |

- The **Default** profile uses that folder directly.
- Each additional profile lives in `profiles/<id>` under it. The `<id>` is an
  opaque identifier: it is not the profile's display name or its numeric CLI ID.
- OS folder redirection and managed environments can move the base folder.
- Confirm which profile is selected in **Settings** before any maintenance.

Each profile also has a sibling folder for disposable agent workspaces, named
`<profile-folder-name>-agent-workspaces-v1`.

### How the folder is protected

- Private folders and files are readable only by your account (owner-only
  permissions on macOS and Linux, private access lists on Windows).
- Only one running copy of BetterPullReview can own a profile at a time.
- BetterPullReview refuses to open a profile folder with unsafe permissions,
  or one where a folder has been swapped for a link. Having a path that looks
  like the examples above does not make it safe.
- Other processes running as you, administrators, backups and disk encryption
  are governed by your OS, not by BetterPullReview.

> [!WARNING]
> Do not copy or edit live databases, delete lock files, or delete a profile
> folder recursively. Use the reset and purge controls described below.

## What is stored

All locations are relative to the selected profile's folder. Notes, drafts and
receipts are private data: they cannot be rebuilt from GitHub.

| Data | Location | Kept until |
|---|---|---|
| Preferences, Home queue, private review notes and drafts, window layout, sign-in metadata, publication recovery, comparison history | `state/v1/`: `appearance-settings.sqlite3`, `workbench-settings.sqlite3`, `diagnostic-preferences.sqlite3`, `review-queue.sqlite3`, `review-ledger.sqlite3`, `review-tours.sqlite3`, `review-cycles.sqlite3`, `window-restoration.sqlite3`, `device-bindings.sqlite3`, `review-mutation-recovery.sqlite3`, `epoch-catalog.sqlite3`, and their sidecar files | You reset or purge the profile |
| Images shown in comment and description bodies | `state/v1/markdown-images.sqlite3` and its sidecars | Seven days after last use; the oldest are removed first once the cache reaches 128 MiB |
| Retained Git history for pull requests | The profile's managed repository and cache folders | You clear caches, reset or purge. There is no automatic expiry |
| Agent and MCP material | The sibling `-agent-workspaces-v1` folder; `agent-workspace-ownership-v1` and `mcp-snapshot` in the profile | Purge, after any active or interrupted work is settled |
| Published Review Cycle candidates | `publication-git-v1/` | You reset review data or purge |
| Diagnostic logs | `logs/session-<id>-<segment>.jsonl` (older versions wrote `logs/diagnostics.log`, which is still read) | See [Diagnostic logs](#diagnostic-logs) |
| Shared state | `profiles/`, `updates/`, `state/v1/profile-registry.sqlite3`, `release-update.sqlite3`, `update-draft-recovery.sqlite3` and their sidecars | Not removed by any profile reset or purge |
| Sign-in tokens | macOS Keychain, Windows Credential Manager or Linux Secret Service, never in SQLite | You sign out, or purge the profile |

### Private review notes and drafts

`review-ledger.sqlite3` holds your private conversations, their individual
turns, which agent run produced what, the exact sources a review was composed
from, local drafts, and the record of what you published. Older notes keep
their exact text when the store is upgraded. Page and per-scope size limits
are listed in the [ledger contract](../architecture/review-ledger.md); a
change that would exceed them is refused as a whole.

If the app is killed mid-write, the last saved state survives. If GitHub
accepted a publish but the local record failed to save, BetterPullReview
reconciles it from the original request and GitHub's state, without posting a
second time. See the [private review workflow](../ux/private-review-workflow.md).

### Review tours

Tour drafts keep your authored Markdown, an editable copy of the remote
version, and outline details (anchors, private reasons and questions, and
which stops you visited).

| Limit | Value |
|---|---|
| Per field | 64 KiB |
| Per draft | 4 MiB |
| Drafts per profile | 128 |

Drafts are never evicted automatically and are not stored on GitHub. Each is
keyed to its profile, account, host, repository and pull request. If two saves
conflict, both copies are kept. See the
[tour storage contract](../architecture/review-tours.md).

### Review Cycles

Everything for Review Cycles is in `state/v1/review-cycles.sqlite3`: each
cycle's binding, captured revisions, notes and suggestions (with every
wording), round drafts and frozen rounds, delivery observations, responses,
verifications, grants, the disclosure policy, publication plans and journals,
and pull-request links.

- A captured file can be at most 128 MiB. A larger file is listed as too large.
- Captured content is held within a 768 MiB budget. When a capture would
  exceed it, the capture is refused and the store's contents are shown; nothing
  that a revision or round still uses is evicted. Reclaiming space removes only
  content nothing refers to.
- Removing a cycle asks for a preview first and permanently loses its history.
  GitHub cannot restore it.
- Rounds you saved or exported, and `.bpr-review.zip` bundles, are your own
  files and stay where you wrote them.
- BetterPullReview does not encrypt any of this.

**Share notes > Publish as Pull Request…** also creates `publication-git-v1/`
in the profile, a Git store that BetterPullReview owns for the candidate's
objects. Purge removes it only when its contents match the layout
BetterPullReview created; anything else inside it stops the purge. The
[domain model](../architecture/domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053)
describes the store's recovery and pinning rules.

### Retained Git history

Retained pull-request history is never pruned automatically. When a
comparison point is pruned, a marker remains and disk space may not be freed
straight away. Deleting a repository cache removes its bytes. History
BetterPullReview never observed cannot be recovered.

### Sign-in credentials

- **Manual tokens** last for one session. BetterPullReview removes them on a
  normal quit.
- **Device sign-ins** survive a normal quit and are checked again on restart.
- **Sign out** deletes the stored access and refresh tokens and stops the
  sign-in from resuming. Replacing a device sign-in with a manual token
  removes the earlier one.
- A crash, or missing sign-in metadata, can leave tokens in the vault that
  BetterPullReview can no longer find.
- Neither purge nor uninstall removes every entry in the vault namespace.
  Purge removes only the sign-ins it knows about.
- Credentials held by `gh` or an AI agent belong to those tools.
  BetterPullReview never removes them.

## Back up inline comments before recovery

Inline comment drafts are private data in
`state/v1/review-ledger.sqlite3`. Each draft keeps its target, the head
commit it was written against, its state and revision. A draft written
against an older head, or one whose **Publish** button is unavailable, has
not been deleted.

To make a safe copy:

1. Quit BetterPullReview normally.
2. Copy the whole selected profile folder into a new folder only you can
   read. Keep each database's `-wal` and `-shm` files with it.
3. Leave the original in place.

Do not copy a live database on its own, remove locks, edit records, or open
your only copy in a tool that might write to it. The copy contains private
review text, so keep it out of repositories and shared logs.

To read earlier text in the app, open **Finish review > Saved comments**. It
shows comments written against earlier heads, for the account and pull
request they belong to.

## Diagnostic controls

Open **Help > Diagnostics…** or **Settings > Storage & privacy**. The
controls work while you are signed out and apply to every window of the
profile. Each profile has its own diagnostic settings and logs.

| Control | Default | What it does |
|---|---|---|
| **Record local diagnostics** | On | Off stops writing new log records and recent context once the change is applied. Existing log files are kept. |
| **Detail** | Standard | **Verbose** also records the intermediate steps of provider calls and their timestamps. Warnings and errors are recorded either way. |
| **Identity privacy** | Anonymized | Links between requests and windows use opaque labels. **Include locally** asks for confirmation, then also keeps their numeric IDs. Account and repository names, paths and review text are not recorded in either mode. |
| **Open logs folder** | | Asks your OS to show this profile's `logs` folder. An error is shown if the OS refuses. |
| **Clear local diagnostics** | | After you confirm, removes this profile's logs and recent context. Exports, preferences and other data are kept. A new empty log file and the lock file remain. |

**Clear local diagnostics** reports **Busy** if another session is using the
logs, and reports unsafe storage or a partial deletion as a failure.

Your settings are saved in `state/v1/diagnostic-preferences.sqlite3` and load
before recording starts.

- The new setting is shown as soon as you choose it, and takes effect once
  pending log writes finish. A setting that could not be saved is not shown
  as applied.
- If the settings store cannot be read, recording and editing are turned off
  and the panel says it is using safe defaults.
- Changing a setting never rewrites existing logs.

### Recent context

In either detail mode, BetterPullReview keeps a short in-memory history of
recent, sanitized steps: at most three minutes, 512 events and 2 MiB. It is
never written to disk unless you include it in an export.

It is cleared when you turn recording off, change detail or privacy, clear
diagnostics, or sign out. Changing privacy also resets identity labels. When
recording is off, no hidden history is kept. Sanitized explanation text can
still carry some context.

### Status and dropped records

The panel shows whether recording is available, how many records have been
lost, the size of recent context, and any control failures. Records can be
lost to contention, a full log, or invalid events; under load an event may
not reach disk. The limits are bounds, not a delivery guarantee. The
[storage reference](storage-reference.md#diagnostic-recording-limits) lists
the queue sizes.

## Diagnostic logs

| Limit | Value |
|---|---|
| Size of one log file before a new one starts | 5 MiB |
| Total logs per profile | 50 MiB and 64 files |
| Inactive logs removed after | 30 days, or oldest first when space runs out |

- Old logs are pruned when logging starts and before writes. An idle file can
  outlive 30 days until the next pass.
- The file a running session is writing is never pruned. If active files fill
  the space, recording reports **full** or **busy** instead of deleting them.
- Recording stops if the logs folder contains links, unknown entries, or files
  that were replaced.
- These limits apply to BetterPullReview's own writing. They are not a disk
  quota against other programs.
- A crash can leave a partial last record. The next start writes a new file
  and never appends to it.

The log file you can read in the logs folder is not an export. It can contain
older provider text and identifying details recorded by earlier versions;
upgrading does not rewrite old records.

## Preview and export diagnostics

Exports let you look at exactly what would be shared before you save it.

1. Choose **Help > Export anonymized diagnostics…** (also in **Settings >
   Storage & privacy**). This works while signed out. BetterPullReview takes a
   bounded snapshot of this profile's logs and opens a preview. Sessions,
   requests and windows get fresh opaque labels.
2. Check the scope, time range, categories, size and anything omitted.
   **Show records** shows the exact file text. Sanitized explanations and
   operational facts remain and can still reveal context.
3. Optionally open **Export options** to limit the export to the current
   session or to include recent context (off by default). Changing an option
   discards the old snapshot; choose **Update preview** before saving.
4. Choose **Save…** and a new file name, or **Cancel**.

To include identifying details, choose **Help > Export with identifying
details…** instead. It shows an explicit disclosure and asks you to confirm
with **Save with identifying details…**; **Cancel** is the default. It keeps
the session and numeric IDs that were recorded, but cannot restore identities
that were never recorded. Credentials and review content are excluded in both
kinds of export.

> [!NOTE]
> An export only writes a file on your computer. Nothing is uploaded, sent to
> support, attached to a repository or passed to an agent.

### Where the file is saved

- The save dialog starts in your Downloads folder, then Documents, then your
  home folder if the earlier one cannot be used. You can pick another folder
  and rename the file.
- An existing file is never overwritten.
- Cancelling the save dialog keeps the preview as it was.
- If none of the three folders is available, the preview stays open with an
  explanation beside **Save…**. Make a folder available and try again.
- Picking a starting folder creates nothing and remembers no folder history.

### What the export contains

- Records added after the snapshot are not included. Original log files are
  left untouched.
- Closing the export dialog, or the Settings window it came from, cancels the
  preview. Until then, a preview can hold the recent context you selected.
- An export reads at most 16 MiB and 65,536 records. A larger set reports a
  limit instead of being cut silently.
- An export contains at most 1,024 records and 256 KiB. Records beyond that,
  unrecognized or incomplete records, and unknown detail are omitted and
  counted. Duplicates are counted once.
- An empty selection says it contains zero records.

### If an export fails

An unsafe source, a changed target or a size limit means the export did not
succeed. Preview again and choose a new file name.

If the file was written but a final save step failed, the file may still be
there; check it before trying again. Cancelling near the end is best effort,
and an export that already finished may still report success.

> [!WARNING]
> Do not send raw logs, databases, MCP snapshots or OS crash reports as if
> they had been through the export preview. BetterPullReview never uploads
> them; OS crash reporting has its own settings. Crash reports are not yet
> shown to keep source, comments and tokens out on every supported platform.

The [storage reference](storage-reference.md#diagnostic-record-format)
describes the record fields and how exports parse them.

## Reset this profile's local data

If BetterPullReview misbehaves because a profile's data is damaged, open
**Settings > Storage & privacy > Troubleshooting**. Each reset keeps you
signed in and reopens the profile's windows.

No reset signs you out, reads or deletes a credential, changes anything on
GitHub, removes diagnostic logs, or touches another profile, the shared
profile list and update state, agent workspaces, exported files or a Git
checkout.

| Reset | Removes (whole files with their sidecars) | Keeps |
|---|---|---|
| **Clear caches** | `repositories/`, `incoming/`, `locks/`, `quarantine/`, `mcp-snapshot/`, `state/v1/epoch-catalog.sqlite3` and its lock, `markdown-images.sqlite3`, `review-overviews.sqlite3` | Everything else |
| **Reset settings** | `appearance-settings`, `workbench-settings`, `diagnostic-preferences`, `window-restoration`; Home's queue display choices are reset inside `review-queue` | Saved repositories, servers, observer rules, review data, caches |
| **Reset review data** | `review-ledger`, `review-tours`, `review-cycles`, `review-mutation-recovery`, `publication-git-v1/` | Settings, caches, saved sources |
| **Reset everything** | All of the above, plus `review-queue` (saved repositories, servers, observer rules and Home's queue) | Sign-in (`device-bindings`, `remembered-connections` and the vault) and logs |

> [!WARNING]
> **Clear caches** also discards the record of earlier pushes. Pushes that
> GitHub no longer has after a force-push cannot be compared again.
>
> **Reset review data** deletes private notes, unsent comments and Review
> Cycles that exist only on this computer.

**Reset review data** and **Reset everything** ask you to acknowledge the
loss. Their confirmation offers **Save a copy of the removed data in Downloads
first**, which is on by default. The copy is a new private folder named
`BetterPullReview reset <date> <time>` in Downloads (or Documents, or your
home folder) holding the removed `state/v1` files. If the copy cannot be
made, nothing is removed.

When you confirm, the profile's windows close and the profile starts again.
The reset runs before any data opens. If the app crashes part-way, the next
start finishes the same reset before the profile opens. If a removed folder
contains anything unexpected (a link, a file owned by another account, a
folder where a database should be), the reset removes nothing. The reopened
window's status bar says what was reset, or why it did not run.

### When a profile cannot open

Most damaged data does not stop a profile opening: only the feature that uses
it fails, and a reset clears it. A damaged comparison history
(`epoch-catalog.sqlite3`) does stop the profile at start. BetterPullReview
then shows **Profile "…" couldn't open** with:

- **Clear caches and try again** (the default)
- **Reset everything…**, which asks for confirmation
- **Quit**

The chosen reset runs before the profile's data opens, and the profile then
opens again without restarting the app. If it fails again, the dialog appears again.

### Reset from the command line

Use these options when a profile crashes before it can show the dialog.

| Option | Effect |
|---|---|
| `--reset-profile-data` | Shows the same dialog before anything in the profile loads |
| `--reset-profile-data-scope caches\|settings\|review-data\|everything` | Runs that reset directly. Naming a destructive reset counts as its acknowledgement |
| `--reset-profile-data-copy` | Also saves the copy. Use it with `--reset-profile-data-scope review-data` or `everything` |

On macOS, run the executable inside the app bundle, for example:

```sh
/Applications/BetterPullReview.app/Contents/MacOS/BetterPullReview --reset-profile-data
```

Damage to the profile list shared by all profiles (`profile-registry.sqlite3`)
is outside these resets.

## Purge a profile or clear its diagnostics

Purge permanently removes a profile's private data. The same flow can clear
only its diagnostic logs.

1. Finish or save work in every window of the profile.
2. In **Settings > Storage & privacy**, choose **Prepare profile purge…** or
   **Prepare diagnostic cleanup…**.
3. Read the warning and choose **Stop profile and prepare**. This closes the
   profile's windows and lists what would be removed; it deletes nothing yet.
   **Cancel** is the default. Once the profile has stopped, cancelling does
   not restart it.
4. Check the profile, the categories and the number of entries, then confirm
   with **Purge prepared profile data…** or **Clear prepared diagnostics…**.
5. Read the result. Close the Settings window to release the profile, then
   reopen the profile if you want to. Other profiles are not affected.

| Choice | Removes | Leaves alone |
|---|---|---|
| **Clear prepared diagnostics…** | The listed diagnostic logs | Everything else |
| **Purge prepared profile data…** | The profile's private stores, retained Git history, managed agent and MCP material, and the sign-ins it remembers | GitHub reviews and comments, your own checkouts and worktrees, shared update state |

Preparation is refused when the folder has unknown or changed entries, is
unsafe, has unfinished recovery for a feature, or holds more than 4,096 items.
An agent workspace with active, interrupted or unknown work also blocks purge
until that work is settled. Do not work around a refusal by deleting folders:
settle the named feature and prepare again.

Once deletion starts it cannot be undone, even if you cancel. A failure can
leave the profile stopped until recovery runs. On the next start,
BetterPullReview finishes deleting exactly what was listed, and refuses
anything that was changed or added. If startup refuses, keep the remaining
files and the `.bpr-maintenance-v1` journal, and do not edit it or remove
locks. There is no general repair or secure-erase command.

### What purge leaves behind

- The profile folder itself and its lock file
- Shared state: `profiles/`, `updates/`, the profile list and update stores
- Other profiles
- Files you exported, saved or copied elsewhere
- Empty workspace and `state` folders
- Backups, and any data that SSDs, write-ahead logs or file-system journals
  still hold physically

Update draft recovery (`update-draft-recovery.sqlite3`) can hold private text
even when it sits inside a profile, and purge does not remove it. There is no
app control that removes all profiles or update storage at once. IT teams
should record these items in their removal inventory.

## Update, migrate and uninstall

### Updates and data upgrades

Each database checks its own identity and upgrades itself in one step. A
database written by a newer version, a foreign file or a damaged one can be
refused. If an optional database fails, the related feature may run in a
limited or session-only mode.

An older version is not guaranteed to open data written by a newer one. To
keep private data across a change, quit every copy of BetterPullReview and
back up the complete set of databases and their sidecar files. Copying one
live `.sqlite3` file is not a backup. A file backup does not include
credentials in the OS vault.

Windows, Linux and managed packages update manually. Eligible macOS
installations use the [application update guide](application-updates.md),
which covers the updater, its backups and recovery. Purging a profile does
not replace that recovery procedure or remove updater draft text.

### Leave completely

To remove BetterPullReview and its data:

1. Finish or settle any publish in progress and any update in progress.
2. Sign out of each account.
3. Purge every profile while the app is still installed. Check each profile,
   and the shared, update and export locations, separately.
4. Quit normally, then uninstall:
   - **macOS:** delete the app from the Applications folder you chose.
   - **Windows:** use **Installed apps** or the MSI uninstaller.
   - **Linux:** remove the per-user installation folder, or remove the DEB or
     RPM package with your package manager.

> [!WARNING]
> Uninstalling removes program files only. Profiles, logs, shared state,
> exports and backups stay on disk. Uninstalling cannot withdraw reviews
> already published to GitHub.

If deleting a credential fails, keep the sign-in and recovery metadata so the
failure can be diagnosed: deleting the metadata first can remove the exact
key needed to clean up the vault.

Keep the matching source archive available to anyone you redistribute the app
to, as the [distribution policy](release-packages.md) requires.

## Troubleshooting: agent workspaces are unavailable

If the status bar of every window says **Disposable agent workspaces are
unavailable: their folder needs repair or has reached its limit.**, startup
recovery refused the `-agent-workspaces-v1` folder. Launched from a terminal,
BetterPullReview also prints
`AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:<reason>` to standard error.
The app still exits normally (code 0) when you quit.

The usual reason is `repair-required`, which means either:

- the folder that holds the area can be written by another user or group, or
- an entry in the area is not the one BetterPullReview recorded.

BetterPullReview never changes the permissions, owner or contents of a folder
it did not create.

To fix it:

- **macOS and Linux:** an earlier install that ran with umask `0002` often
  left the folder holding the Default profile group-writable (on Linux,
  normally `~/.local/share/BetterPullReview`). Current versions create new
  folders owner-only but do not repair existing ones. If `ls -ld` shows group
  or other write permission, run `chmod go-w` on that folder and start
  BetterPullReview again.
- **Windows:** remove any write permission that another account holds on that
  folder.

Do not repair an entry inside the area by hand; keep it for diagnosis.
Recovery refuses anything whose recorded identity it cannot prove.

## Related

- [Storage reference](storage-reference.md): log record format, export parsing
  and maintenance recovery details
- [Maintenance contract](../architecture/release-maintenance.md)
- [Basic local diagnostics](build-and-test-reference.md#basic-local-diagnostics)
