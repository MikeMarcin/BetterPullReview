# BPR-0114 foreground appearance probe operator review

Status: **stale; do not run or approve. The bound binary changed and the
BPR-0220 rebuild-cause evidence is not yet present.**

This packet is preserved as the superseded v001 procedure. A current packet
must bind the rebuilt binary and updated BPR-0114 bytes, and its log must carry
per-rebuild cause attribution rather than only the aggregate delta. No response
to this version can satisfy BPR-0114 or BPR-0220.

This packet presents the one bounded human action [[BPR-0114]] still needs. It
does not approve the appearance palettes, does not judge presentation quality,
and does not close RFC-BPR-0006.

## Why a person is needed, and for exactly one thing

Both halves of this card's claim are already proven, unattended, on a real
display. The product owner's standing authorisation permits the scheme change
to be scripted, and it was:

```
NATIVE_APPEARANCE_TRANSITION:Light->Dark
APPEARANCE_SYSTEM_FOLLOWS_OK
APPEARANCE_EXPLICIT_IGNORES_OK
APPEARANCE_VARIANTS:0:1:0:1
NATIVE_APPEARANCE_SIGNAL_OK
```

What the standing authorisation does not cover is **focus**. The probe requires
its window to be active, focused and frontmost, and says so in the marker's
name. A process launched from a background shell never satisfies it, and
raising it with `System Events` produced `APPEARANCE_VARIANTS:-1:-1:-1:-1` — no
baseline captured at all.

That gate is deliberate. An appearance judgement made against a window that was
never frontmost is a judgement about a window nobody was looking at.

Eight probe terms currently fail, and the card's prediction is that they share
one root — `StatePassed`, which is gated by the window being active:

```
NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED
NATIVE_APPEARANCE_STATE_FAILED
NATIVE_APPEARANCE_PIXELS_DIFFER_FAILED
NATIVE_APPEARANCE_DIFF_PALETTE_INHERITED_FAILED
NATIVE_APPEARANCE_DIFF_FONT_INHERITED_FAILED
NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_FAILED   (delta 2)
NATIVE_APPEARANCE_DIFF_SELECTION_FAILED
NATIVE_APPEARANCE_CHANGED_RENDER_FAILED
```

This run tests that prediction. One of the eight is expected **not** to resolve:
`DIFF_GEOMETRY_REBUILD` is a product question tracked as [[BPR-0220]], not a
focus artefact, and it should still be red afterwards. If it goes green, that is
itself informative and must be recorded rather than welcomed.

## What to do

Set the desktop to **Light** first — the probe's transition is Light to Dark.

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
./build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --native-appearance-probe \
  --render-evidence /tmp/bpr-appearance-before.png \
  --changed-render-evidence /tmp/bpr-appearance-after.png \
  --automated-run-budget-ms 30000 \
  2>&1 | tee /tmp/bpr-0114-foreground.log
```

**Click the application window to bring it to the front as soon as it appears,
and leave it frontmost.** That is the entire human contribution. The scheme
change is scripted from inside the run under the standing authorisation, and
the desktop is restored afterwards.

Do not run this with `QT_QPA_PLATFORM=offscreen`; the probe refuses offscreen
at argument parsing, which is correct.

## Evidence bound by this packet

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review` | `sha256:0da20d0070843e52953afcdd70fabc868bdd5a35ff565786aa09affa825f0e08` | 38979856 |
| `docs/issues/backlog/BPR-0114-rfc-0006-system-scheme-toggle.md` | `sha256:7f82b454d181dfbd211b7ab7c2d575c4bd11bc99e7c973ec8d7c5e6c3a533a79` | 18344 |

A rebuild changes the binary digest and reopens this gate.

## Operator response

Paste back the complete contents of `/tmp/bpr-0114-foreground.log`. The log is
the verdict; there is nothing for you to assess by eye.

Then, to approve, reply with this exact statement:

> I ran the BPR-0114 foreground appearance probe against binary
> sha256:0da20d0070843e52953afcdd70fabc868bdd5a35ff565786aa09affa825f0e08 with the window frontmost, and the pasted
> log is that run's complete output.

To reject, say what prevented the run. Rejection leaves BPR-0114 in `backlog`
and closes nothing.

This response applies only to the foreground probe run. The per-term audit of
whatever remains red is separate work, one term per commit, and
`DIFF_GEOMETRY_REBUILD` belongs to BPR-0220 either way.

## Addendum, 2026-08-09: this run also answers BPR-0220

[[BPR-0220]] asks whether an appearance change rebuilding the comparison
geometry twice is a double application or two legitimate causes. It was
measured offscreen and narrowed but not answered:

| Run | Rebuilds |
|---|---|
| diff-workbench probe, offscreen | 4 |
| the same with an appearance change | 5 |

Offscreen the change costs **one** rebuild. The probe reports **two**. So the
second rebuild is either display-dependent, or specific to the scheme change
rather than a profile capture — and this run is the only configuration that can
tell those apart.

Nothing extra is asked of you. The delta is already in the marker output the
run produces; pasting the complete log is what records it.

**This does not change the decision requested above.** Approving the foreground
run approves that the run happened as described. It does not approve any
conclusion about the geometry delta, which is BPR-0220's to draw from the log.
