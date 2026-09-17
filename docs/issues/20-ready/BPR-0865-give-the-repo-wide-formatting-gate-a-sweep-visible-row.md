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

## `bpr-linux`'s two `clang-format` versions (dsp-005, bpr-linux/int-12)

Collected on station `bpr-linux` (JaegAW, WSL2, Ubuntu 26.04 LTS). This
station has **two** `clang-format` installations, and which one answers
depends entirely on `PATH`:

| Resolution | Version string | Meets the `(22,)` minimum |
|---|---|---|
| `clang-format` (PATH) | `Ubuntu clang-format version 21.1.8 (6ubuntu1)` | **No** |
| `/usr/bin/clang-format` | `Ubuntu clang-format version 22.1.8 (++20260714015917+ca7933e47d3a-1~exp1~20260714135927.17)` | Yes |

`which -a clang-format` resolves first to `/home/mmarcin/.local/bin/clang-format`,
so the 21.1.8 is what a plain `clang-format` invocation gets here; the
supported 22.1.8 is reachable only by absolute path. This is the station's
default `PATH`, not a contrived one — [[BPR-0855]]'s and [[BPR-0856]]'s gate
runs on this station already had to say `/usr/bin/clang-format` explicitly for
that reason.

**So a sweep row that gated on `clang-format` 22 through `PATH` would go red
on this station today**, for a toolchain reason and not for formatting rot —
precisely the outcome this card was opened to avoid. That is a concrete
instance of the risk the card describes, on a real station, rather than a
hypothetical one.

**Integrator's decision on these numbers (dsp-005).** The two versions settle
this card against adding the row. If such a row is ever added it must resolve
the tool explicitly rather than trusting `PATH`, and must **skip** rather than
fail when no supported tool is found. Windows's `clang-format` version is the
last unknown input.

macOS was already recorded on this card (Homebrew 22.1.8 and 23.1.0, both
reporting the tree clean), so `bpr-win` is the only station whose version is
still missing.

**Not run here.** This task is runs, not fixes: the repo-wide
`coding-format-check` gate was not executed and no registration was added or
changed. Only the versions above were collected.
