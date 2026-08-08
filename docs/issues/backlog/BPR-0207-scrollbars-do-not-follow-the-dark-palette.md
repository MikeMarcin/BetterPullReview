---
id: BPR-0207
title: The comparison scrollbars stay light in the dark scheme
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# The comparison scrollbars stay light in the dark scheme

Found the moment dark rasters became obtainable, in the first one taken.

Both comparison panes' horizontal scrollbars render as a bright white trough
with white arrow buttons, against a `#0D1117` pane and `#1C2026` chrome. They
are the brightest thing on the surface by a wide margin, sitting directly under
the code they belong to.

Evidence: `--diff-workbench-probe --diff-evidence-appearance dark`, frame
`11-deletion-early.png`, bottom strip of both panes.

## Why it was not seen before

Not because nobody looked. Until [[BPR-0201]] the probe could only produce light
captures, so every raster reviewed for [[BPR-0194]], [[BPR-0202]] and
[[BPR-0206]] was a light one, where a white scrollbar is unremarkable. This is
the first dark frame the project has, and the defect is in it.

That is worth recording as a pattern rather than an anecdote: a whole scheme
had no rendered evidence at all, and the gap was invisible because the light
evidence was plentiful and good.

## What is probably wrong

`appearance_profile::derive` assigns `QPalette::Window`, `Base`, `Text`,
`Highlight` and their neighbours, but a `QScrollBar` under `QMacStyle` draws its
trough and handle from roles the derivation may not set — or draws them from the
style rather than the palette entirely, the way the platform decides scrollbar
appearance from the system theme rather than from the widget's palette.

If it is the latter, the palette cannot fix it and the scrollbars need styling
or a `QStyle` that honours the requested scheme. Establish which before
choosing, because those two have very different costs and only one is a palette
edit.

## Required direct evidence

- A dark capture in which the scrollbar trough and handle are drawn from the
  dark palette, measured from pixels rather than from the palette object.
- The same check in the light scheme, so a fix cannot be a hard-coded dark
  scrollbar that then breaks the other three profiles.
- Whether the platform style is overriding the palette, established by
  inspection and recorded, since it decides the shape of the repair.
- The assertion fails when the scrollbar is left on the platform default.

## Do not

Do not fix this with a stylesheet on the scrollbar alone. Every other widget
that takes its colours from the platform rather than the profile has the same
latent problem, and a fix that names one widget leaves the rest and tells the
next reader the palette is in charge when it is not — the same trap
[[BPR-0194]] records for the selection colour.
