# RFC-BPR-0001 Verification Plan and Ledger

RFC base: `470741a605ac401464d9e50480667ca45033ca66`

## Architecture allocation

| Region | Planned responsibility | Direct evidence |
|---|---|---|
| Functional core | Product-authority policy, compact interaction state, typed transitions, request identity, deltas, diagnostics, stale-result acceptance | Core-only configure/build/test and replay suite |
| Application host | Bounded observations, request execution, manual executor, serialized completions, cancellation, lifecycle | Host integration suite with reordered and stale completions |
| Qt presentation adapter | Native window, QAction translation, command/status projection, accessibility/focus | Qt adapter tests plus rendered application automation |
| Composition root | Concrete wiring and public path | End-to-end trace from QAction through completion and projection |

## Typed vertical slice

```text
refresh QAction
  -> refresh_requested_intent
  -> current authority/configuration observation
  -> deterministic transition
  -> refresh_foundation_request(request, session, generation)
  -> host/manual executor completion
  -> foundation_refreshed_event
  -> status_delta + command_delta
  -> incremental Qt projection
```

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves the claim | Result | Evidence |
|---|---|---|---|---|---|
| C++23 core is Qt- and effect-free | `better_pull_review_core` target | Configure, build, and test the core-only preset; scan core includes and link graph | The target builds without Qt/provider packages and forbidden imports are absent | Passed | `cmake --preset core-only && cmake --build --preset core-only && ctest --preset core-only` |
| Native Qt Widgets shell is selected | Real `better_pull_review` application | Build and launch the application; inspect linked Qt modules and rendered empty state | Native Widgets app renders build/platform/configuration facts | Passed on macOS; Windows/Linux CI unexecuted | `application_public_path`, `application_render`, native rendered inspection, and `otool -L` |
| GitHub is sole shared authority | Public core transition/product contract | Replay disagreement between local projection and GitHub observation | Result converges to the GitHub observation while private state remains distinct | Foundation passed; mixed-client contract remains RFC-BPR-0008 | `core_foundation` and `product_contract` authority tests |
| Core decides, host performs, Qt presents | Full composition harness | Trigger refresh through QAction and inspect typed trace | No widget effect call; request/completion and incremental projection are all present | Passed for foundation public path | `application_public_path` plus core, host, and Qt focused suites |
| Replay is deterministic | Core replay executable/test | Replay identical events/observations twice and compare state, requests, deltas, diagnostics | Value-equivalent outputs are produced | Passed | `core_foundation` identical replay and 256-revision property range |
| Stale completions cannot apply | Manual-executor integration | Queue two generations and complete the older one last | Only the matching session/generation updates state | Passed for foundation flow | `application_host` late-generation, serialized-completion, and shutdown cases |
| Dependency direction is inward | CMake target graph and source scan | Inspect target links and run forbidden-dependency checker | Core has no upward dependency and adapters do not bypass host contracts | Passed for current concrete surfaces | Core-only and Qt-enabled builds plus `core_dependency_boundary` and `architecture_boundaries` |
| No browser or local listener is selected | Linked application and running process | Scan dependencies/source; launch under socket inspection | No WebEngine/Chromium/Node/Electron/Tauri and no listening socket | Passed on macOS; binary CI checks pending on Windows/Linux | `architecture_boundaries`, `binary_dependency_surface`, and `runtime_process_surface` |
| No telemetry, remote loading, plugin discovery, or updater exists | Composition/configuration/runtime | Scan source/targets and inspect runtime diagnostics/network state | No selected code path or configuration exposes those behaviors | Passed for current foundation surfaces | `architecture_boundaries` and `runtime_process_surface` |
| Qt event and projection are incremental | Qt adapter harness | Trigger refresh and observe QAction/status changes | Semantic intent is emitted and only affected presentation state changes | Passed for foundation public path | `qt_foundation_window` and `application_public_path` zero-reset assertions |
| Repository instructions cover owned surfaces | Documentation gate over RFC base range | Inspect changed paths against owning agent/product docs and run validators | Every changed class has an updated or justified documentation disposition | Passed | Manual RFC-base path map plus `documentation_structure` / `DOCUMENTATION_OK` |
| Supported platform claims are evidence-backed | macOS runtime plus Windows/Linux CI | Run platform configure/build/test/launch jobs and inspect selected Qt modules | Each claimed platform has direct build and launch evidence | macOS passed; Windows/Linux CI configured but unexecuted | Native macOS render/public path; `.github/workflows/ci.yml` pending remote run |

