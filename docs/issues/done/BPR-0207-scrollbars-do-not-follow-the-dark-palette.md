---
id: BPR-0207
title: The comparison scrollbars stay light in the dark scheme
status: Done
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

## Cause, 2026-08-07 — an incomplete palette, not a style override

The open question was whether the platform style was drawing scrollbars from
the system theme rather than the widget palette, because that decides whether a
palette edit can help at all. It is not.

Two measurements settled it. The active style is **`QFusionStyle`**, not
`QMacStyle` — and the scrollbar was `#F6F6F6` under the dark profile against
`#F9F9F9` under the light one. Not identical, so the palette *was* reaching it;
it was reading a role nothing had set.

`appearance_profile::derive` assigned seven roles — `Window`, `WindowText`,
`Base`, `AlternateBase`, `Text`, `Highlight`, `HighlightedText` — and left the
rest at whatever the inherited palette held, which under a dark profile is a
light default. Qt's styles draw scrollbars, buttons, frames and separators from
`Button` and from the shade family, and **`setColor(Button, …)` does not
recompute `Light`, `Midlight`, `Mid`, `Dark` or `Shadow`**. So every one of
them stayed light.

## Fixed

`derive` now also owns `Button`, `ButtonText`, `ToolTipBase`, `ToolTipText`,
`PlaceholderText` and the shade family, all derived from
`application_surface` using Qt's own shade factors. Keeping those factors
matters: a style that expects `Light` to be lighter than `Button` still gets
that in a dark scheme, where a hand-picked set would have to re-establish the
ordering by hand and could silently fail to.

Dark comparison scrollbar: `#F6F6F6` before, `#282B32` after.

Nothing is hard-coded per widget, which is what this card warned against. The
scrollbar was only the visible symptom; every control Qt draws from the chrome
family had the same latent defect, and all of them are fixed by the same
change.

## Verification

`qt_appearance_profile` asserts, against a deliberately dark profile, that
`Button` and `ToolTipBase` carry the application surface, that the shade family
stays ordered light through dark, and that the shades follow a dark surface
rather than an inherited light one.

Shown non-vacuous by two separate breaks, each with a clean build: dropping the
`Button` assignment fails the chrome assertion, and dropping one shade fails the
ordering assertion.

The test that previously used `Button` as an example of *an unowned role* now
uses `Link`, which the diff surface genuinely never draws. That assertion was
not deleted — the principle it protects, that the profile must not clobber roles
it has no opinion about, still holds; `Button` simply stopped being an honest
example of one.

## Superseded — required direct evidence

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
