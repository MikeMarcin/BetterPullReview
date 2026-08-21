# BPR-0114/BPR-0220/BPR-0410 selected-diff foreground appearance operator packet

Status: **pending operator run; not palette-quality approval or RFC closeout**

This packet requests one bounded human action on macOS: establish a frontmost
real selected diff, change the desktop appearance from Light to Dark in System
Settings, return focus to the same diff, and restore the original desktop
appearance after the application exits. Return the complete raw log so the new
BPR-0410 owner-return and capture markers can be audited without inference.

This is an observation packet. It does not decide whether the measured
geometry-rebuild count is correct, judge presentation quality, or close
BPR-0114, BPR-0220, BPR-0410, or RFC-BPR-0006 by itself.

## Why v008 exists

v007 was run against executable
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93`.
It observed Light to Dark, then reported the collapsed
`NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED` marker and no rasters. That run remains
historical evidence, but its marker could not distinguish owner focus from the
unrelated appearance-state and capture failures folded into the same verdict.

BPR-0410 replaced that ambiguity without weakening the strict owner-return
predicate. Every wait entry is now indexed, a successful strict return is
reported separately, the three owner-return operands are printed independently,
and capture and persistence failures carry factual dispositions. The authorized
BPR-0409/BPR-0410/BPR-0334 relink batch then settled at committed source HEAD
`145f6d4c6345c65c42c6d5ea00dfc9b67fa514b0`. v008 supersedes v007 as the live
question and binds the resulting final executable bytes. It does not rewrite or
reinterpret the v007 attempt.

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
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28` | 51,383,520 |
| `docs/issues/60-done/BPR-0249-make-native-appearance-probe-await-selected-diff.md` | `sha256:fe76c86329e9eb6650a9fa11103739fb0bfae0480a174b69689a74d057ff153b` | 4,144 |

The bound artifact digests, rather than the mutable repository branch head, are
the packet authority. BPR-0114, BPR-0220, and BPR-0410 are mutable decision
targets and are deliberately not digest authorities: recording this run must be
able to update those cards without invalidating the human statement.

Before starting, verify the bound inputs:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  docs/issues/60-done/BPR-0249-make-native-appearance-probe-await-selected-diff.md
```

Do not proceed unless both digests exactly match the table. The deterministic
`application_native_appearance_probe_contract` preflight passed against these
executable bytes. It proves the selected-diff wire, strict predicate, marker
structure, refusal behavior, and diagnostic control flow; it cannot perform the
native Cocoa Light-to-Dark sequence.

## Operator procedure

1. Note the desktop appearance that was active before this procedure. It must
   be restored and visibly confirmed at the end.
2. In macOS System Settings, set Appearance to **Light** and confirm the desktop
   is visibly light.
3. Run this exact command in Terminal. It creates a fresh v008 root and does
   not delete, edit, or overwrite any earlier evidence.

   ```sh
   cd /Users/jaeger/Documents/GitHub/BetterPullReview
   set -o pipefail
   bpr_appearance_root=$(mktemp -d /tmp/bpr-appearance-foreground-v008.XXXXXX)
   bpr_appearance_log=$bpr_appearance_root/bpr-0114-foreground-v008.log
   printf 'BPR_FOREGROUND_ARTIFACT_ROOT:%s\n' "$bpr_appearance_root" \
     | tee "$bpr_appearance_log"
   ./build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
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
   Return to BetterPullReview, click the same left code pane, and leave
   BetterPullReview frontmost until it exits. The probe waits up to the
   five-minute run budget for the exact original focus widget.
6. Restore the desktop appearance noted in step 1 using System Settings, and
   visibly confirm the restoration.
7. Paste the complete contents of the exact path printed by
   `BPR_FOREGROUND_LOG`, including the artifact-root, process output,
   exit-status, artifact, and final log-path lines. Do not return a marker
   excerpt or summary. Then include the exact statement below.

Do not set `QT_QPA_PLATFORM=offscreen`, automate either click, automate the
global appearance change, edit the log, reuse an earlier packet's artifact
path, discard a failing first result, or run against rebuilt bytes under this
packet.

## Required raw-log audit

The complete log must first prove that appearance reached the real selected
diff composed by BPR-0227/BPR-0249:

- `NATIVE_APPEARANCE_SELECTED_DIFF_READY`
- `NATIVE_APPEARANCE_SELECTED_DIFF_REASON:none`
- `NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:111111`
- `NATIVE_APPEARANCE_SELECTION_WIRE_OK`
- `NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK`