## Platform and mutation plan

| Surface | Plan |
|---|---|
| macOS arm64 | Passed direct configure, build, test, 1.25× offscreen UI automation, native launch, and rendered inspection |
| Windows x64 | Pinned CI configure, build, test, and application launch smoke; not yet executed |
| Linux x64 | Pinned CI configure, build, test, and offscreen application launch smoke; not yet executed |
| GitHub mutations | None in RFC-BPR-0001; fixtures remain local and deterministic |

## Documentation ledger

| Scope | Changed/public surface | Required documents | Disposition | Evidence/validation | Intentionally unverified |
|---|---|---|---|---|---|
| Repository map and ownership | New C++/CMake/test/package roots | Root and nearest `AGENTS.md`, README, contributor/build docs | updated | Manual path map over RFC base range plus `documentation_structure` | None |
| Product authority | Shared/private/derived state contract | Product contract and developer overview | updated | `product_contract`, foundation implementation, README | Mixed-client mutation behavior belongs to RFC-BPR-0008 |
| Core/host/Qt architecture | Targets, typed flow, dependency direction | System architecture, state model, implementation/lifecycle docs | updated | Core-only build, target scan, composition trace, scoped `AGENTS.md` | Provider-specific ports remain dependent RFC work |
| Native UI | Empty-state shell, actions, diagnostics | UI architecture and accessibility/build docs | updated | Qt automation, native/offscreen rendered inspection, build guide | Full workbench belongs to RFC-BPR-0005 |
| Security/runtime | No browser/listener/telemetry/plugin/updater | Threat model and foundation security docs | updated | Dependency/source/socket/process/canary scans | Provider credential boundaries belong to RFC-BPR-0002 |
| Build/test/package | Presets, commands, platform CI baseline | README, contributor guide, build/test/package docs | updated | Clean preset commands, pinned CI, packaging scope docs | Signed distribution belongs to RFC-BPR-0014 |

## Documentation gate result

Manual inspection of
`470741a605ac401464d9e50480667ca45033ca66..HEAD` mapped:

- root CMake, presets, CI, warnings, hardening, and scripts to root
  `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, and
  `docs/development/build-and-test.md`;
- deterministic state and transitions to `src/core/AGENTS.md`, the product
  contract, domain/state model, and foundation implementation;
- host ports, decision lane, cancellation, and lifecycle to
  `src/application/AGENTS.md` and the foundation implementation;
- Qt event/projection/accessibility behavior to `src/adapters/qt/AGENTS.md`,
  the UX baseline, and the foundation implementation;
- composition and launch behavior to `src/app/AGENTS.md`, README, and the build
  guide;
- test and fixture surfaces to `tests/AGENTS.md`, the fixture catalog, and the
  build guide;
- runtime/security checks to the threat model and foundation security baseline;
- packaging policy to `packaging/AGENTS.md` and `packaging/README.md`;
- RFC, board, architecture, UX, and verification records to `docs/AGENTS.md`.

`scripts/verify_documentation.py` is the canonical structural validator. It
checks required coverage, all fourteen RFC/index statuses, board lane/status
agreement, `NEXT_ID`, relative links, and full-SHA GitHub Action pins.

An `Auto-Verified` closeout is blocked until every `Planned` disposition and
claim has a direct passing result or the RFC text is narrowed to the evidence.
