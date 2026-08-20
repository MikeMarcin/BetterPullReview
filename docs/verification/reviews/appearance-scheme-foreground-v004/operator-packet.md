# BPR-0114/BPR-0220 selected-diff foreground appearance operator packet

Status: **superseded by
[`appearance-scheme-foreground-v007`](../appearance-scheme-foreground-v007/operator-packet.md);
do not run or approve.**

This packet is preserved as the superseded v004 procedure, and it is the one
version here that must not be attempted again. The owner ran it twice and it
failed both times with the same signature —
`NATIVE_APPEARANCE_TRANSITION:Light->Dark` observed, then
`NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED` and `NATIVE_APPEARANCE_STATE_FAILED`,
and no rasters. The first attempt's log is preserved beside this file as
`foreground-run-attempt-1.log`.

That was not operator error. [[BPR-0391]] established that the dark-side
measurement fired about half a second after all three schemes read Dark and
measured **once**, requiring focus on the exact widget the baseline recorded —
which a person who must surrender frontmost to System Settings cannot supply in
time. The v004 procedure was humanly impossible, and v005 replaced it. No
response to this version can satisfy [[BPR-0114]] or [[BPR-0220]].

This packet requests one bounded human action on macOS: establish a frontmost
real selected diff, change the desktop appearance from Light to Dark in System
Settings, return focus to the same diff, and restore the original desktop
appearance after the application exits.

This is an observation packet, not a palette-quality approval. It does not
decide whether the measured geometry-rebuild count is correct, and it does not
close BPR-0114, BPR-0220, or RFC-BPR-0006 by itself.

This v004 packet supersedes the never-run v003 packet. v003 bound a dev
executable that no longer exists at its recorded digest, so its procedure
became unexecutable by construction. The bound BPR-0249 implementation record
is byte-identical to v003's; only its board path moved to `60-done/` and the
executable digest changed with the rebuild at commit
`0e34519f2d49b035e2caa4c801ea4827ba8e07db`.

## Why a person is required

The probe deliberately refuses to infer a native result offscreen. It requires
the real Cocoa window to be active, focused, and frontmost when the Light
baseline is accepted and again when the Dark result is measured. The desktop
owner must also make and restore the global macOS appearance change. Repository
automation must not synthesize the focus gesture or mutate the global setting.

## Bound evidence

Run only these exact immutable inputs:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review` | `sha256:7fc2a16ade93ec35c322b927eb8cb4811a29e2a102c5a64d51eb3bb30a6a0ade` | 51132976 |
| `docs/issues/60-done/BPR-0249-make-native-appearance-probe-await-selected-diff.md` | `sha256:fe76c86329e9eb6650a9fa11103739fb0bfae0480a174b69689a74d057ff153b` | 4144 |

The bound artifact digests, rather than the mutable repository branch head, are
the packet authority. BPR-0114 and BPR-0220 are the mutable decision targets and
are deliberately not digest authorities: recording this run must update those
cards without invalidating the human statement.

Before starting, verify the bound inputs:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 \
  build/dev/src/app/better_pull_review \
  docs/issues/60-done/BPR-0249-make-native-appearance-probe-await-selected-diff.md
```

Do not proceed unless the two digests exactly match the table.

## Operator procedure

1. Note the desktop appearance that was active before this procedure. It must
   be restored and visibly confirmed at the end.
2. In macOS System Settings, set Appearance to **Light** and confirm the desktop
   is visibly light.
3. Run this exact command in Terminal. It uses new v004 paths and must not
   delete, edit, or overwrite any earlier `/tmp/bpr-0114-foreground-v00*.log`.

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
set -o pipefail
bpr_appearance_root=$(mktemp -d /tmp/bpr-appearance-foreground-v004.XXXXXX)
bpr_appearance_log=$bpr_appearance_root/bpr-0114-foreground-v004.log
printf 'BPR_FOREGROUND_ARTIFACT_ROOT:%s\n' "$bpr_appearance_root" \
  | tee "$bpr_appearance_log"
./build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --native-appearance-probe \
  --render-evidence "$bpr_appearance_root/before.png" \
  --changed-render-evidence "$bpr_appearance_root/after.png" \
  --automated-run-budget-ms 300000 \
  2>&1 | tee -a "$bpr_appearance_log"
bpr_appearance_status=$pipestatus[1]
printf 'BPR_FOREGROUND_EXIT_STATUS:%s\n' "$bpr_appearance_status" \
  | tee -a "$bpr_appearance_log"
for bpr_appearance_artifact in \
  "$bpr_appearance_root/before.png" \
  "$bpr_appearance_root/after.png"
