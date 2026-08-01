---
id: BPR-0057
title: Support intentional aggregate defaults under GCC 15
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0048]
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
