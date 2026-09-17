# BPR-0816 Linux GCC 15.2 stock build evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-2`), mandate r2, task `task-20260917-lnx-build`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority
  `lane-commit`. Base `20a72bf7`.
- **State.** BPR-0816 claimed at `ef3f782b`. Stock fresh keep-going builds at
  `ef3f782b` fail as the card lists (app-release 6 TUs, dev 7 TUs). A discovery
  probe (launcher appending `-Wno-error`, not evidence) built both graphs
  completely and found no other site: app-release 42 `-Wmaybe-uninitialized` in
  five TUs; dev 14 `-Wdangling-pointer=` in five test files and 3
  `-Wunused-function` in `dif_runtime_tests.cpp`.
  - `github_capability_probe.cpp:1468` fixed at source by the integrator (pass
    the `std::string_view` alias; no temporary `QString`); GCC dev/app-release
    and clang 22 single-TU compiles clean.
  - Three leaf slices returned, consumed: 12 files changed, no suppression
    anywhere. Candidate is the working tree on `ef3f782b`.
  - Stock fresh configure and full build of both presets at the candidate is
    running (`stock.summary`, `<preset>.build-k0-candidate.log`).
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/`
  (`stock.summary`, `<preset>.build-k0-base.log`, `probe.summary`,
  `probe-<preset>.build.log`, `tools/tu.py`).
- **Next.** Focused dev tests for every touched source; clang proxy; independent
  review; commit and finish.
