---
id: BPR-0184
title: "Qt: the appearance settings gesture"
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# Qt: the appearance settings gesture

RFC-BPR-0006 puts "translates settings gestures" among the Qt adapter's
responsibilities. There is no gesture. `appearance_profile::select_mode` is
reachable only from tests, so no person can choose an appearance mode, a font,
or a tab width.

## What this slice does

A surface that lets a reviewer choose appearance mode, UI and code font family,
size, weight, tab width, ligatures, bold, and italic, and that translates each
gesture into an intent the appearance host adopts. The RFC requires each to
"persist and preview immediately", which is two claims: the surface shows the
change without a restart, and the change is still there after one.

Code line height is deliberately absent — BPR-0118 retires it on the
product owner's decision, because `QPlainTextDocumentLayout` ignores every
line-height mode Qt offers.

## What must not be done

Do not let the surface hold presentation state of its own. It translates a
gesture and shows what the host published; a control that remembers its own
value will disagree with the host the first time a change is refused, and will
show the reviewer a setting that is not in effect.

Do not clamp a rejected value into range. The core refuses a typography record
whole and returns typed reasons; the surface reports the reason. A silently
clamped value is a setting the reviewer believes they chose.

## Required direct evidence

- Every listed setting is reachable, changes the composed presentation without
  a restart, and survives one.
- A value the core rejects is refused whole, the typed reason is shown, and the
  previous value stays in effect — asserted on the composed surface, not on the
  intent.
- Every gesture translates to an intent; no widget path mutates presentation
  directly.
- The mode control offers all six modes in `core::appearance_mode`, and the
  test reads the vocabulary from the core rather than restating a list that can
  drift.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

Unblocks BPR-0114's explicit-mode half, which needs a way to select an explicit
mode.
