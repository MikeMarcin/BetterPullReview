---
schema: "implement-rfc.card.v1"
id: "BPR-0865"
title: "Give the repo-wide formatting gate a sweep-visible row, once every station is known to have clang-format 22"
status: "Ready"
rfcs: []
depends_on: ["BPR-0862"]
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005, deferred out of BPR-0862","found_at":"70a52630","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0865: Give the repo-wide formatting gate a sweep-visible row, once every station is known to have clang-format 22

## Why

[[BPR-0862]] found the repo-wide `coding-format-check` gate red on main for
seventeen files and, more importantly, found that **nothing in a full ctest
sweep would have noticed**. `coding-format-check` is a build target; the one
ctest row with a similar name, `coding_standard_format_gate_contract`, pins the
gate script's own behaviour rather than the project's formatting. A reader of a
green 381-row sweep can conclude the tree is formatted when it is not — and
that is how seventeen files drifted.

## Why BPR-0862 did not just add the row

The gate hard-fails without a supported tool. `_coding_standard.py`
(`MINIMUM_CLANG_FORMAT_VERSION = (22,)`) refuses when `clang-format` is absent,
unreadable, or older than 22, because releases before 22 render `{}` where this
tree holds `{ }` and would call hundreds of files dirty. Registering the
repo-wide check as a ctest row therefore turns **every station's sweep red for
a toolchain reason** on any machine whose `clang-format` is older or missing.

At the time BPR-0862 landed, only the macOS integrator's versions were known
(Homebrew 22.1.8 and 23.1.0, both of which report the tree clean). The Windows
and Linux stations' versions were not, and a sweep was about to be asked of
both. Making that gamble in the same pass would have cost a station round trip
to learn a version. So the row was deferred here, deliberately, rather than
left unrecorded.

## Scope

- Collect `clang-format --version` from `bpr-win` and `bpr-linux`. (The
  dsp-005 envelopes ask for it; the answers may already be on the board or in
  the station replies by the time this card is picked up.)
- If all three stations satisfy the minimum, register the repo-wide check as a
  ctest row next to `coding_standard_format_gate_contract` in
  `tests/CMakeLists.txt`, guarded so it is registered only when configure found
  a supported `clang-format` — the tree already conditions the coding targets
  on `BPR_ENABLE_CODING_TARGETS` and a centrally discovered
  `Python3::Interpreter`, so follow that pattern rather than inventing a new
  one. A station without the tool must **skip**, not fail: a missing tool is
  not formatting rot, and a row that conflates them is worse than no row.
- If some station cannot have clang-format 22, do not add the row. Instead make
  the gate part of the integrator's own pre-merge pass and say so where a
  sweep's greenness is interpreted, so nobody reads a green sweep as a
  formatted tree.

## Acceptance

- Either a sweep row exists that goes red when a tracked file is unformatted
  and is skipped (not failed) where no supported `clang-format` exists, and it
  is shown to do both; or the card records, with the actual station versions,
  why the row cannot exist and where the gate runs instead.
- Both stations' `clang-format` versions are recorded here either way.

## Station versions, collected 2026-09-17 (dsp-005)

Two of the three are known, and they settle the question against a naive row.
`bpr-linux`'s numbers were collected by that station (`bpr-linux/int-12`) and
re-verified there; macOS is the integrator's own.

| Station | `clang-format` | Python |
|---|---|---|
| `bpr-main-mac` (integrator, macOS) | 23.1.0, and 22.1.8 also installed; both report the tree clean at `754d0538` | 3.14 |
| `bpr-linux` (JaegAW, WSL2, Ubuntu 26.04) | **21.1.8 on the default `PATH`**; 22.1.8 only at `/usr/bin/clang-format` | 3.13.15 on `PATH`, 3.14.4 at `/usr/bin/python3` |
| `bpr-win` | not yet collected | not yet collected |

The Linux station's exact strings, because the shadowing is the whole point:

| Resolution | Version string | Meets the `(22,)` minimum |
|---|---|---|
| `clang-format` (PATH) → `/home/mmarcin/.local/bin/clang-format` | `Ubuntu clang-format version 21.1.8 (6ubuntu1)` | **No** |
| `/usr/bin/clang-format` | `Ubuntu clang-format version 22.1.8 (++20260714015917+ca7933e47d3a-1~exp1~20260714135927.17)` | Yes |

**This is the answer the deferral was waiting for, and it is concrete rather
than hypothetical.** A ctest row that resolved `clang-format` from `PATH` and
gated on version 22 would have gone red on `bpr-linux` **today** — for a
shadowed toolchain, not for formatting rot, which is exactly the outcome this
card was opened to avoid. That station's own gates have had to say
`/usr/bin/clang-format` explicitly since the shadowing was found, including
[[BPR-0855]]'s and [[BPR-0856]]'s runs, which is why its formatting verdicts
have been trustworthy while a naive row would not have been.

**Integrator's decision on these numbers (dsp-005).** They settle this card
against adding the row as first imagined. If the row is ever added it has two
hard requirements rather than one:

- **Resolve the tool explicitly**, the way that station's gates do and the way
  CMake's own discovery would, rather than trusting `PATH`. A row that picks
  up whichever `clang-format` is first on `PATH` is not reproducible across
  these three machines.
- **Skip, not fail**, when no supported tool is found, so a missing or old
  tool never reads as unformatted source.

Windows's version is the last input. It is asked for in the next `bpr-win`
envelope.

**Not run on the Linux station.** That task was runs, not fixes: the repo-wide
`coding-format-check` gate was not executed there and no registration was
added or changed. Only the versions above were collected.
