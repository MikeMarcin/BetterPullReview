---
id: BPR-0367
title: The format gate fails on this host, on a file nothing recently changed
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The format gate fails on this host, on a file nothing recently changed

`cmake --build --preset core-only --target coding-format-check` fails:

```
src/app/src/github_application.cpp:2094:62: error: code should be clang-formatted
                        ? std::filesystem::path{ QDir::tempPath().toStdString() }
src/app/src/github_application.cpp:2097:22: error: code should be clang-formatted
                        AgentWorkspaceRoot = std::make_unique<QTemporaryDir>(
Checking 375 file(s)
Files needing formatting:
  src/app/src/github_application.cpp
```

One file of 375, and it is the only one.

## What is and is not known

The file was last touched by `27e4bed` ([[BPR-0341]]), which predates the
Windows program's base commit `605aa56`. Nothing in that program touched it, and
the only C++ file that program did change,
`tests/sqlite_contract/platform_test_support.hpp`, is format-clean under the
same tool. So this is pre-existing, not a regression from that work.

What is **not** established is whether the file is genuinely misformatted or
whether the tool disagrees with the one that last passed over it.

`_coding-standard/scripts/format_check.py` takes `--clang-format` defaulting to
whatever `clang-format` resolves to on `PATH`, and pins no version. The version
here is **22.1.3**, from
`…/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin`. The version that
last passed this file is not recorded anywhere in the tree.

Both offending lines are in a ternary that wraps a braced
`std::filesystem::path{ … } / "…"` — exactly the kind of construct whose line
breaking has changed between clang-format releases. That makes a version
disagreement the more likely of the two explanations, and it is a guess until
someone runs an older clang-format over the same file.

Note also that `clang-format` is not on `PATH` at all without adding the Visual
Studio LLVM directory, so the gate's first failure on a fresh Windows checkout is
`Tool not found on PATH: clang-format`, not a formatting complaint.

## Required final state

- Which of the two explanations is true is established rather than assumed, by
  running at least one earlier clang-format over the same file.
- If the file is genuinely misformatted, it is reformatted and the gate passes.
- If the tool disagreed, the project states which clang-format versions it
  supports and the gate enforces or reports that, so the next host does not
  rediscover this. A gate whose verdict depends on an unrecorded tool version is
  not reproducible, which is the same class of defect [[BPR-0355]] repaired once
  already.
- The Windows developer setup records that `clang-format` needs the Visual Studio
  LLVM directory on `PATH`.

## Not in scope

Reformatting anything else, and changing `.clang-format`.

## Verification

```sh
cmake --build --preset core-only --target coding-format-check
py -3 _coding-standard/scripts/format_check.py
```

On Windows both need `…/VC/Tools/Llvm/x64/bin` on `PATH`.

## Answered from the Linux lane — 2026-08-18

Both explanations are true, for different files, and the split is now measured
rather than guessed.

Ubuntu 26.04 x86_64. `clang-format` 22.1.2 was fetched into a scratch prefix
for this comparison; 21.1.8 is what the provisioned toolchain actually supplies
(`~/.local/bin/clang-format` →
`~/.local/bpr-toolchain/debroot/usr/lib/llvm-21/bin/clang-format`, and
`~/.local/bin` precedes `/usr/bin`).

| `clang-format` | files checked | files needing formatting | violations |
|---|---|---|---|
| 21.1.8 | 375 | 292 | 10275 |
| 22.1.2 | 375 | **1** | 2 |

The one file 22.1.2 reports is `src/app/src/github_application.cpp`, at lines
2094 and 2097 — the same file and the same two lines this card recorded from
Windows under 22.1.3. Two hosts, two operating systems, two 22.x builds, one
answer. **That file is genuinely misformatted**, and reformatting it is this
card's second bullet, still to do. Nothing else is.

Everything else is the other explanation. The dominant cause reduces to three
lines against the project's own `.clang-format`:

```sh
printf 'auto f() {\n\tauto state = process_state{ };\n}\n' > probe.cpp
clang-format-22 --style=file:.clang-format probe.cpp   # auto state = process_state{ };
clang-format-21 --style=file:.clang-format probe.cpp   # auto state = process_state{};
```

21 strips the space from an empty braced initializer and no setting restores
it: `Cpp11BracedListStyle` produces `{}` either way, and `SpaceInEmptyBraces` is
not a key 21 accepts. `{ }` is the project's style in every file that has one,
so the disagreement is not distributed randomly — of the C++ files under `src/`,
every one of the 151 containing `{ }` is reported dirty under 21, and every one
of the 81 clean files contains none.

That correlation also corrects [[BPR-0156]], which saw ~290 files dirty under
18 through 21 and concluded "this is not a blanket version disagreement,"
reasoning that `src/core/src/model.cpp` was clean under all of them while
`src/app/src/github_application.cpp` was not. `model.cpp` is clean because it
contains no `{ }` at all, not because the disagreement is selective.