The BPR-0410 attribution is then audited directly:

- The reachable wait is reported as
  `NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN:1`. Each live wait report is followed by
  `NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:<true|false>`,
  `NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:<true|false>`, and
  `NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:<true|false>`.
- A measurement after the strict return includes
  `NATIVE_APPEARANCE_OWNER_RETURNED`. The terminal report repeats each owner
  operand independently; a successful strict return reports all three as
  `true`.
- `NATIVE_APPEARANCE_INITIAL_CAPTURE:<disposition>` and
  `NATIVE_APPEARANCE_CHANGED_CAPTURE:<disposition>` distinguish
  `not_attempted_state_precondition`, `window_grab_null`,
  `normalization_failed`, and `captured`.
- `NATIVE_APPEARANCE_PERSISTENCE:<disposition>` distinguishes
  `not_attempted`, `initial_write_failed`, `changed_write_failed`,
  `persisted_readback_failed`, and `persisted`.
- The obsolete collapsed `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK` and
  `NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED` markers must not appear.

The current appearance stage has only one reachable wait episode: once the
strict predicate becomes true, that same event callback captures and transitions
to shutdown without polling the predicate again. Persistence and reporting
follow during finalization without another owner-return poll. The episode field
keeps the entry explicit; it is not evidence that a second episode was
exercised. An episode other than `1` is an unexpected result to return unchanged,
not something this packet predicts.

A fully successful observation has both capture dispositions `captured`,
persistence `persisted`, two artifact digest/byte records, and no
`BPR_FOREGROUND_ARTIFACT_MISSING`. A failure is still useful evidence only when
the complete raw log preserves the new owner, capture, persistence, budget, and
shutdown facts; do not infer screen-recording denial or occlusion from a Qt
capture disposition.

For a completed observation, the same complete log must retain the native
appearance and geometry facts:

- `NATIVE_APPEARANCE_READY:Light`
- `NATIVE_APPEARANCE_GLOBAL_READY:Light`
- `NATIVE_APPEARANCE_TRANSITION:Light->Dark`
- `NATIVE_APPEARANCE_GLOBAL_TRANSITION:Light->Dark`
- `NATIVE_APPEARANCE_SIGNAL_OK`
- `APPEARANCE_SYSTEM_FOLLOWS_OK`
- `APPEARANCE_EXPLICIT_IGNORES_OK`
- `APPEARANCE_VARIANTS:0:1:0:1`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:<n>`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_CAUSES:<n>:...`
- `PUBLIC_PATH_VERDICT:` with `shutdown_complete=1 cleanup_failed=0`
- `PUBLIC_PATH_BUDGET:expired=0 budget_ms=300000`
- one `BPR_FOREGROUND_ARTIFACT_ROOT:/tmp/bpr-appearance-foreground-v008.*`
- one `BPR_FOREGROUND_EXIT_STATUS:<n>`
- one matching `BPR_FOREGROUND_LOG:.../bpr-0114-foreground-v008.log`

`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` appears only when the Light-side
gate actually finds the Cocoa window inactive or without its focus widget. Its
absence is valid when the operator establishes focus before the first poll; the
two `READY:Light` markers above are the required accepted-baseline evidence.

The exit status and the exact `== 1U` geometry checks are observations, not the
human verdict. A nonzero exit or a rebuild delta other than one must be returned
unchanged; it must not be normalized, omitted, or silently rerun. Each geometry
cause remains `measured-change-mask/Qt-trigger-mask` in successful rebuild order.
Return those values verbatim; the human statement makes no correctness decision
about them.

## Operator response

Paste the complete v008 raw log, then include this exact statement:

> I ran the BPR-0114/BPR-0220/BPR-0410 selected-diff foreground appearance
> probe against executable
> sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28
> and BPR-0249 implementation record
> sha256:fe76c86329e9eb6650a9fa11103739fb0bfae0480a174b69689a74d057ff153b;
> I established the visible review.cpp comparison in Light, focused its left
> code pane, changed macOS Appearance from Light to Dark in System Settings,
> returned the same pane to the front until the process exited, restored and
> visibly confirmed my original desktop appearance, and the pasted v008 log is
> that run's complete unedited output.

If the procedure cannot be completed, return the complete raw log and identify
the exact obstruction instead. No partial log, visual-quality judgment, or
inferred geometry result closes any card.
