# Application updates

**Check for Updates…** works only on a production-signed macOS installation
that has been copied out of the DMG into a folder you can write to. Windows,
Linux and other macOS installations update manually from the
[release page](https://github.com/MikeMarcin/BetterPullReview/releases/latest).
Administrators can turn self-update off by setting the boolean
`BPRManagedUpdates` preference in the `com.gamecoretech.betterpullreview`
domain to `true`.

In **Settings › General › Software Update**, **Update channel** defaults to
**Stable only**. Choose **Include prereleases** to receive newer numbered
prerelease builds as well as stable releases. You can switch back at any time
before installation starts. A channel change clears the current offer, so use
**Check Now** to see what is available on the selected channel. An installed
prerelease stays in place until a newer build is available on that channel.

The macOS updater can offer a higher build of the same version. Copies from the 0.5.3 release-candidate series shipped with the older
version check. To move from one of those copies to stable 0.5.3, quit
BetterPullReview and replace it with the app from the stable macOS DMG.

Installing an update closes every window. If a GitHub publish was in progress
with an unknown result, check GitHub before publishing again: updating never
retries it.

Text recovered during an update appears in **Saved update drafts**. Use
**Copy** to take the selected text, or **Discard…** to remove a recovery
copy. **Close** keeps the copies. Open them again from **Help › Saved update
drafts** or **Settings › Storage & privacy › Troubleshooting**. Draft comments
already saved in the review are available there when you return to it.

## If an installation is interrupted

The old version is kept as a backup until the new one starts cleanly, so it
can be recovered. Quit every copy of the app first.

1. Next to the app is a control folder named `.bpr-update-<hash>`, where
   `<hash>` is the first 24 lowercase hex characters of the SHA-256 of the
   app's absolute path. Check that its journal names your app and note the
   `transaction` ID:

   ```sh
   /usr/bin/plutil -p "/absolute/install-parent/.bpr-update-EXACT-HASH/active.json"
   ```

2. Run the bundled helper, with every placeholder replaced by an absolute
   value:

   ```sh
   /usr/bin/env -i "/absolute/install-parent/BetterPullReview.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
   ```

   If `BetterPullReview.app` is missing, run the backup's helper instead:

   ```sh
   /usr/bin/env -i "/absolute/install-parent/.bpr-update-EXACT-HASH/transaction-EXACT-TRANSACTION-ID/backup.app/Contents/MacOS/bpr_update_helper" --recover "/absolute/install-parent/BetterPullReview.app" EXACT-TRANSACTION-ID
   ```

3. Exit code 0 means recovery finished. Any other result: keep every copy and
   ask an administrator.

> [!WARNING]
> Do not edit the journal, rename bundles or delete the control folder by
> hand. Recovery has been tested on Apple Silicon only.

Removing only the app leaves update preferences, recovered drafts and the
control folder behind; see
[Storage and diagnostics](storage-and-diagnostics.md).

Maintainers: [release operator gate](application-updates-release-gate.md).
