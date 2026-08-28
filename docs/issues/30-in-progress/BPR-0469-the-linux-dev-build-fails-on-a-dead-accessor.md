---
id: BPR-0469
title: The Linux dev build fails on a dead line-number accessor
status: In Progress
rfc: RFC-BPR-0005
depends_on: []
---

# The Linux dev build fails on a dead line-number accessor

`cmake --build --preset dev` does not complete on Linux at `13fbc9df`. It
stops in the Qt adapter:

```
src/adapters/qt/src/diff_workbench.cpp:275:22: error: 'QWidget*
bpr::qt::{anonymous}::source_pane::gutter() const' defined but not used
[-Werror=unused-function]
```

`source_pane::gutter()` is declared at line 66 and defined at line 275, both
inside an anonymous namespace, and is called nowhere. Every real use reaches
`Gutter` directly through the member. GCC diagnoses an unused member function
of an anonymous-namespace class; Clang does not, which is why a macOS pass
never saw it.

It arrived with [[BPR-0455]], which drew the line-number gutter, and survived
[[BPR-0457]].

## Why it was not caught

The card that introduced it ran focused selectors, and the focused selectors
that matter for a Qt change link the Qt adapter through targets that were
already built. Nothing on this host built the whole `dev` tree from a state
that included the new function until now. [[BPR-0443]] made `coding-tidy`
usable on Linux but ran it against the compilation database, not a full build;
[[BPR-0421]] and [[BPR-0440]] fixed the tests-disabled Release link, which
compiles a different target set.

## Required final state

- `cmake --build --preset dev` completes on Linux.
- The repair is removal, not a suppression or a cast to void: the accessor has
  no caller and no seam depends on it.
- The Qt workbench selectors this file owns pass afterwards.

## Not this card

The line-number feature itself, which is done and unaffected, and the
identifier-naming findings in the Linux-only test sources, which are
[[BPR-0470]].

## Found by

Reproduced at unmodified `HEAD` while verifying [[BPR-0444]]: `git stash` of
that card's two files, then `cmake --build --preset dev --target
better_pull_review_adapter_qt`, fails identically.
