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

Other profiles live in `profiles/<id>` under it. Private notes, drafts, tours,
local PR drafts and agent conversations exist only here and
cannot be rebuilt from GitHub. They are
not encrypted; the folders are readable only by your account.

To back up, quit BetterPullReview and copy the whole profile folder, including
every `-wal` and `-shm` file. Never edit live databases or delete lock files.
Back up before changing versions: an older version may not open newer data.

Agent scratch folders, where an agent may write and run scripts, sit outside
the profile folder: beside it, in a folder named after it with
`-agent-scratch-v1` appended. When that path contains a space, as macOS's
`Application Support` does, they go under the user's cache folder instead, in
`betterpullreview-agent-scratch-v1/<hash>` (on macOS,
`~/Library/Caches/betterpullreview-agent-scratch-v1`). Windows has no scratch
folders. By default a folder unused for 30 days is removed when
BetterPullReview starts, and a folder's limit is 256 MiB. Both are set, and
the folders cleared, in [Settings > Agents](settings.md#agents).

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

Tour publication records its preview, admission, provider setup, each publication
operation and its terminal or recovery in the same profile log. A refusal names
the stage, classified reason and HTTP status when one was received. An opaque
attempt digest connects those records without recording your tour text. These
records are retained in anonymized diagnostic export too. A screen saying that
publication is unavailable does not by itself establish a server, credential or
qualification cause.

## Clear this profile's local data

**Settings > Storage & privacy > Clear local data** lists each kind of local
data with a checkbox and one line saying exactly what it holds. Check any
number, choose **Clear…**, and confirm once; the profile's windows close, the
checked data goes, and Home reopens with what was cleared, naming any kind
that could not be. Nothing on GitHub changes. **Check all** is the former
**Reset everything**.

| Checkbox | Clears |
|---|---|
| Caches | Repository data downloaded from GitHub, images, Home's summaries and agent context snapshots. They download again. |
| Settings | Every choice in Settings, agent setup and prompts, notifications, filters and window layout (`agent-setup.sqlite3` among them). Agent conversations are kept. Includes Agent setup. |
| Agent setup | The former **Reset agents…**: every agent you added with its model, effort and check, the clone folders and linked clones, and an edited review skill (a copy goes to Downloads first). Setup starts again; prompts and conversations stay. |
| Agent sessions | BetterPullReview's agent sessions (transcripts, names, costs, finding card states) in `agent-sessions.sqlite3`, and the follow-ups queued to send next. Includes Agent workspaces. |
| Agent workspaces | The former workspace **Remove…**: every retained agent workspace not in use, with its measured size beside the box. Conversations stay and make a fresh one when they next ask. |
| Review data | Read and seen marks, private notes, unsent comments and reviews, tours and Review Cycles. |
| Saved repositories and servers | Saved repositories and servers, observer rules and Home's queue. |
| Insights statistics | Insights' lifetime statistics and history. |
| Connections | Sign-ins made in this app and their saved tokens, and this profile's GitHub CLI connection. |

**Reset agents…** (Settings › Agents and the Agent tab's History), the
workspaces' **Remove…** and the unreadable agent setup's recovery open this
list with their box checked.

Agent conversations live in their own store since this change; the first
start of a newer build moves an earlier build's out of `agent-setup.sqlite3`,
in two steps that each finish or leave nothing changed, so a stop part-way
finishes at the next start. Agent workspaces are removed first, through their
exact ownership records, while the conversations that name them still exist.
One another app holds, or whose ownership cannot be verified, stays, and so
do the conversations that record it; the notice says so. No category touches
Claude Code's or Codex's own session files under their home folders: they
belong to those tools.

**Connections** signs this profile out and opens at **Sign in to GitHub**; the
app connects through gh again only when you choose **Use GitHub CLI**. It
never signs you out of GitHub CLI itself, which other tools use (`gh auth
logout` does that). Nothing here is an uninstall, and nothing touches shared
registry or updater state, diagnostic logs or other profiles.

> [!WARNING]
> **Caches** loses earlier pushes that GitHub dropped after a force-push.
> **Review data**, **Agent sessions**, **Agent workspaces** (changes an agent
> made there and did not publish), **Saved repositories and servers** and
> **Insights statistics** delete data that exists only here and ask you to
> acknowledge that first; leave **Save a copy … in Downloads** ticked to keep
> a copy of the removed store files.

Agent sessions can also be renamed or deleted one at a time in the Agent
tab's and a tour chat's session list: each session's row has a pencil
(rename in place; an empty name shows its first question again), a trash can
(delete, after a question) and an i (its turns, revision and cost), and
**Delete all sessions…** sits below the rows. Deleting one removes its
transcript and name, and its retained workspace when no other session of the
pull request uses it; it is refused while a turn runs on it.

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

A restart alone must never cause it ([[BPR-2118]]). When the sentence names a
workspace record, it says which check refused it, the first record, and how
many records were refused. Quit BetterPullReview, move every refused record
from `agent-workspace-ownership-v1` and its `betterpullreview-agent-<token>`
folder from `BetterPullReview-agent-workspaces-v1` into a backup folder
outside both, then restart. Leave `.git-process-boundary.lock` where it is.
Nothing is deleted, but the app no longer knows about the moved workspaces.
