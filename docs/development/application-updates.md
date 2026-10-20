# Application updates and recovery

This page covers updating BetterPullReview from inside the app, what to do if
an installation is interrupted, and where update data is stored.

Self-update is available only for eligible, production-signed macOS
installations. Windows, Linux and administrator-managed installations are
updated manually from the
[release page](https://github.com/MikeMarcin/BetterPullReview/releases/latest).

## Check for updates

No sign-in is needed. Releases are public, so the updater reads them without a
credential and there is no account to choose.

1. Choose **Check for Updates…** in the application menu. It is available even
   with no review windows open. You can also use **Check Now** under **Software
   Update** in **General** settings; both use the same service and preferences.
2. If a newer stable version is available, choose **Download**.
   BetterPullReview downloads the exact universal ZIP and verifies it.
3. Choose **Install and Restart**. The release's identity and macOS trust are
   checked again. Every BetterPullReview window and process must save its state
   and quit on its own.
4. After the restart, check the installed version and the last installation
   result in **General** settings.

The panel shows only the commands that apply at the moment:

| Command | What it does |
|---|---|
| **Cancel** | Stops the download. The installed app is unchanged. |
| **Later** | Dismisses this offer. |
| **Skip This Version** | Stops automatic checks from offering this version. A manual check can still offer it. |
| **Open Release Page** | Opens the release page in your browser. Shown beside an offer and wherever this copy cannot update itself. |

An unavailable or unreachable release is reported as an error, never as "up to
date".

### What a successful install means

Success means the new version started, its storage was ready, and it confirmed
a healthy start.

- If a save fails or times out, installation stops. The updater never forces
  another app process to quit.
- Saved review text appears in a read-only recovery window after the restart.
  Copy it into the right review yourself.

> [!WARNING]
> If a publish to GitHub was in progress and its result is unknown, check
> GitHub before you publish again. Updating never retries that write.

### Automatic checks

- On by default for eligible installations, and run at most once a day.
- Use the same public release access as manual checks.
- A failed check waits for the next daily check or a manual retry. Manual
  checks still respect GitHub's rate limits.
- The panel shows the last successful check. A previous installation result
  appears only when that installation did not finish.
- Turning automatic checks off leaves **Check for Updates…** available.
- The panel warns you if the preferences cannot be saved.

### When self-update is unavailable

These installations explain why they cannot update themselves and offer the
release page instead:

- unsigned or ad-hoc-signed builds;
- an app run from App Translocation, a mounted disk image or a read-only
  volume;
- an installation the app cannot write to;
- unsupported platforms.

Copy the app out of the DMG before using self-update. The updater never asks
for administrator privileges.

**Managed installations.** Setting the `BPRManagedUpdates=true` preference in
the application's preference domain turns off automatic update traffic and
self-replacement. Follow your administrator's distribution process instead.

**Older releases.** Releases from before the signed update helper and its
startup check must be updated manually once, by downloading and installing the
new version yourself.

## If an installation is interrupted

If the app is still running, quit it normally in every process using that
installation.

| Stage | Timeout |
|---|---|
| Waiting for every process to quit | 30 seconds |
| Waiting for the new version to confirm a healthy start | 60 seconds |

Until the new version confirms a healthy start, the old version is kept as a
backup. Recovery never starts both versions on the same data, and never forces
a process to quit.

An interrupted installation can stop the app from starting before any window
appears. There is no automatic recovery wizard; recover with the update helper
bundled in the app, as described below.

> [!NOTE]
> These recovery steps are for an administrator or experienced user. They
> have been tested on Apple Silicon only.

### Find the installation's control directory

1. Find the exact absolute path of the installation, for example
   `/Users/you/Applications/BetterPullReview.app`.
2. The control directory sits next to it, named `.bpr-update-<hash>`. `<hash>`
   is the first 24 lowercase hex characters of the SHA-256 of the absolute
   installation path's UTF-8 bytes, with no trailing newline. The directory is
   private to its owner.
3. Read its `active.json` on this computer and check that:
   - `installed` is the application you mean;
   - `schema` is `bpr.update.transaction.v1`;
   - `transaction` names an exact `transaction-<id>` directory.

> [!WARNING]
> Do not edit the journal, rename app bundles by hand, remove lock files, or
> pick a directory because it looks old. A process ID or a directory name is
> never enough to decide what to replace. The helper checks signatures, the
> journal, held locks, file-tree identities and recorded inodes itself.

### Run the recovery helper

The commands below are templates, **not literal paths**. Replace every
`/absolute/...` path, `EXACT-HASH` and `EXACT-TRANSACTION-ID` with the values
you found. Every file argument must be absolute; the current directory is not
used.

1. Inspect the journal:

   ```sh
   /usr/bin/plutil -p "/absolute/install-parent/.bpr-update-EXACT-HASH/active.json"
   ```

2. If `BetterPullReview.app` exists, run its signed helper:

   ```sh
   /usr/bin/env -i "/absolute/install-parent/BetterPullReview.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
   ```

   If the installation was already moved aside and `BetterPullReview.app` is
   missing, run the helper inside that transaction's backup instead, still
   naming the original installation path:

   ```sh
   /usr/bin/env -i "/absolute/install-parent/.bpr-update-EXACT-HASH/transaction-EXACT-TRANSACTION-ID/backup.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
   ```

3. Check the result:
   - **Exit code 0** means recovery finished. It does not mean the newer
     version was installed. Start the app normally, then check **General**
     settings and any recovered draft text.
   - **A nonzero exit code** can mean another app process or helper is still
     running, something is untrusted or changed, or the journal is incomplete.
     Keep every copy and get help from an administrator.

If the helper is missing or fails its trust checks, keep the whole transaction
directory for diagnosis before an administrator reinstalls the app by hand.
Repeatedly forcing the app to launch, deleting the control directory or
clearing quarantine do not recover an installation.

## Where update data is stored

| Data | Location |
|---|---|
| Update preferences (automatic checks, skipped version, last check and result) | `state/v1/release-update.sqlite3` in the application's private registry folder, shared by all profiles |
| Recovered review text after an update | `state/v1/update-draft-recovery.sqlite3` in each profile |
| Downloads in progress | Private staging owned by the update transaction |
| Installation journal, last result, locks, process records and transaction folders | The control directory next to the app: `active.json`, `last-result.json` and `transaction-<id>` folders |

Credentials stay in the system's secure credential store. Your review data
never moves with the app bundle. The installer records where your data lives,
so the restarted app cannot pick a different store.

After a healthy start, the old backup is cleaned up using the saved file
list. Files that changed or were added are kept. Finished journals and failed
downloads can stay behind for diagnosis.

> [!NOTE]
> There is no automatic age-based cleanup and no menu command to remove
> update data. Removing only the app can leave private preferences, recovered
> drafts and transaction data behind. If an installation is in progress, keep
> its transaction before you uninstall. Complete uninstall and purge are not
> yet qualified.

Maintainers: see the [release operator gate](application-updates-release-gate.md).