do
  if test -f "$bpr_appearance_artifact"
  then
    shasum -a 256 "$bpr_appearance_artifact"
    stat -f 'BPR_FOREGROUND_ARTIFACT_BYTES:%z:%N' "$bpr_appearance_artifact"
  else
    printf 'BPR_FOREGROUND_ARTIFACT_MISSING:%s\n' "$bpr_appearance_artifact"
  fi
done | tee -a "$bpr_appearance_log"
printf 'BPR_FOREGROUND_LOG:%s\n' "$bpr_appearance_log" \
  | tee -a "$bpr_appearance_log"
```

4. When the BetterPullReview window appears, wait until `review.cpp` is visible
   in both code panes. Click inside the left code pane to make the application
   active and focused. Leave it frontmost briefly so the Light baseline can be
   accepted.
5. Open macOS System Settings and change Appearance from **Light** to **Dark**.
   Return immediately to BetterPullReview, click the same left code pane, and
   leave BetterPullReview frontmost until it exits.
6. Restore the desktop appearance noted in step 1 using System Settings, and
   visibly confirm the restoration.
7. Return the complete contents of
   the path printed by `BPR_FOREGROUND_LOG`, including the artifact-root,
   exit-status, and artifact lines, followed by the exact statement below.

Do not set `QT_QPA_PLATFORM=offscreen`, automate either click, automate the
global appearance change, edit the log, reuse a v001/v002/v003 artifact path,
or rerun against a rebuilt binary under this packet.

## Required log audit

A usable run must first prove that appearance was measured on the real selected
diff composed by BPR-0227/BPR-0249:

- `NATIVE_APPEARANCE_SELECTED_DIFF_READY`
- `NATIVE_APPEARANCE_SELECTED_DIFF_REASON:none`
- `NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:111111`
- `NATIVE_APPEARANCE_SELECTION_WIRE_OK`
- `NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK`

It must then include the complete native observation:

- `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`
- `NATIVE_APPEARANCE_TRANSITION:Light->Dark`
- `NATIVE_APPEARANCE_GLOBAL_TRANSITION:Light->Dark`
- `NATIVE_APPEARANCE_SIGNAL_OK`
- `APPEARANCE_SYSTEM_FOLLOWS_OK`
- `APPEARANCE_EXPLICIT_IGNORES_OK`
- `APPEARANCE_VARIANTS:0:1:0:1`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:<n>`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_CAUSES:<n>:...`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_OK`
- `PUBLIC_PATH_VERDICT:` with `shutdown_complete=1 cleanup_failed=0`
- `PUBLIC_PATH_BUDGET:expired=0 budget_ms=300000`
- one `BPR_FOREGROUND_ARTIFACT_ROOT:/tmp/bpr-appearance-foreground-v004.*`
- `BPR_FOREGROUND_EXIT_STATUS:<n>`
- one matching `BPR_FOREGROUND_LOG:.../bpr-0114-foreground-v004.log`
- two artifact digest/byte records and no `BPR_FOREGROUND_ARTIFACT_MISSING`

The process exit status and the exact `== 1U` geometry checks are not the human
verdict. A measured delta other than one may deliberately make the process exit
nonzero while still supplying the complete cause record BPR-0220 requires. Do
not reinterpret, normalize, omit, or rerun that result.

Each geometry cause is `measured-change-mask/Qt-trigger-mask` in successful
rebuild order. The measured-change bits are:

| Bit | Change |
|---:|---|
| 1 | accepted diff |
| 2 | file or epoch |
| 4 | font |
| 8 | zoom |
| 16 | tab width |
| 32 | device pixel ratio |
| 64 | screen |
| 128 | viewport or divider |
| 256 | wrapping |
| 512 | folding |
| 1024 | document layout |
| 2048 | variable-height item |

The exact Qt-trigger bits are:

| Bit | Trigger |
|---:|---|
| 1 | accepted diff |
| 2 | font event |
| 4 | style event |
| 8 | show event |
| 16 | device-pixel-ratio event |
| 32 | screen-change event |
| 64 | tab-width adoption |
| 128 | pane viewport resize |
| 256 | workbench resize |

## Operator response

Paste the complete v004 log, then include this exact statement:

> I ran the BPR-0114/BPR-0220 selected-diff foreground appearance probe
> against executable
> sha256:7fc2a16ade93ec35c322b927eb8cb4811a29e2a102c5a64d51eb3bb30a6a0ade
> and BPR-0249 implementation record
> sha256:fe76c86329e9eb6650a9fa11103739fb0bfae0480a174b69689a74d057ff153b;
> I established the visible review.cpp comparison in Light, focused its left
> code pane, changed macOS Appearance from Light to Dark in System Settings,
> returned the same pane to the front until the process exited, restored and
> visibly confirmed my original desktop appearance, and the pasted v004 log is
> that run's complete unedited output.

If the procedure cannot be completed, report the exact obstruction instead.
No partial response, visual-quality judgement, or inferred geometry result
closes either card.
