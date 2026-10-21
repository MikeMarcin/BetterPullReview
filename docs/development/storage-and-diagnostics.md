# Storage, diagnostics and removal

What BetterPullReview keeps on your computer, what leaves it, and how to
remove it.

> [!WARNING]
> Credential-vault handling, crash privacy and complete removal are not yet
> qualified on every platform. See the
> [qualification record](../verification/RFC-BPR-0014.md).

## Where your data is

| Platform | Default profile folder |
|---|---|
| macOS | `~/Library/Application Support/BetterPullReview/BetterPullReview` |
| Windows | `%LOCALAPPDATA%\BetterPullReview\BetterPullReview` |
| Linux | `~/.local/share/BetterPullReview/BetterPullReview` (or under `$XDG_DATA_HOME`) |

Other profiles live in `profiles/<id>` under it. Private notes, drafts, tours
and Review Cycles exist only here and cannot be rebuilt from GitHub. They are
not encrypted; the folders are readable only by your account.

To back up, quit BetterPullReview and copy the whole profile folder, including
every `-wal` and `-shm` file. Never edit live databases or delete lock files.
Back up before changing versions: an older version may not open newer data.

Sign-in tokens live in the operating system's credential store. Manual tokens
are dropped when you quit. Credentials held by `gh` or an AI agent are never
touched.

## Diagnostics

Diagnostic logs stay on this computer, per profile, for 30 days (at most
50 MiB). Names, paths and review text are never recorded.

To share diagnostics, use **Help > Export anonymized diagnostics…**. The
preview shows exactly what the file will contain, and saving only writes a
file: nothing is uploaded. The file is a `.zip` holding that text as one
`.txt` file, small enough to attach to an issue. **Export with identifying
details…** also keeps request and window IDs.

> [!WARNING]
> Logs written by earlier versions can contain provider text and identifying
> details, so send an export rather than a raw log. Operating-system crash
> reports bypass the export preview and may contain source or comments.

## Reset this profile's local data

The resets in **Settings > Storage & privacy > Troubleshooting** keep you
signed in and change nothing on GitHub.

> [!WARNING]
> **Clear caches** loses earlier pushes that GitHub dropped after a
> force-push. **Reset review data** and **Reset everything** delete notes and
> unsent comments that exist only here; leave **Save a copy … in Downloads**
> ticked to keep a copy.

If a profile cannot open, BetterPullReview offers **Clear caches and try
again**. If it crashes before it can ask, start it with
`--reset-profile-data`; on macOS:

```sh
/Applications/BetterPullReview.app/Contents/MacOS/BetterPullReview --reset-profile-data
```

## Purge a profile

**Prepare profile purge…** stops the profile and lists exactly what will be
removed before you confirm. Purge removes private data, retained Git history,
agent material and remembered sign-ins. It leaves GitHub, your checkouts,
other profiles, shared update state and exported files.

> [!WARNING]
> Purge cannot be undone and is not a secure erase. Never delete a profile
> folder by hand.

## Uninstall

Uninstalling removes only the program. To leave nothing behind:

1. Finish any publish or update and sign out of each account.
2. Purge every profile.
3. Uninstall the app.

The profile list, shared update state and exports still remain afterwards.

## Agent workspaces are unavailable

If the status bar says **Disposable agent workspaces are unavailable**, a
parent folder of your data is usually writable by other users. On macOS and
Linux run `chmod go-w` on the folder that holds the default profile; on
Windows remove other accounts' write access.
