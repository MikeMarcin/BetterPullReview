---
schema: "implement-rfc.card.v1"
id: "BPR-0057"
title: "Support intentional aggregate defaults under GCC 15"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0048"]
completed: "2026-08-01"
---

# Support intentional aggregate defaults under GCC 15

Keep the repository warning-as-error policy while allowing the RFC-BPR-0003
core and host to use intentional partial designated initialization whose
omitted aggregate members retain their declared defaults. GCC 15 reports
those valid defaulted members through `-Wmissing-field-initializers`, blocking
the fresh Ubuntu 26.04 build before BPR-0049 can exercise the application.

Scope the compiler exception to GNU C++ only. Do not weaken other warnings,
disable warnings-as-errors, change aggregate semantics, or add a hosted build
surface.

## Completion evidence

- A fresh Ubuntu 26.04 GCC 15.2 configure and build reaches the selected real
  application and Qt epoch-comparison targets with warnings-as-errors active.
- The effective C++ command retains `-Werror` and adds only
  `-Wno-missing-field-initializers` for GNU C++.
- The macOS developer build remains clean, documentation validation passes,
  no GitHub Actions workflow exists, and `git diff --check` passes.

## Result

`bpr_enable_warnings` retains `-Werror` and all existing warning groups while
disabling only GCC C++'s `-Wmissing-field-initializers`. The exception is a
CMake compile-language-and-compiler generator expression, so it does not
alter C warnings, Clang, AppleClang, or MSVC.

## Evidence

- Ubuntu 26.04 aarch64 full-system QEMU guest, Linux 7.0.0-28, GCC 15.2,
  Qt 6.10.2, Git 2.53, and libsecret 0.21.7: fresh
  `build/gcc15-evidence` configure passed and the selected application and Qt
  epoch targets completed all 57 build steps.
- Ninja's effective `epoch_transition.cpp` command contains both `-Werror`
  and `-Wno-missing-field-initializers`; the same source failed before the
  scoped exception because GCC promoted only that warning family.
- The macOS `dev` application and Qt epoch targets reconfigured and built
  cleanly. `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`, the on-disk GitHub Actions workflow count is zero, and
  `git diff --check` passes.
