---
schema: "implement-rfc.card.v1"
id: "BPR-0895"
title: "macOS: the per-user runtime directory is age-reaped, so a long-idle owner can lose its rendezvous"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-006, as a consequence of BPR-0881's rendezvous relocation","found_at":"68f8152e","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"AC1 The card states the measured reaping behaviour: threshold, and whether sockets are exempt","outcome":"pass","revision":"74aca1b8","evidence":{"kind":"observation","detail":"Observed read-only on the dsp-009 host and boot (kern.boottime 2026-09-14 16:51:53; launchctl reports dirhelper runs = 9). Against the 2026-09-23 03:35 run's cutoff (2026-09-20 03:35): 15 sockets older than it by birth and access survive, including dsp-009's 2026-09-14 sockets (six runs) and a stale bpr-activate endpoint born 2026-09-19 13:44. Of 469 regular files, six predate it: four are held open (lsof), two were read after it, and none is both old and idle. So the reap removes regular files over three days old by birth and access, unless held open, and skips sockets.","path":"docs/issues/60-done/BPR-0895-the-macos-runtime-directory-is-age-reaped-under-the-rendezvous.md#the-pending-observation-taken"}},{"criterion":"AC2 An idle owner past the threshold still accepts a handoff, or the card records why that is not worth buying and what the user sees","outcome":"pass","revision":"74aca1b8","evidence":{"kind":"observation","detail":"Not worth buying: the reap skips sockets, so an owner idle past three days keeps its endpoint name and a second launch reaches it; the observation includes a bpr-activate socket past the cutoff. Only the boot-time clean removes sockets, and it ends the owner too. The user sees nothing different. No product change was made; recorded in the card's Decision section.","path":"docs/issues/60-done/BPR-0895-the-macos-runtime-directory-is-age-reaped-under-the-rendezvous.md#decision-no-product-change"}},{"criterion":"AC3 Linux and Windows are unaffected","outcome":"pass","revision":"74aca1b8","evidence":{"kind":"observation","detail":"The product change is a comment in POSIX runtime_directory(); the new row is POSIX-only. Windows' named pipe has no runtime directory and no Windows code changed. Linux behaviour is unchanged; the new row was not observed on Linux by this card."}},{"criterion":"AC4 The contract covers what was chosen, so a future change cannot quietly reintroduce the gap","outcome":"pass","revision":"74aca1b8","evidence":{"kind":"command","detail":"sqlite_profile_activation passes with test_rendezvous_keeps_only_its_socket, which requires binding to add nothing but sockets among the runtime directory's bpr- entries and to add the endpoint. Negative control: binding also creating <endpoint>.pid as a regular file fails with 'binding adds nothing but sockets to the runtime directory'; restored, it passes. runtime_directory() points at the card and the row. architecture_boundaries and architecture_boundary_contract pass; clang-format and project tidy clean."}}]
---

# BPR-0895: macOS: the per-user runtime directory is age-reaped, so a long-idle owner can lose its rendezvous

## Defect

[[BPR-0881]] moved the profile activation rendezvous out of the data root,
because the in-root path exceeded `sun_path` for every user name longer than
one character. It now lives in the first per-user runtime directory that
verifies owner-private: `/run/user/<uid>`, then `$TMPDIR`, then an
owner-private `bpr-<uid>` child of `/tmp`.

On macOS that resolves to `$TMPDIR`
(`/var/folders/<a>/<b>/T/`), and **macOS age-reaps that directory**. Linux's
`/run/user/<uid>` has no equivalent exposure: it is cleared at logout, which is
also when the owner process dies.

So on macOS a long-running owner whose socket is reaped keeps running while its
endpoint file disappears. The next launch finds no socket, cannot hand off, and
degrades to `owner_busy` — the exact symptom BPR-0881 fixed — until the owner
restarts. The owner's listening file descriptor survives; the *name* does not.

This is a narrower window than the defect it replaces: it needs an owner idle
long enough to be reaped, rather than any user name of two characters or more.
It is recorded because it is a known residual of a change made deliberately,
not a discovery waiting to happen.

## Scope

- Establish the real exposure before choosing a fix. macOS reaps `$TMPDIR`
  contents by access time, and the reaper's threshold and whether it skips
  sockets should be measured, not assumed from folklore.
- Then decide between the plausible answers:
  - have the owner keep its endpoint alive (touch it on a long interval, or on
    each accepted activation), which is the smallest change and matches how
    other long-lived Unix rendezvous survive reaping;
  - or have the *client* treat a missing endpoint as a possible reap rather
    than a possible absence, and give the owner a way to re-bind;
  - or choose a macOS location that is not reaped, which would mean giving up
    the per-user 0700 guarantee `$TMPDIR` provides for free — the reason it was
    chosen.
