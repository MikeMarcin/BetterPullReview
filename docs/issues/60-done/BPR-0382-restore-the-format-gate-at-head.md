---
id: BPR-0382
title: Restore the format gate at HEAD again
status: Done
rfc: RFC-BPR-0011
depends_on: []
completed: 2026-08-18
---

# Restore the format gate at HEAD again

`coding-format-check` is red at HEAD and has been since `27e4bed`
("fix(agent): put the workspace on the cache's filesystem (BPR-0341)"). Two
lines in `src/app/src/github_application.cpp` — the workspace-template
expression the BPR-0341 repair introduced — are not `clang-format` clean:

```
src/app/src/github_application.cpp:2094:62: error: code should be clang-formatted
src/app/src/github_application.cpp:2097:22: error: code should be clang-formatted
```

Found while running the gate for [[BPR-0345]], whose own file was clean before
and after. Not folded into that slice: a shared gate someone else broke is not
part of an unrelated repair, and quietly absorbing it would hide that the gate
had been red for several commits.

## This is the second time

[[BPR-0355]] was the same card with a different file. Two occurrences is a
pattern rather than an accident, and the pattern is that a slice can land
without the gate having been run over it — the gate is a manual target, and
nothing refuses a commit that has not passed it.

Recording that here rather than fixing it: the automation question belongs
with whoever owns the commit path, and this card owes only the repair.

## Required final state

- `cmake --build --preset dev --target coding-format-check` passes across the
  whole tree.
- The change is whitespace only. `git diff -w` for the slice is empty.

## Verification

```sh
cmake --build --preset dev --target coding-format-check
git diff -w HEAD -- src/app/src/github_application.cpp   # empty
```

## Result — 2026-08-18

`clang-format` applied to `src/app/src/github_application.cpp`. The change is
two lines of continuation indent and one assignment-alignment column, and
`git diff -w` for the slice is empty, so nothing but whitespace moved.

`coding-format-check` now passes across all 375 files, and the application
target rebuilds.
