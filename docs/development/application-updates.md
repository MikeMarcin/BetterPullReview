# Application updates and recovery

RFC-BPR-0035 implements a user-confirmed updater for eligible production-signed
macOS installations. **Check for Updates…** is in the application menu, including
with no review windows open. General Settings uses the same application-wide
service and preferences. Windows, Linux and administrator-managed installations
use manual distribution. The [verification ledger](../verification/RFC-BPR-0035.md)
records the qualified Apple Silicon packages, Intel follow-up and outstanding live
private-service evidence; implementation does not mean final release acceptance.

## Updating

1. Sign in through native GitHub.com authentication in **Accounts & hosts**, then
   choose that account explicitly under **Release access account** in General
   Settings or the Updates window. It must have access to the private
   `MikeMarcin/BetterPullReview` distribution repository. GHES, public preview and
   GitHub CLI accounts cannot supply update access. The current review account
   is never chosen automatically.
2. Use **Check for Updates…**. An unavailable or inaccessible release is an error,
   not an up-to-date result. **View Release** opens the fixed repository in the
   system browser, where the browser has its own sign-in state.
3. For a newer stable version, **Download Update** streams and verifies the exact
   universal ZIP. **Cancel Download** leaves the installed application unchanged.
   **Later** dismisses this offer; **Skip This Version** suppresses just this
   version from automatic offers. A manual check can offer it again.
4. Choose **Install and Restart** for the verified version. Release identity and
   native trust are rechecked. Every participating window/process must save its
   state and quit cooperatively. A failed save or timeout stops installation;
   the updater never kills another application process.
5. After restart, check the installed version and last installation result.
   Success means the replacement acknowledged healthy startup after its state
   stores were ready. Saved review text appears in a read-only recovery window.
   Copy it deliberately into the appropriate review. An indeterminate publication
   must be reconciled with GitHub before another Publish gesture; updating never
   retries that write.

Automatic checks default on for eligible installations and run at most daily.
They require the explicitly selected account to be available. Failures wait for
the next daily opportunity or a manual retry; manual checks still obey server
rate limits. General Settings shows the last successful check separately from
the last installation result. Turning automatic checks off does not remove the
manual action. Preferences are marked session-only when their store is unavailable.

Unsigned/ad-hoc builds, App Translocation, mounted images, read-only volumes,
unwritable installations and unsupported platforms explain their limitation and
offer the release page. Install outside the DMG before using self-update. A
corporate `BPRManagedUpdates=true` preference in the application's preference
domain disables automatic traffic and self-replacement; follow the administrator's
distribution process. There is no privilege elevation.

Releases predating the signed helper and startup-receipt protocol require a
manual bootstrap update. The historical 0.1.3 package is staging/trust evidence,
not a participating baseline. The disposable 0.0.350–0.0.352 qualification versions
are unpublished and numerically below 0.1.3; do not distribute them as an upgrade.

## Interrupted installation

If the application still runs, use normal Quit in every process using that exact
installation. A helper waits up to 30 seconds for cooperative shutdown. After
replacement it waits up to 60 seconds for the healthy-start receipt. An old backup
is retained if the replacement has not acknowledged startup. Recovery never
starts both versions on the same state root or forces a process to exit.

An interrupted active transaction can refuse ordinary startup before a window
appears. The current recovery interface is the bundled helper below; there is no
automatic recovery wizard. The signed backup-helper course has been exercised on
Apple Silicon. The instructions are operator guidance, not an attended usability
or Intel qualification claim.

An operator must resolve the exact absolute installation path and its control
directory before running recovery. The control directory is a sibling named
`.bpr-update-<first 24 lowercase hex characters of SHA-256 of the absolute
installation path's UTF-8 bytes, without a newline>`. It is owner-private. Read
its `active.json` locally and verify that `installed` is the intended application,
`schema` is `bpr.update.transaction.v1`, and `transaction` names the exact
`transaction-<id>` directory. Do not edit the journal, rename bundles by hand,
remove locks or choose a directory merely because it is old. Neither a PID nor
a directory name authorizes replacement. The helper checks native signatures,
the journal, held locks, tree identities and recorded inodes itself.

