# BPR-2135: reset replacement uses a live file identity

## Failure and correction

Frozen60 Linux Debug `application_profile_recovery_public_path` failed at
`Reset everything replaced the review data`. The original test compared a
ledger's numeric inode before reset with a file opened after reset, without
retaining the original file object. A POSIX unlink/recreation may reuse that
number. Its original output is retained; it contains no observation that can
identify the old object after its lifetime ended. This is a fixture identity
correction, with no claim that a product data-loss bug or Qt issue was fixed.

The reset contract removes the selected review stores before application
startup opens them again. The POSIX adapter selects `review-ledger.sqlite3`
and its sidecars, revalidates its captured file identity, then performs
`unlinkat` and directory `fsync`.

The corrected course puts a unique row in a disposable ledger while its app
is stopped. The no-reset control reads a private copy and requires that row
to remain. On POSIX it also holds a noninheritable read descriptor through
reset, keeping the old inode allocated. After the complete public reset it
requires the old descriptor's link count to be zero, the existing inode
replacement assertion to succeed, and the old row to be absent from a private
copy. A no-op reset fails both the unlink and data-removal checks.

Windows retains the original inode comparison without changing delete-sharing
behavior, and gains the data-removal/no-reset controls. Healthy startup, Quit,
cache recovery, retained stores/logs/connections and refused copy-without-reset
checks and their original time bounds remain. No product source, graph,
compiled library, executable, app pin or release configuration changes.

## Exact candidate and native proof

- Frozen source: `6efba00e1afbf874c88c7e5de2623a33528ff36f`.
- Final Python SHA-256: `4fec311061a37dda8dcceb8ababce06eb7cf037c728b6ca1aa5ddfd303d5fad1`.
- Private sentinel commit: `b9c814499ca76b89be2807a7e85d1fad205e8db7`; Main `fc1b985fcc291543084663b79dc354050561347f`.
- Private live-identity commit: `e165af950ed30fd8675cc5999da46d6f0b9f4664`; Main `85ed1c7574ff3b7aabe1a100c5e0e257d52eeb20`.
- Runtime input closure: only `tests/end_to_end/profile_recovery_course.py`.

Both final courses ran serially on the native ordinary-UID1000 Ubuntu26.04
WSL2 producer using unchanged compiled60 Release/Debug apps. The actual
configured sandbox, interpreter, environment, working directory and watchdog
were retained. The Python script token passed through a private runpy loader
that verified and reused the stock helper modules; temporary data stayed in
private children of the configured runtime roots.

| Configuration | Exit | Full course reached | Protected inputs exact | Remaining owned forest |
| --- | --- | --- | --- | --- |
| Release | 0 | True | True | empty |
| Debug | 0 | True | True | empty |

Release: old inode41794, new41810. Debug: old41800, new41811. Both explicitly
observed `old_file_unlinked=True previous_data_removed=True
no_reset_replacement_rejected=True` and `PROFILE_RECOVERY_COURSE_OK`. The
noninherited POSIX descriptor makes this an observation of actual old-object
unlinking, independent of inode reuse. The original input/failure remains.

Before/after seals cover the normal source, stock script/support, reset adapter,
allocation/cohort, app, interpreter, network sandbox, CTest registration/cache,
maintained supervisor and staged script/loader/driver. All match; normal source
remains clean6ef. The four configured-row/course steps exit0 with empty retained
process forests. No source/build input was modified.

The earlier sentinel-only candidate also passed full native Release/Debug, but
its inodes differed; those results were not misrepresented as reproduction of
the original equality. The final source adds a live object anchor and was
verified again because its identity assertion changed. No source revision
was rerun until green.

## Evidence and remaining release work

External evidence root:
`/Users/jaeger/.codex/engineering-runs/bpr60-workspace-fixture-r1/linux-blockers-r1`.
`reset-native-r2/raw/receipt.json` SHA-256: `3127dcb02c5d3c5899bfdf8327940c15f7937a3b2f05fe0ae02890382f7efaa4`.
Its sibling raw files retain complete stdout/stderr, exact configured rows,
argv, selected input hashes, native process identities and sentinel observations.
`reset-native-r1/raw/` retains the earlier candidate separately.

Python syntax and diff checks and the documentation/owned plan gates cover
this narrow fixture slice. Source60/public assets remain immutable. These
are focused engineering proofs against compiled60, not successor release
qualification. The coordinator owns selective intake and any evidence bridge.

Cross-platform closure is explicit: the added sentinel uses existing Python
SQLite and the course's stopped disposable stores on every platform; POSIX
adds only a descriptor lifetime/link check, while Windows leaves deletion
sharing unchanged. Linux Release/Debug are measured here. No new macOS or
Windows native pass is claimed; their affected fixture gates can be selected
by the coordinator for successor verification without rebuilding unchanged
product inputs. Unrelated latency/submission/multi-window blockers remain
separate issues, and global foreign plan-schema drift remains recorded.
