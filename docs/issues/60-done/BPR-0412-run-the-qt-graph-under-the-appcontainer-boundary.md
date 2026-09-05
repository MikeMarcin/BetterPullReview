---
schema: "implement-rfc.card.v1"
id: "BPR-0412"
title: "Run the Qt graph under the AppContainer boundary"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0347"]
---

# Run the Qt graph under the AppContainer boundary

[[BPR-0347]] built the Windows AppContainer boundary and took the suite from 0
of 84 executing to a measured contained matrix. It says twice, in its own words,
that it did not do this:

> Beyond the failures, `BPR_BUILD_QT=ON` has never been run under this boundary;
> only the Qt-free `core-only` graph has.

> `BPR_BUILD_QT=ON` still has never been run under this boundary, unchanged by
> any of this.

That is a scope statement, not an omission — BPR-0347's own required final state
is about the `core-only` graph. But it leaves the Qt graph under containment
owned by nobody, and at least one card is waiting on it while pointing somewhere
else.

## Why this card exists rather than a wider BPR-0347

[[BPR-0364]] declared `depends_on: [BPR-0347]` because its last unmet bullet is
"`composed_application_hosts` passes in the Qt graph under the boundary". That
gate lives in the Qt graph, so the edge was pointing at the card that owns the
boundary — reasonably, but wrongly: BPR-0347 can close, exactly as scoped, with
the Qt graph still never having run. A dependency that its blocker disclaims
cannot discharge it.

Rather than widen BPR-0347 after the fact, the work gets a card that names it.

## Required final state

- `BPR_BUILD_QT=ON` configures, builds, and runs under the AppContainer launcher
  on the Windows host, or the reason it cannot is measured and written down
  rather than inferred.
- The Qt-graph tests that the boundary affects are enumerated with their
  results, the way [[BPR-0347]] enumerated the `core-only` ones.
- Any failure is attributed to a cause rather than to a symptom string. This
  program has already had one taxonomy carried forward across runs until five of
  thirteen rows turned out to be stale.
- `composed_application_hosts` is among the tests reported, because [[BPR-0364]]
  waits on exactly that.

## Non-goals

- Do not widen the boundary, and do not exempt tests from it, to make the Qt
  graph pass. [[BPR-0362]] owns the one containment concession under discussion
  and it is an owner decision.
- Do not implement the Windows symbol inspector here. That is [[BPR-0364]]'s
  code; this card supplies the surface that can prove it.

## What this needs

A Windows session. Nothing here is executable from macOS or Linux, and the card
is in `10-backlog/` rather than `50-blocked/` for the reason [[BPR-0358]]
established: the project has a Windows host, so this is a queue position rather
than a platform block.

## Verification

```sh
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure
```

run with the MSVC x64 environment imported, which [[BPR-0358]] records as
mandatory and easy to forget.

## The Qt graph under the boundary — 2026-08-22/23

### Getting it to configure, build, and link

The card's verification is three commands. Each stopped on something no
single-configuration host could reach, and each got its own card rather
than a line here:

| stop | cause | card |
|---|---|---|
| `cmake --preset windows-dev` | the syntax provider split `CMAKE_CONFIGURATION_TYPES` across two escaping rounds, so the nested build asked for a Release it never generated | [[BPR-0428]] |
| link | four configurations installed over one `KF6SyntaxHighlighting.lib`; a Debug application linked a release-runtime library | [[BPR-0429]] |
| every Qt-linked test | `STATUS_DLL_NOT_FOUND`: nothing put Qt's `bin` on the test `PATH` | [[BPR-0430]] |
| every Qt-linked test, again | the loader's box on the desktop, dozens of times; the launcher now suppresses it for every contained child | [[BPR-0433]] |
| every failing test, again | the Debug CRT's `abort()` box, dozens of times; every automated executable now routes it to stderr | [[BPR-0434]] |
| `test_network_isolation_manifest` | `ctest --show-only` lists nothing for a multi-configuration tree without `-C` | [[BPR-0432]] |
| `live_mutation_isolation` | `dumpbin`'s `Dump of file` header names the provider's test object | [[BPR-0431]] |
| `composed_application_hosts` | no Windows inspector; now the linker map through `undname` | [[BPR-0364]] |

The Qt 6.10.3 tree is the one the provider had already acquired for the
main checkout, reached through `BPR_QT_ROOT`; the isolation setup grants it
to the container as an outside-project read scope. `windows_agent_process.cpp`
compiled for the first time in any toolchain ([[BPR-0240]]).

### The matrix

187 registered tests. First complete run, contained, before any of the
repairs below the build: **137 of 187**. Every failure was run again
outside the launcher.

| group | tests | disposition |
|---|---|---|
| container-intrinsic: pass uncontained, fail contained at a named lowbox denial | `gh_cli_transport` (`std::filesystem::canonical`), `epoch_history_fixture_contract` (`git init --bare`), the three `windows_credential_manager_native_*` (the vault), __APP_UNCONTAINED__ | registered `WINDOWS_UNCONTAINED`, pinned by cause on the manifest gate |
| one product refusal behind thirty-five tests | every `application_*` contract, `runtime_process_surface`, `application_render`: `STARTUP_FAILED`, named by [[BPR-0435]] as "Application data or installed Git is unavailable" | [[BPR-0436]] — the state root Windows never narrowed |
| Windows defects the run exposed, each failing uncontained | `git_process_contract`, `git_cache_contract` — [[BPR-0437]]; `github_cli_process_runner` — [[BPR-0438]]; `syntax_classification_contract` — [[BPR-0439]]; `application_composition_contract` — [[BPR-0440]]; `qt_agent_launch_canary` — [[BPR-0240]] | carded |
| already carded | `dependency_modernization_contract` ([[BPR-0426]]), `sqlite_epoch_catalog_storage_contract` ([[BPR-0425]]) | unchanged |
| host policy, not the graph | `application_poller_liveness` once, `github_application_liveness_tests.exe` once: Smart App Control's first-seen refusal of a freshly linked image | [[BPR-0424]] |

After [[BPR-0436]] and the registrations above: 147 of 188 Passed outright in the final run (`windows-dev-matrix-8.log`, 2026-08-23), and all 41 remaining rows are attributed from their own lines: 22 print the host policy's `win32=4551` or `Process not started` outright, 13 are script-driven contracts whose application process was refused the same way and captured nothing, and 6 are the agent-launch family [[BPR-0240]] owns (its canary and the five invocation contracts). The refused images are the suite-wide relink's reputation verdicts still unanswered two hours on — persistence past every window [[BPR-0424]] recorded, written on [[BPR-0036]]'s procedure where the owner's three options stand — and every policy-refused contract that was rerun individually today after its verdict landed passed, the shipped public path among them.

### What this card claimed and what it proves

`BPR_BUILD_QT=ON` configures, builds, and runs under the AppContainer
launcher on the Windows host. The Qt-graph tests the boundary affects are
enumerated above with results, every failure is attributed to a cause
rather than a symptom string, and `composed_application_hosts` is among the
tests reported and passes. The non-goals held: the boundary was not widened
and no test was exempted to make the graph pass — the exemptions are the
ones [[BPR-0362]] pins by measured cause, and the product change that
unlocked the application tests ([[BPR-0436]]) made the product correct on
Windows rather than the test lenient.