The following are command templates, **not literal installation paths**. Replace
every `/absolute/...` path and `EXACT-TRANSACTION-ID` with the values just resolved.
Every file argument is absolute; the current working directory has no meaning.
First inspect the selected journal:

```sh
/usr/bin/plutil -p "/absolute/install-parent/.bpr-update-EXACT-HASH/active.json"
```

If `BetterPullReview.app` exists, use its signed helper:

```sh
/usr/bin/env -i "/absolute/install-parent/BetterPullReview.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
```

If the first rename removed the installed path, use the helper in that exact
journal-owned backup instead, still naming the original installation path:

```sh
/usr/bin/env -i "/absolute/install-parent/.bpr-update-EXACT-HASH/transaction-EXACT-TRANSACTION-ID/backup.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
```

A zero helper exit means the recovery operation completed, not that a newer
version installed. Launch the exact installed application normally and check
General Settings and any recovered draft text. A nonzero exit can mean a live
participant/helper, untrusted or changed material, or an incomplete journal.
Keep all copies and obtain operator assistance; repeated forced launches, deleting
the control directory or clearing quarantine are not recovery procedures. If the
normal helper is unavailable or fails trust checks, preserve the complete
transaction for diagnosis before an administrator performs manual installation.

## Local storage and removal

Application-wide update preferences and the opaque selected-account binding live
in `state/v1/release-update.sqlite3` beneath the application's private registry
root. Each profile's inert recovered text lives in
`state/v1/update-draft-recovery.sqlite3`. The installer records the canonical state
root and its inode, so its empty-environment relaunch cannot choose another store.
Credentials stay in the native vault; review state never moves with the app bundle.

Downloaded staging is private and transaction-owned. Replacement state is in the
sibling control directory: `active.json`, `last-result.json`, locks, participant
records and exact transaction directories. A matching healthy receipt permits
backup cleanup using the saved inode inventory. Changed/new entries are preserved.
Terminal journals and failed candidates can remain for diagnosis; there is no
age-based purge or implemented general updater-storage removal UI. Removing the
app alone can therefore leave private preferences, draft recovery and transaction
data. Preserve an active transaction before a deliberate uninstall; RFC-BPR-0014
still owns the complete uninstall/purge qualification.

## Release operator gate

Build from an identified committed source with tests disabled using the documented
[local release workflow](../../packaging/README.md). The universal ZIP must contain
exactly `BetterPullReview.app`, including `Contents/MacOS/bpr_update_helper`, both
CPU slices, the expected bundle identifier/team and the deployed runtime/notices.
Sign all nested code, use hardened runtime and secure timestamps, notarize and
staple the package, and repeat native signature/trust and relocation checks. A
checksum is integrity metadata, not a publisher trust root. Signing rotation
needs a separately reviewed trust migration.

Before distributing an updater-bearing release, retain source/archive/executable
digests and exercise a participating older signed package through explicit
Download and Install, healthy receipt, retained state, shutdown timeout,
missing-receipt recovery and interrupted-rename recovery. Record native Apple
Silicon and Intel execution separately; universal headers or Rosetta do not prove
Intel hardware acceptance. Use disposable roots and synthetic review state. A
live private-release/vault check requires its own explicitly authorized account
and target, including the observed asset redirect route and revocation. Fixtures
cannot establish those live observations.

The current recorded package course is Apple Silicon only. Owner amendment r3
defers Intel updater validation to [[BPR-0950]], outside M4 and RFC-BPR-0035
closeout; it does not report Intel success or amend RFC-BPR-0014's wider matrix.
Authorized live private-service/vault evidence remains on [[BPR-0911]], with
the explicit RFC-BPR-0014 dependency on [[BPR-0912]]. Building, signing or notarizing a
local artifact grants no tag, push, GitHub release or installed-app replacement
authority. Publication is a separate authorized operation.
