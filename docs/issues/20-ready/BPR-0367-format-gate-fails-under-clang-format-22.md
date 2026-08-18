---
id: BPR-0367
title: The format gate fails on this host, on a file nothing recently changed
status: Ready
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