- Whatever is chosen must keep every guarantee BPR-0881 records: the
  private-storage root still gates the rendezvous, the socket is still
  `S_ISSOCK` + owner + `0600`, peer credentials are still checked, and the
  runtime directory is still verified owner-private without following links.

## Acceptance

- The card states the measured reaping behaviour — threshold, and whether
  sockets are exempt — rather than reasoning about it.
- An owner that has been idle past that threshold still accepts a handoff from
  a second launch, or the card records why that is not worth buying and what
  the user sees instead.
- Linux and Windows are unaffected.
- The contract covers whatever is chosen, so a future change cannot quietly
  reintroduce the gap.

## Measured on this macOS: the age reap skips sockets, and the premise above is probably false — dsp-009

Read-only investigation of the installation itself (Darwin 25.6.0, macOS
Tahoe 26.6.2), by an `rfc-scout` on Fable; nothing was run as root and nothing
in `$TMPDIR` was touched. Evidence class is stated per finding, because two
of them come from disassembly rather than from a run.

**The reaper is `/usr/libexec/dirhelper`, not `periodic`.** Launched by
`/System/Library/LaunchDaemons/com.apple.bsd.dirhelper.plist` with
`CLEAN_FILES_OLDER_THAN_DAYS = 3`, `RunAtLoad = true`, and a daily
`StartCalendarInterval` at 03:35. `/etc/periodic`, `/etc/defaults/periodic.conf`
and the `periodic` binary do not exist on this install. `$TMPDIR` is exactly
`getconf DARWIN_USER_TEMP_DIR` (`/var/folders/<a>/<b>/T/`), and dirhelper's
directory list for each `/var/folders/<a>/<b>/` is `{"T/", "TemporaryItems"}`,
so the `T/` that holds `bpr-activate-<digest>.sock` is in scope. *Config-
established, plus `otool` on the binary.*

**Threshold and criterion.** Cutoff is `now - 3*86400`. The walker requests
`ATTR_CMN_ACCTIME | ATTR_CMN_CRTIME | ATTR_CMN_OBJTYPE` (no MODTIME, no
CHGTIME), and deletes a *regular file* only when **both** its birth time and its
access time are older than the cutoff. mtime and ctime are not consulted.
*Disassembly-established.*

**Sockets are exempt from the age reap.** The per-entry dispatch handles
`VDIR` and `VREG`; every other object type (`VSOCK`, `VFIFO`, `VLNK`, …) is
skipped whenever the cutoff is nonzero. The `unlink` path for non-regular
entries is reachable only with cutoff 0. *Disassembly-established, not yet
observed:* 13 Unix sockets (Chrome, Spotify, Codex, `vscode-git-*`) currently
live in `$TMPDIR` with birth = atime = 2026-09-14 16:52, i.e. they became
three days old at 16:52 on 2026-09-17, after that day's 03:35 run. **The first
run that can decide on a >3-day-old socket is 2026-09-18 03:35.** If those
sockets are still present after it, the exemption is observed; `stat -f '%SB
%HT %N'` over `$TMPDIR` is the whole check.

**The mode that does remove sockets is boot.** `/sbin/launchd` invokes
`dirhelper -machineBoot`, which cleans `{"T/", "TemporaryItems", "Cleanup At
Startup", "X/"}` with cutoff 0 — everything, sockets included. Corroborated:
`kern.boottime` is 2026-09-14 16:51:52 and nothing in `$TMPDIR` (1,826
entries) predates 16:52 that day. The owner process dies at reboot too, so
this is the same lifetime Linux's `/run/user/<uid>` has, not the card's
"idle owner loses its name" case.

**The third fallback, `/tmp/bpr-<uid>`,** is cleaned by `/usr/libexec/tmp_cleaner`
(daily at 00:00) with `find -dx . -fstype local -type f -atime +3 -mtime +3
-ctime +3 -delete`: regular files only, all three times, sockets exempt.
*Config-established.*

### What this does to the card

The defect as written — an idle owner keeps running while its endpoint file is
reaped — needs the age reap to remove a socket, and on this system's own
reaper it does not. So, pending the one observation above, **the exposure is
nil and no product change is warranted**: touching the socket, re-binding, or
moving off `$TMPDIR` would each be paying for a reap that does not happen.
Acceptance point 1 is satisfied by the system's configuration plus disassembly
with the observation pending; point 2 becomes moot if the observation
confirms; the "contract covers it" point cannot be a test row, because no row
can drive dirhelper, and should instead be this record plus a pointer from the
rendezvous code to it. The decision stays joint with [[BPR-0906]], whose Linux
measurement is on that card: reclaimed not refused, accumulation inert,
harness debris from rows that `terminate()` a live owner.