So the version the tree is formatted for is 22 or later, and the Linux
toolchain provisions 21. Until this card names a version, the C/C++ half of
`coding-format-check` is unusable on the provisioned Linux toolchain: cards
worked in the Linux lane can only claim their own edits added no violation, not
that the gate passes.

Not done here: `github_application.cpp` was not reformatted, and
`.clang-format` was not touched. Both belong to this card, and a concurrent
session may hold it.

## Claimed on macOS — 2026-08-18

The macOS line holds the card from here. Two of the four required-final-state
bullets were already answered before it was claimed, and the answers are
recorded rather than repeated:

- The file **was** reformatted, by [[BPR-0382]] at `14a654d`, which is the
  macOS line's own copy of this failure. Its diff is exactly the two lines this
  card names, whitespace only. `clang-format` 22.1.8 on this host now reports
  the whole tree clean: `Checking 381 file(s)` … `Formatting check passed.`
  Three 22.x builds on three operating systems therefore agree about that file
  in both directions — dirty before `14a654d`, clean after.
- `.clang-format` was not touched, and is not in scope.

What remains is the pair of bullets nobody has done: naming the supported
version, and recording the Windows `PATH` requirement.

## The version is now part of the verdict — 2026-08-19

`MINIMUM_CLANG_FORMAT_VERSION` in `_coding-standard/scripts/_coding_standard.py`
is `(22,)`, and both `format_check.py` and `format.py` read the tool's reported
version before doing anything else. Below the minimum, or with no parseable
version at all, they print what they found, what they need, and why, and exit 2
without invoking the tool on a single file. `format.py` says the stronger thing,
because an older tool there would not misreport those files but rewrite them.

`--allow-unsupported-clang-format` runs anyway. It still prints the diagnostic
and adds `this result is not a gate verdict`, which is the honest description:
on the provisioned Linux toolchain a 21.x run can say whether an edit added a
violation *relative to 21*, and cannot say the gate passes. Both scripts now
also print the version they used next to the tool path, so a recorded run says
which tool produced it — the fact this card could not recover for the run that
last passed over `github_application.cpp`.

The banner parser takes the first dotted number after the banner text, because
every distribution writes a different prefix: `Homebrew clang-format version
22.1.8`, `Ubuntu clang-format version 21.1.8`, `Apple clang-format version
16.0.0 (clang-1600.0.26.6)`.

`coding_standard_format_gate_contract` is registered and covers it. Its stubs
answer `--version` and append to a marker file on any other invocation, so a
refusal is proven by the *absence* of a formatting call rather than by the exit
status alone — which also proves `format.py` cannot rewrite a file it refused.
Removing the check from `format_check.py` fails three of its eight cases
directly; that was run rather than assumed.

Documentation: `_coding-standard/README.md` gains a
`Supported clang-format Versions` section, and `docs/development/build-and-test.md`
gains `The format gate needs clang-format 22 or newer`, carrying the measured
21-versus-22 split, the `{ }` mechanism, and the Windows `PATH` line — Visual
Studio 2022 does not put `clang-format` on `PATH`, so the first failure on a
fresh Windows checkout is `Tool not found on PATH: clang-format`.

### Evidence

| Check | Result |
|---|---|
| `cmake --build --preset core-only --target coding-format-check` | `Checking 381 file(s) with …clang-format (22.1.8)` / `Formatting check passed.` |
| `python3 _coding-standard/scripts/format_check.py` | same, passed |
| `ctest --preset dev -R '^coding_standard_format_gate_contract$'` | 1/1 passed, 1.28 s |
| gate deleted from `format_check.py`, contract re-run | 3 of 8 cases fail; restored and re-run 8/8 |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

### A stronger fact, found by running the refusal against a real old tool

The stub cases prove the gate's logic; Apple clang-format 16.0.0, which is
actually installed here, proves the premise. Under
`--allow-unsupported-clang-format` it reports:

```
.clang-format:11:3: error: unknown key 'AlignFunctionPointers'
Error reading .clang-format: Invalid argument
```

`AlignFunctionPointers` arrived in clang-format 18, so releases before that
cannot read this project's style file at all — they fall back to a default style
and call everything dirty for a reason that has nothing to do with the files.
`src/core/src/model.cpp` is reported dirty under 16 even though the Linux lane
measured it clean under 18 through 21, which is exactly that fallback.

So the floor has two independent causes, not one, and the middle tier is the
dangerous one because it produces a plausible-looking result:

| clang-format | Behaviour |
|---|---|
| ≤ 17 | cannot parse `.clang-format`; falls back to a default style |
| 18–21 | parse it, render `{}` where the tree holds `{ }` |
| ≥ 22 | render the tree as committed |

Both documentation sites now carry the three tiers rather than only the `{ }`
difference.

### Left open deliberately

The Linux toolchain still provisions 21.1.8. This card does not change that; it
makes the consequence explicit instead of silent. Provisioning a supported
clang-format on that host is separate work, and until it happens the C/C++ half
of `coding-format-check` there returns a refusal rather than 292 false
positives.
