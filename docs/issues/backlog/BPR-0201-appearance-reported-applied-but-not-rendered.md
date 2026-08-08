---
id: BPR-0201
title: The app reports a dark profile applied while rendering light pixels
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# The app reports a dark profile applied while rendering light pixels

Observed 2026-08-07 at commit `6f0a79d` while producing captures for the
selection-shade batch. The system appearance was Dark.

`--diff-workbench-probe` emitted:

```
APPEARANCE_SETTINGS:dark:4
APPEARANCE_APPLIED:#193151:#0d1117:from-profile
```

`#193151` and `#0D1117` are the Dark profile's `code_selection_background` and
`code_background`. So the profile resolved dark and reported itself applied.

Every rendered pixel was light. A histogram of `14-replacement-late.png` and of
the `--render-evidence` window capture returns `#FFFFFF` and `#F6F7F9`
backgrounds with `#E6FFEC` / `#FFEBE9` / `#E7EEF9` bands — the **Light**
profile's values, exactly.

## What this is not yet

**Not a confirmed defect.** Three explanations fit the observation and they
have not been separated:

1. The probe builds a standalone `diff_workbench` with a default colour set
   rather than the composed one, so its frames were never going to follow the
   profile and the marker is describing a different widget.
2. `--render-evidence` captures the window before the appearance is applied, so
   the marker is true and the capture is merely early.
3. The profile genuinely does not reach the panes, and the marker reports
   intent rather than effect.

Only 3 is a user-visible bug, and it is the one that would matter most —
`APPEARANCE_APPLIED` would be attesting to something that did not happen, which
is the same class of error [[BPR-0194]] documents for the palette contrast
work: a check that reads the object it was supposed to verify was applied.

## Why it matters beyond itself

The attended-review batches depend on captures being evidence of what a user
sees. If a capture can carry the Light palette while the application reports
Dark, then **no capture is trustworthy without checking its pixels against the
profile it claims** — and the dark-scheme half of [[BPR-0199]] cannot be
reviewed at all until this is resolved.

That is why the selection-shade batch is Light-only and says so.

## Narrowed by instrumentation, 2026-08-07 — explanation 3 is eliminated

The profile **does** reach the panes. A temporary print inside
`apply_appearance` and `diff_workbench::adopt_appearance` shows exactly two
adoptions in a probe run:

```
DBG adopt added=#e6ffec      <- light, at composition
DBG settle start
DBG first capture
DBG adopt added=#0f2f1c      <- dark, after every capture
APPEARANCE_APPLIED:#193151:#0d1117:from-profile
```

`#0f2f1c` is the dark profile's `diff_added_background`, so the workbench
receives it. Nothing is broken in the appearance path, and explanation 3 — the
only user-visible one — is **eliminated**. This is explanation 2: the capture is
early. The marker reports end-of-run state; the rasters are mid-run state.

### The reason it cannot simply be waited out

Three attempts, all ineffective, all reverted:

1. Drain until the window palette stops moving. Exits after two turns, because
   the palette settles instantly at whatever the profile currently resolves.
   Draining until *that* is stable finishes long before the observation and
   proves nothing.
2. Drain while `QStyleHints::colorScheme()` is `Unknown`. Exits immediately —
   the platform already reports Dark by then. It is the *signal* that has not
   been delivered, not the value that is missing.
3. Drain unconditionally, 64 turns. Still light in all nineteen frames.

So the dark adoption is not reachable from inside the probe by turning the event
loop. It arrives after the probe's whole lifetime, on a path this investigation
has not identified. `colorSchemeChanged` is the only connected trigger, so
either that signal is delivered very late under the offscreen platform, or
something outside the probe re-observes at shutdown.

### What this changes

Captures from `--diff-workbench-probe` are evidence of the profile in effect
**at composition**, not of the one the run ends on, and the two differ whenever
the observation lands late. That is a property of the harness rather than of
the product, but it makes every dark-scheme capture unusable until it is fixed,
because the frames will show light while the markers say dark.

It also means the light-scheme captures used throughout BPR-0194, BPR-0202 and
BPR-0206 are sound: they were taken under the light profile and reported the
light profile, and the two agree.

### The late trigger, found — and it is not the operating system

Instrumenting the observation and the mode together:

```
DBG construct scheme=0 colorScheme=0     <- Unknown, at composition
DBG before adopt_settings mode=0         <- system
DBG apply mode=0 added=#e6ffec           <- resolves light
DBG composition complete
DBG probe begin colorScheme=0            <- still Unknown
DBG apply mode=2 added=#0f2f1c           <- explicit dark, after every capture
APPEARANCE_APPLIED:#193151:#0d1117:from-profile
```

`observe_native_appearance` **never fires**, and `QStyleHints::colorScheme()`
reports `Unknown` from start to finish. The operating system is not involved at
all. What changes is the *mode*: `0` (system) at composition, `2` (explicit
dark) afterwards.

The source is `main.cpp`, in the appearance-**gesture** assertion:

```cpp
apply_appearance_gesture(aApplication, bpr::core::appearance_mode::dark, 8U);
```

That assertion deliberately dispatches an explicit dark mode to prove a gesture
reaches presentation, and it runs later in the same process than the diff
probe's captures. So the run ends dark, the marker reports dark, and every
raster was written while the run was still light.

**There is no defect.** All three explanations were wrong, including the two
this card called plausible: the profile reaches the panes, the capture is not
merely early in the sense of racing a signal, and nothing is misreported. Two
assertions share one process and one of them changes the appearance the other
is capturing under. The marker describes the end of the run; the frames
describe the middle.

### Resolved by making a capture describe itself

`--diff-interaction-evidence` now writes `appearance-at-capture.json` beside
the frames, holding the window's base, highlight and text colours as they were
when the rasters were taken. A reader no longer has to reconcile a capture
against a marker written at a different moment:

```json
{"base":"#ffffff","highlight":"#7fb0e8","window_text":"#1f2328"}
```

next to an end-of-run `APPEARANCE_APPLIED:#193151:#0d1117`. The disagreement is
now visible in the evidence rather than being a discovery.

The stamp is held to the pixels: the probe grabs the left pane and fails unless
the commonest colour in it is the palette's base. A stamp that described an
appearance the panes were not painting would fail the run rather than be
believed. Shown non-vacuous by inverting the comparison, which fails
`application_diff_workbench_public_path` with a clean build.

### What is still true

Dark-scheme captures remain unavailable from this probe, but for a mundane
reason rather than a mysterious one: it runs under whatever mode composition
resolved, and on this host that is light. Obtaining dark rasters means running
the probe with an explicit dark mode selected before it captures, which is new
work rather than a fix. [[BPR-0199]]'s dark half stays blocked on that.

## Required direct evidence

- Which of the three explanations holds, established by inspection rather than
  inferred from the marker.
- If the marker can report applied while the panes carry another profile, an
  assertion that fails in exactly that case. Marker plus histogram, not marker
  alone.
- The assertion is shown to fail when the profile is prevented from reaching
  the panes.

## Do not

Do not fix this by making the marker quieter. If the probe's workbench is a
different widget from the composed one, say so in the marker name; the problem
is that a reader cannot tell, not that the marker exists.
