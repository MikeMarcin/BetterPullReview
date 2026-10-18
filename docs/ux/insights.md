# Insights

Open Insights from BetterPullReview › Insights… on macOS, or Help › Insights…
on Windows and Linux. The shortcut is Command–Option–Shift–I on macOS and
Ctrl–Alt–Shift–I elsewhere. Each profile has one window. Escape closes it.

The content is fixed at 900 × 542 logical pixels and clamped to the opening
screen’s available geometry with an 18 px margin. All six tabs keep that size.
Only Storage’s bounded ledger scrolls. The window follows the profile’s font
and appearance. Centered tabs and two icon actions share the compact top
strip; the title/status header beneath keeps its own height. Summary uses
compact cards with space below them, rather than stretching them to fill
the page. Storage uses thin rows with budget meters.

The first RFC-BPR-0086 milestone supplies the shell, Summary’s Local data and
Background work tiles, and Storage. Reviews, GitHub, Work and Energy show
explicit empty states until their respective milestones supply observations.
Those states do not claim a measured zero.

While visible, local snapshots refresh every five seconds when the app is
active, and every thirty seconds otherwise. A refresh already in progress is
skipped. Storage is measured off the UI thread at most once a minute, only
when Summary or Storage is selected. Hiding or closing the window stops both
refresh and status-age timers. Refresh makes no provider request, Git operation
or process launch.

Storage groups each SQLite file in `state/v1` with its WAL, SHM and journal
sidecars, lists diagnostic logs and Review Cycle publication objects, and uses
the Git adapter’s existing inventory walk for each repository cache. It shows
128 MiB for Markdown images, 768 MiB for the Review Cycles store, 50 MiB for
logs and 64 GiB per repository cache. There is no aggregate Git cache quota.
The total counts each component once; repository detail rows are not added
again. An unreadable or linked entry is unavailable, rather than zero.
Repository names learned from accepted reads stay local; an older unnamed
cache uses its abbreviated cache identity until it is opened again.

Reveal data folder opens this profile’s root. Manage cached repositories…
opens the existing Storage & privacy controls. Export anonymized diagnostics…
opens the existing frozen-preview flow, whose Insights section contains only
last-measured byte totals. Export does not start a storage scan. The identifying
export adds no Insights section. This window introduces no deletion action.

`insights.sqlite3` is profile state: profile purge and Reset everything remove
it and its sidecars. Clear local diagnostics, cache deletion or pruning, and
the three narrower Troubleshooting resets keep it. The first launch creates
its metadata; review counting and history arrive in later milestones.
