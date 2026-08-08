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
