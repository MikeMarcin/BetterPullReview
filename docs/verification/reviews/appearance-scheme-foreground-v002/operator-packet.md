# BPR-0114/BPR-0220 foreground appearance probe operator packet

Status: **pending operator run**

This packet requests one bounded human action: bring the application window to
the front and leave it there while the already-scripted Light → Dark transition
runs. It does not approve palette quality, does not decide whether one or two
geometry rebuilds are correct, and does not close BPR-0114, BPR-0220, or
RFC-BPR-0006.

## Why a person is required

The native probe deliberately requires the application to be active, focused,
and frontmost. The scheme transition and restoration are automated under the
existing standing authorization; focus is the only human contribution.
Offscreen execution cannot satisfy this gate and cannot reproduce the
display-dependent second rebuild under investigation.

## Bound evidence

Run only the artifacts with these exact digests:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review` | `sha256:d65c4f8d7f5f98050d6e8eb03213d0f4785643fb812078ab70634e506a0bdf8a` | 44777216 |
| `docs/issues/backlog/BPR-0114-rfc-0006-system-scheme-toggle.md` | `sha256:158fd5806d9b05ce1a83f36998414a316bd499501c6615468a4b41f65772e4d1` | 19954 |
| `docs/issues/backlog/BPR-0220-an-appearance-change-rebuilds-comparison-geometry-twice.md` | `sha256:f1ffc22c0c54b0b781f01d56465c5847255a1c107bcc0e79df41e980d652cbeb` | 8101 |

The repository commit at packet preparation was
`5bcc6886f3e5683709b577abfd92b7b838396430`. Rebuilding or changing either
card invalidates this packet.

## Operator procedure

1. Set the macOS desktop appearance to **Light**.
2. Run this exact command in Terminal:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
set -o pipefail
./build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --native-appearance-probe \
  --render-evidence /tmp/bpr-appearance-before-v002.png \
  --changed-render-evidence /tmp/bpr-appearance-after-v002.png \
  --automated-run-budget-ms 30000 \
  2>&1 | tee /tmp/bpr-0114-foreground-v002.log
```

3. As soon as the application window appears, click it to bring it frontmost.
   Leave it frontmost until the process exits and the desktop appearance is
   restored.
4. Return the **complete** contents of
   `/tmp/bpr-0114-foreground-v002.log`, including every line before and after
   the geometry markers. A nonzero command result does not invalidate the run;
   the complete marker sequence is the evidence being requested.

Do not set `QT_QPA_PLATFORM=offscreen`, automate the click, edit the log, or
rerun against a rebuilt binary under this packet.

## Required log audit

A usable run must include all of these facts:

- `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`
- `NATIVE_APPEARANCE_TRANSITION:Light->Dark`
- `NATIVE_APPEARANCE_SIGNAL_OK`
- `APPEARANCE_SYSTEM_FOLLOWS_OK`
- `APPEARANCE_EXPLICIT_IGNORES_OK`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:<n>`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_CAUSES:<n>:...`
- `NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_OK`
- normal shutdown markers

The process exit status is not an attribution verdict. In particular, the
current exact `== 1U` checks remain unchanged. A delta of two may therefore
leave `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_FAILED` red while still
providing the complete cause evidence needed by BPR-0220.

Each cause entry is `measured-change-mask/Qt-trigger-mask`, in successful
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

The returned order and masks decide the next product change. They must not be
interpreted or normalized by the operator.

## Operator response

Paste the complete log, then include this exact statement:

> I ran the BPR-0114/BPR-0220 foreground appearance probe against binary
> sha256:d65c4f8d7f5f98050d6e8eb03213d0f4785643fb812078ab70634e506a0bdf8a,
> with BPR-0114 card
> sha256:158fd5806d9b05ce1a83f36998414a316bd499501c6615468a4b41f65772e4d1
> and BPR-0220 card
> sha256:f1ffc22c0c54b0b781f01d56465c5847255a1c107bcc0e79df41e980d652cbeb,
> with the application window frontmost, and the pasted log is that run's
> complete output.

If the procedure cannot be completed, report the exact obstruction instead.
No partial response or visual judgement closes either card.
