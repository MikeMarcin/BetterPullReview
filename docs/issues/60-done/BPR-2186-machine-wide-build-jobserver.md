---
schema: "implement-rfc.card.v1"
id: "BPR-2186"
title: "Share one machine-wide build jobserver across agent builds"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2186--BPR-2186--b1183192e6e3.json","kind":"tooling","found_by":"owner: several agents ran full-width Ninja builds at once and used 83 GB of RAM; asked for a machine-wide GNU make jobserver with the dev preset as the fallback"}
acceptance: [{"criterion":"AC1: Ninja 1.13.2 is a FIFO jobserver client and a shared pool bounds concurrent builds","outcome":"pass","revision":"f6f390e70","evidence":{"kind":"command","command":"two concurrent /opt/homebrew/bin/ninja builds of 12 one-second logged sleep jobs sharing MAKEFLAGS=--jobserver-auth=fifo:PATH (scratch project)","detail":"Ninja prints 'Jobserver mode detected'. Raw clients: 4 tokens peaked at 6 and 8 tokens at 10 jobs in total, i.e. N plus one implicit slot per client; an explicit -j disables the client (4 tokens, -j 8 each: 16). A missing FIFO is an error message and a fallback to Ninja's default width, exit 0; a FIFO with no server runs one job at a time and does not hang. Through scripts/jobserver_ninja.py, which pays each implicit slot with a token: 4 tokens peaked at 4 in total, 2 tokens at 2."}},{"criterion":"AC2: token server with idempotent start, status, pidfile-only stop and leak handling","outcome":"pass","revision":"f6f390e70","evidence":{"kind":"command","command":"build_jobserver.py start twice (both exit 0), status (exit 0 running, 3 not), stop; SIGKILL a joined Ninja mid-build; SIGKILL the server","detail":"After the killed Ninja: 1 of 4 free, then the server logged 'idle with 1 of 4 tokens: restored 3 leaked' and status read 4 free. A killed server leaves fifo and server.pid, status reports not running (the lock died with it) and the next start replaces them. lsof cannot find a FIFO by path on macOS, so idleness is the absence of locked client leases; FIONREAD on a read-write FIFO descriptor reads 0 on macOS, so the server counts through a read-only one. Limits in the script's docstring."}},{"criterion":"AC3: plain cmake --build --preset dev joins a running server and otherwise builds at -j 8","outcome":"pass","revision":"f6f390e70","evidence":{"kind":"command","command":"cmake --preset dev; cmake --build --preset dev --target bpr_fixture_agent with the server down, with a stale FIFO from a SIGKILLed server, and with the server up (12 tokens), sampling the real Ninja's argv and environment","detail":"Down: '/opt/homebrew/bin/ninja -j 8 bpr_fixture_agent', exit 0. Stale: the same, exit 0. Up: '/opt/homebrew/bin/ninja bpr_fixture_agent' with MAKEFLAGS=--jobserver-auth=fifo:~/.cache/bpr/jobserver/fifo, exit 0, all 12 tokens back afterwards. Reconfigure of the existing build/dev took 20 s and recorded CMAKE_MAKE_PROGRAM:FILEPATH=<worktree>/scripts/jobserver_ninja.py."}},{"criterion":"AC4: AGENTS.md documents start, joining, the -j 8 fallback and the width rule; gates","outcome":"pass","revision":"f6f390e70","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .; python3 tests/scripts/platform_configuration_tests.py","detail":"DOCUMENTATION_OK; 7 tests OK (1 skipped)."}}]
---

# Share one machine-wide build jobserver across agent builds

Several agents built at once with full-width Ninja and the Mac ran out of
memory (83 GB). The owner asked for one GNU make jobserver every build joins,
with the dev preset building at -j 8 when no server runs.

## What landed

- `scripts/build_jobserver.py start|status|stop`: a FIFO and 12 tokens
  (`--tokens N`) in `~/.cache/bpr/jobserver` (`$XDG_CACHE_HOME` honoured;
  `BPR_JOBSERVER_DIR` overrides for tests). Liveness is an exclusive flock the
  server holds, so pid reuse cannot fake a live server; `stop` signals only the
  pid in its own pidfile and only while that lock is held.
- `scripts/jobserver_ninja.py`, the dev preset's `CMAKE_MAKE_PROGRAM`. Ninja
  ignores a jobserver whenever `-j` is given, and the preset now passes
  `-j 8` (`"jobs": 8`), so the wrapper decides: server live, drop `-j`, name the
  FIFO in MAKEFLAGS and take one token for the build's lifetime to pay for
  Ninja's implicit slot; otherwise run Ninja with `-j 8`. `--version`, `-t`,
  `-n`, `-h` and nested Ninja (MAKEFLAGS already names a jobserver) pass through.
- AGENTS.md, Commands, "Build jobserver".

## Limits

- One build may take all N tokens; the per-build `-j 8` applies only without
  a server.
- A Ninja run by hand with this MAKEFLAGS holds no lease and gets its implicit
  slot free; an idle-time reconciliation during its run can add tokens it holds,
  taken back at the next idle moment. A bare `ninja` without MAKEFLAGS does not
  join at all.
- Leaked tokens come back only when no client lease is held; `stop` then
  `start` always resets.
- If the server dies between the wrapper's check and Ninja's start, that Ninja
  reports it cannot open the FIFO and builds at its default width.
- A tree configured before this change joins after one `cmake --preset dev`.
  The forwarded make program changes the KSyntaxHighlighting signature, so that
  configure may rebuild the provider once.