Limitations, verbatim from the scout: the socket exemption and the
crtime-and-atime criterion are read from arm64e disassembly, internally
consistent (the `lstat` fallback fills the same slots; the `attrlist` bitmask
matches) but a mis-read is possible; no dirhelper "Cleaning … older than"
lines were visible in the unified log without sudo; the `-machineBoot` path was
established from a launchd string plus the post-boot emptiness, not traced;
`com.apple.cache_delete` was not analysed.

## Resolution — 2026-09-23, Mac integrator (Mac batch 2)

### The pending observation, taken

Read-only, on the same Mac and the same boot as dsp-009 (`kern.boottime`
2026-09-14 16:51:53). `launchctl print system/com.apple.bsd.dirhelper` reports
`runs = 9` and a normal idle exit, so the daily reap has run since. At
2026-09-23 10:47, over `$TMPDIR` (`getconf DARWIN_USER_TEMP_DIR`), with the
03:35 run's cutoff at 2026-09-20 03:35:

- **Sockets past the cutoff survive.** Fifteen sockets whose birth and access
  times both predate it are present. They include every one dsp-009 listed from
  2026-09-14 16:52 (the Chrome, Codex and Spotify `SingletonSocket`s,
  `discord-ipc-0`, `vscode-git-3951a7ae11.sock`), past the cutoff for six daily
  runs now, and a stale `bpr-activate-c94cdabddc2523d3.sock` (birth and access
  2026-09-19 13:44) that no process holds.
- **Idle regular files past it do not.** Of 469 regular files, six were born
  before the cutoff. Four are held open by a live process (`lsof`: three
  `cpptools` crash logs and `adb.501.log`); the other two were read after the
  cutoff (2026-09-21 and 2026-09-23). None is both idle and old. Births then
  run 104, 49, 287 and 32 a day from 2026-09-20 on.

So the age reap removes a regular file whose birth and access times are both
more than three days old, and skips sockets. That is observed here, not read
from disassembly. There is one refinement to dsp-009's criterion: a regular
file that a process holds open was not removed either.

### Decision: no product change

The exposure is nil. The reap does not remove the endpoint, so an owner idle
past three days keeps its name and a second launch reaches it as it does on
the first day. The only macOS event that removes the socket is the boot-time
clean (`dirhelper -machineBoot`), and that ends the owner too, which is the
lifetime Linux's `/run/user/<uid>` has. Touching the socket, re-binding it or
moving off `$TMPDIR` would each pay for a reap that does not happen. The user
sees nothing different from today.

What would bring the gap back is the rendezvous keeping a regular file in the
runtime directory, such as a lock, pid or marker beside the socket. The reap
would remove that after three idle days. The contract now covers it (`74aca1b8`):

- `sqlite_profile_activation`'s new POSIX row
  `test_rendezvous_keeps_only_its_socket` lists the runtime directory's `bpr-`
  entries before and after binding. It requires every entry binding adds to be
  a socket, and the endpoint to be one of them. Only `bpr-` names are compared
  because Linux's `/run/user/<uid>` is shared with everything else the user
  runs.
- `runtime_directory()` in `src/adapters/sqlite/src/profile_activation.cpp`
  states the measured reap and points at this card and that row.

### Consistent with BPR-0906

Nothing here keeps an endpoint alive or re-binds it, so the accumulation
[[BPR-0906]] describes gets no worse, and that card stays free to choose
unlink-on-exit or a sweep. The same observation shows the accumulation is not
Linux-only. This Mac's `$TMPDIR` holds 20 `bpr-activate-*.sock` entries born
between 2026-09-19 and 2026-09-22, none held by any process (`lsof -U`). They
are test-run debris the reap leaves because it skips sockets. BPR-0906 is noted
accordingly.

## Evidence

At `74aca1b8`, macOS 26.6.2 arm64, `build/dev`:

- `sqlite_profile_activation` passes (4.2 s) with the new row.
- **Negative control.** With binding also creating `<endpoint>.pid` as a
  regular file, the row fails with `binding adds nothing but sockets to the
  runtime directory`. With the source restored it passes again.
- `architecture_boundaries` (17.8 s) and `architecture_boundary_contract`
  (91.8 s) pass. They scan `profile_activation.cpp` for forbidden runtime
  surfaces, so the new comment avoids spelling them.
- clang-format 23.1.1 passes both files. The project tidy script reports
  nothing on either translation unit.
- Windows is unaffected. The row and the comment are in POSIX-only code, and
  Windows' named pipe has no runtime directory. Linux runs the same row. The
  product behaviour there is unchanged, since the product change is a comment,
  but this card did not observe the row on Linux.
