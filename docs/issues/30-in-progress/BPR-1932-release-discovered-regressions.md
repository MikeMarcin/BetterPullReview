---
schema: "implement-rfc.card.v1"
id: "BPR-1932"
title: "Land release-discovered portability and presentation regressions on main"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"v0.4.2 native qualification","mandate":"Owner 2026-11-07: an implementer owns crash/fix implementation and lands it on main before release qualification resumes."}
---

# BPR-1932: Release-discovered regressions

## Outcome and acceptance

- AC1. Platform-specific declarations and fixture capture constants remain with
  the translation units that consume them after the source splits. Retain the
  native Windows Release/Debug compiler and clone-path evidence.
- AC2. Overview completion is announced once after the visible presentation
  handoff; superseded or still-hidden completions stay silent. Reconcile
  focused courses with already accepted header, setup, Tour wording and skill
  behavior without weakening their negative controls.
- AC3. Diagnose and prevent the Tour publication geometry recursion seen in the
  composed manual publication course. Exercise a focused regression and the
  public composition, retaining the original intermittent crash evidence.
- AC4. Format and documentation gates pass; fixes land on authoritative main.

## Bounded milestone

One engineering milestone owns AC1–AC4 and integration. It does not accept
native full-release qualification, signing, security or publication. Source
baseline is `160427576`; prior release-only fixes are reviewed before adoption.

## Retained failure

The v0.4.2 build-34 investigation's manual Tour publication course crashed with
SIGSEGV and a macOS stack-overflow report: repeated `QScrollArea::eventFilter`
and `QWidget::resize`. A registered repeat reproduced it after nine passes.
External evidence is under the release run's `candidate34` directory, not in
this repository. The coordinator owns release artifacts and final qualification.
