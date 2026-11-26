---
schema: "implement-rfc.card.v1"
id: "BPR-2421"
title: "Install documentation says GitHub CLI is offered on all three platforms and full diffs through it are macOS-only"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"docs","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"157f6dd07","origin":"owner, 2026-11-26: approved fixing the contradictory GitHub CLI platform statements noticed in passing on BPR-2420","reservation":"docs/issues/_reservations/BPR-2421--BPR-2421--716a2e7dc017.json","related":["BPR-1791","BPR-2420"],"acceptance":["BPR-2421:packages","BPR-2421:readme","BPR-2421:inventory","BPR-2421:docs"]}
---

# BPR-2421: GitHub CLI platform statements agree

## Why

`docs/development/release-packages.md` said "optional GitHub CLI support is
macOS-only", while README said `gh`'s file writes on Windows and Linux are not
confined (implying it is offered there). The product says otherwise:
`src/app/src/tour_cli.cpp` reports `gh_transport` as "explicit opt-in" on
macOS and "explicit opt-in; gh file writes not denied" on Windows and Linux,
and `docs/architecture/security-threat-model.md` ([[BPR-1791]]) records that
gh's Git-object acquisition and exact blob reads remain macOS-only, so full
diffs through GitHub CLI are unavailable on Windows and Linux and the app says
so, and that Linux `gh` needs the session bus socket in `XDG_RUNTIME_DIR`.
README omitted the full-diff half.

## Change

- `docs/development/release-packages.md`: "Install Git 2.39+ yourself.
  Optional GitHub CLI access is offered on macOS, Windows and Linux and needs
  `gh` 2.96+ and Git 2.49+. On Windows and Linux, `gh`'s file writes are not
  confined as they are on macOS, and full diffs through GitHub CLI are not
  available; the app's own sign-in gives full diffs on every platform. On
  Linux, `gh` also needs the session bus socket in `XDG_RUNTIME_DIR`."
- `README.md` Install: "On Windows and Linux, `gh`'s file writes are not
  confined as they are on macOS, and full diffs through GitHub CLI are not
  available; the app's own sign-in gives full diffs everywhere."

## Acceptance

- **BPR-2421:packages** The package guide says GitHub CLI is offered on all
  three platforms, gives its version floors, and states the Windows/Linux
  write-confinement and full-diff differences and the Linux session-bus need.
- **BPR-2421:readme** README's Install section states both Windows/Linux
  differences and names no `X.Y.Z` version.
- **BPR-2421:inventory** No other current document claims GitHub CLI is
  macOS-only or confined everywhere.
- **BPR-2421:docs** Both documentation gates pass.

## Verification

macOS arm64. Evidence revision `157f6dd07`.

| Criterion | Check | Result |
|---|---|---|
| BPR-2421:packages | `release-packages.md` lines 19-24 read after the change, against `tour_cli.cpp` `gh_transport` and `security-threat-model.md` lines 419-431 | pass |
| BPR-2421:readme | README lines 44-47 read after the change; the BPR-2420 Install check passes (`2.96+`, `2.49+`, `2.39+` are `X.Y`) | pass |
| BPR-2421:inventory | `grep -rniE` for GitHub CLI/`gh` near macOS-only, confined or sandbox over README, CONTRIBUTING, `docs/`, `packaging/`. Remaining hits are accurate or historical: `settings.md:74` ("macOS, Windows and Linux"), `RFC-BPR-0015` line 45 and the threat model (gh reads macOS-only), BPR-1791's card, `product-contract.md` "GitHub CLI profile milestone" (the macOS milestone's record, not a platform claim), and `docs/verification/RFC-BPR-0047-0051.md:2633` (a historical record about the macOS-only confirmation-window code) | pass |
| BPR-2421:docs | `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`; `python3 tests/scripts/verify_documentation_tests.py`: 88 tests OK (worktree temporarily linked to the deployed implement-rfc skill, link removed after) | pass |

## Limits

- BPR-2420's card still carries its "noticed in passing" note about this
  contradiction; it is a historical Limits entry and was left as found.
