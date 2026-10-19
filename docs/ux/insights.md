# Insights

Open Insights from BetterPullReview › Insights… on macOS, or Help › Insights…
on Windows and Linux. The shortcut is Command–Option–Shift–I on macOS and
Ctrl–Alt–Shift–I elsewhere. Each profile has one window. Escape closes it.

The content is fixed at 900 × 542 logical pixels and clamped to the opening
screen’s available geometry with an 18 px margin. All six tabs keep that size.
Only bounded ledgers inside a page scroll. The window follows the profile’s font
and appearance. Centered tabs and two icon actions share the native title-bar
row, using Qt's expanded client area and the workbench's macOS title suppression;
the title/status header beneath keeps its own height. Summary uses
compact cards with space below them, rather than stretching them to fill
the page. Storage uses thin rows with budget meters.

The first RFC-BPR-0086 milestone supplies the shell, Summary’s Local data and
Background work tiles, and Storage. The GitHub budget milestone adds the Summary
budget bar and Problems tile and the GitHub tab. Reviews supplies lifetime and
ranged activity, its local-day calendar and both Summary activity tiles. Energy
now supplies native process measurements and the idle-cost tile, with the child
measurement limit below. Work retains its explicit empty states.

The Core budget bar uses the lowest remaining response in the current reset
window, subtracting this profile’s persisted BPR requests to estimate other
clients. Both used counts say “about”. Restart preserves the counts; an expired
or missing reading shows an explicit state. Insights never fetches a reading.
GraphQL and Search show their own remaining budget, reset and BPR request count;
GraphQL has no other-client split because its quota charges query cost.

GitHub’s hour/day chart puts BPR requests per minute above Core remaining, with
separate scales and maxima, interval event bars, a filled budget area, gaps and
reset rules. The tall framed chart sits beside the bounded detail tables, with
its range control centered above it and color swatches below. Hover picks the
nearest sample across midpoints and shows a dated label/value card near the
pointer, flipping horizontally and vertically inside the edges. Changing
range clears hover. The chart follows the profile’s accepted/unread color roles
and exposes its name and range description. Bounded tables show requests by the
closed operation categories, gh launches and observed failures, quarantines,
abandoned children, slowest call and timestamped secondary/Retry-After events.
Problems counts failed operations for 24 hours even when diagnostic recording
is off. Collection is local to the profile and numeric account partition;
repository names, addresses, credentials and response bodies are not retained
in this history.

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
its metadata; lifetime review counts and retained measurement history stay in
the same private store.


Energy samples process CPU every30 seconds while the app is active and every60
seconds otherwise, including when no window is open. An activation change splits
the interval before changing its foreground/background attribution; shutdown saves
the final partial interval. macOS supplies package-idle plus interrupt wakeups.
Other platforms leave wakeups absent and show “Not reported on this platform”.
No measurement is taken by a chart or by a snapshot refresh.

The three leading Energy tiles show background measurements only. CPU seconds
per background hour are compared with the2-second budget, and wakeups per minute
with the6-wakeup budget. A measured zero remains zero; no samples remain an empty
state. Over-budget ranges are named in the tiles. The Summary idle tile always
uses the last hour. Every fact row pairs foreground and background, including
CPU, wakeups, each child family, sample count, wall time and peak CPU load.

The hour/day chart shares one measured duration for its foreground/background
CPU stack and gives wakeups their own lane. The dashed CPU budget line accounts for each interval’s measured background
share, so it stays comparable to the stack when both states share an interval.
The wakeup lane marks6 wakeups per minute. Unsampled spans remain gaps.
The3m calendar uses local days, dot size for total application CPU, and color for
background cost relative to its budget. History keeps2 hours raw,48 hours in
five-minute rollups and93 days in daily buckets, with measured sums and peak load
kept separately for each state.

A child retains its launch state through completion. Exact ordinary Windows
child handles have an implementation, but native Windows qualification is pending.
Qt’s Unix reap currently discards per-child resource usage; those gh/git terminals
show “Child CPU unavailable”, not a measured zero. Supervised Git also stays
unavailable where the helper’s CPU cannot establish the child’s CPU. This is the
open BPR-1612 obligation. Energy is not accepted as a complete milestone yet.


Work shows observations for1h or1d. Its leading figures are stalls over100ms,
files actually computed, and median source-search wall time (acquisition and
matching). The fact table pairs event-loop p95 with the100ms target and names
move-detection work, segment and approximate-alignment counts separately. Syntax
hit rate is hits divided by hits plus submitted classifications; coalesced
requests are not extra lookups. Evictions include the panes, rendered Markdown
and Review Cycle syntax caches. Cached diffs add no computation or timing.
The five slowest local file paths are ordered by each path's longest observed
computation; paths stay on this computer and never enter export.

Qt observes busy event-loop turns, excluding dispatcher sleep and nested modal
waits. It installs no polling timer. Ordinary latency bins have1ms resolution;
stalls retain measured microseconds. Batches flush on the next dispatcher
boundary after30seconds, before a snapshot, and at shutdown. Work history keeps
five-minute buckets through48hours and daily buckets through93days, preserving
weighted duration distributions for percentiles and medians. The snapshot reads
only the retained facts on the Insights worker lane. With no observations, it
names what will populate each figure rather than displaying a measured zero.
