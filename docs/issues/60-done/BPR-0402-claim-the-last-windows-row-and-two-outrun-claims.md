---
id: BPR-0402
title: Re-check the last Windows row, and two claims the board outran
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# Re-check the last Windows row, and two claims the board outran

[[BPR-0347]] and [[BPR-0358]] closed 2026-08-19 by re-reading their shared
failure taxonomy against the commits that landed after it, and five of thirteen
rows did not survive. That pass left exactly one row unchecked and said so:

> `dif_assets_contract` is the only row not re-checked here; [[BPR-0350]] and
> [[BPR-0378]] hold its two halves.

This card checks it, and picks up two further statements that the same method
finds false — both of them written by the board about itself, and both
falsified by the board's own later commits.

## 1. The last row: `dif_assets_contract`

**Measured here, on macOS, at `0c4b3b7`:**

| probe | result |
|---|---|
| `ctest --test-dir build/core-only -R '^dif_assets_contract$'` | 1/1 pass, 27.34 s |
| `python3 tests/scripts/dif_asset_tests.py` | 35 tests, OK |
| `python3 -c "import zlib; print(zlib.ZLIB_RUNTIME_VERSION)"` | `1.2.12` |
| `scripts/dif_assets.py` import list | `tempfile` only; no `contained_temporary_support` |

**And it does not discharge the row.** This is the point worth recording,
because a green result is the easiest kind of evidence to over-read.

The row has two halves and this host defeats neither of them:

- [[BPR-0378]]'s half is a PNG canon that depends on the local zlib. Its own
  text already records that the failure "does not reproduce on this host, whose
  interpreter links stock zlib 1.2.12". This interpreter still links stock
  zlib 1.2.12, so the probe re-confirms the *precondition of the exemption*
  rather than testing the claim. A zlib-ng-linked interpreter is the case that
  matters and this host does not have one.
- [[BPR-0350]]'s half is a temporary root inside the Dif tool's import
  boundary. That conversion was reverted at `acb36e5` and the revert is still
  in place at HEAD, so the code path that failed is not present to fail.

So the row moves from *never re-checked* to **measured, and still unmeasured
for Windows** — a distinct and weaker status than the five rows that were
retired. Both halves are correctly attributed and neither is diagnosable
further from macOS.

The five retired rows were retired because a later commit removed their cause.
Nothing removed this one's cause; this host simply cannot reach it.

## 2. [[BPR-0358]]'s format-gate line is stale

BPR-0358's "Gates" block states:

> `cmake --build --target coding-format-check`  FAILS, pre-existing, [[BPR-0367]]

and attributes it to one file, `src/app/src/github_application.cpp`, with the
open question of "whether the file is misformatted or the tool version
disagrees".

That question was answered and the gate was repaired. [[BPR-0367]] is in
`60-done/`, and its fix landed at `14a654d`, *"style(app): restore the format
gate at HEAD"*. Measured directly at HEAD with the same tool the gate uses:

```
clang-format --version                                       22.1.8 (Homebrew)
clang-format --dry-run --Werror -style=file \
  src/app/src/github_application.cpp                         clean, exit 0
```

The gate does not fail. BPR-0358's block is a snapshot that outlived its
subject, exactly like the taxonomy rows it corrected two sections later in the
same card.

## 3. The board README states a rule its own next three commits broke

`docs/issues/README.md` says, in bold, as a considered conclusion rather than
an aside:

> **No unattended work on this board is executable from a macOS session today.**

It is false, and the counterexample is in the same day's log. The sentence was
written at `7dd4641`, 2026-08-19 21:30:10. The next three commits —
`365c662`, `e07183d`, and `0c4b3b7` at 21:44:38 — are all unattended macOS work
on this board, and they are the commits that retired five taxonomy rows. This
card and [[BPR-0256]]'s gate run are two more.

The sentence's reasoning was sound for one class of work and then
over-generalized. Target-native execution is genuinely unavailable here; that
does not make the board's *claims* unavailable to check, and re-reading a
recorded claim against later commits is unattended work that any host can do.
It is also, on the current evidence, the highest-yield work available from a
portable host: it has retired five taxonomy rows and three board claims in two
sessions without a single Windows run.

The replacement has to be narrower than the original and narrower than its
opposite. Both are recorded below.

## Required final state

- BPR-0347's and BPR-0358's `dif_assets_contract` rows state the measured
  result and the reason it does not discharge, rather than reading as
  never-checked.
- BPR-0358's "Gates" block no longer asserts a failing format gate.
- The README's macOS sentence states what is actually unavailable —
  target-native execution and attended verdicts — rather than "no unattended
  work".
- No claim in this card is stated more strongly than its evidence. Where the
  Windows outcome is unknown, it stays unknown.

## Non-goals

- Do not touch anything under `src/`. Three pending operator packets bind the
  exact bytes of the built `dev` application, and a relink invalidates all
  three. This card is tracker and documentation work only.
- Do not retire the `dif_assets_contract` row. It is unmeasured for Windows,
  not resolved.
- Do not close [[BPR-0347]], [[BPR-0358]], [[BPR-0350]], or [[BPR-0378]].

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

## Result — 2026-08-19

All three corrections landed. Nothing under `src/` was touched, so the dev
application binary is unchanged and the three pending operator packets that bind
its bytes are intact.

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| `dif_assets_contract` passes on this host | `core-only` graph | `ctest --test-dir build/core-only -R '^dif_assets_contract$'` | the registered contract runs and reports | 1/1 pass, 27.34 s | this commit |
| …and the same assets check passes directly | `tests/scripts/dif_asset_tests.py` | `python3 tests/scripts/dif_asset_tests.py` | it exercises the canon without CTest framing | 35 tests, OK | this commit |
| …and neither result can speak to Windows | `zlib` linkage; `scripts/dif_assets.py` imports | `python3 -c "import zlib; print(zlib.ZLIB_RUNTIME_VERSION)"`; read the import list | stock `1.2.12` is [[BPR-0378]]'s stated exemption precondition, and [[BPR-0350]]'s reverted half is absent at HEAD | `1.2.12`; `tempfile` only | this commit |
| the format gate does not fail at HEAD | the tool the gate invokes | `clang-format --dry-run --Werror -style=file src/app/src/github_application.cpp` | `--Werror` makes any disagreement a nonzero exit | clean, exit 0, clang-format 22.1.8 | this commit |
| [[BPR-0367]] is terminal | issue board | front matter of the card in `60-done/` | lane placement is authoritative | `status: Done`, fix at `14a654d` | this commit |
| the README sentence was falsified by later commits | `git log` | `git log -1 --format='%ci' 7dd4641` and `0c4b3b7` | the sentence's commit precedes three commits that are counterexamples to it | 21:30:10 vs 21:44:38, same day | this commit |
| the corrections keep the board consistent | `scripts/verify_documentation.py` | `python3 scripts/verify_documentation.py --root .` | it rejects lane/status disagreement and a frontier table that does not match the lanes | `DOCUMENTATION_OK` | this commit |
| the documentation contract suite still passes | `tests/scripts/verify_documentation_tests.py` | `python3 tests/scripts/verify_documentation_tests.py` | 94 cases including the frontier-derivation cases [[BPR-0399]] added | 94 tests, OK | this commit |
| no whitespace damage | Git | `git diff --check` | it fails on trailing whitespace and conflict markers | clean | this commit |

### What this card deliberately did not do

The `dif_assets_contract` row is **not** retired. Measuring a green result on a
host that cannot reproduce the failure is evidence about the host, not about the
claim, and recording it as a retirement would repeat the exact error this card
was opened to correct.

[[BPR-0334]]'s extraction of the duplicated agent protocol loop was identified
as portable — the shared loop can be written and proven behind a fake transport
without compiling the Windows translation unit — and was still not done here,
for a reason that is not about portability: it edits `src/`, and a relink
invalidates three operator packets a person is currently expected to act on.
That is a sequencing constraint worth naming rather than a hidden one, and it
expires the moment those three verdicts are returned.
