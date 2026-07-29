# Foundation Security Baseline

RFC-BPR-0001 selects a deliberately small runtime:

- Qt Core, Gui, and Widgets
- native operating-system UI frameworks
- the C++ runtime

It selects no GitHub transport, credential vault, Git process, SQLite runtime,
AI process, browser engine, listener, plugin loader, telemetry, remote content
loader, or updater.

## Enforced boundaries

- The core source scan rejects Qt, SQLite, curl, thread, future, filesystem,
  random, and Qt-symbol dependencies.
- The Qt scan rejects network, process, TCP/UDP/local server, plugin/library
  loader, desktop-URL, SQLite, and provider-adapter access.
- The selected-runtime scan rejects browser/server/listener, telemetry, and
  updater surfaces.
- Binary inspection rejects WebEngine, WebKit, Chromium, Electron, Tauri,
  Node, CEF, and HTTP-server dependencies. Windows PE inspection selects
  `dumpbin`, `llvm-readobj`, or `objdump` in deterministic preference order.

## Runtime audit

The macOS, Linux, and Windows process audit launches the real application with
the offscreen Qt platform, injects a canary secret, maps process-owned network
endpoints and child processes, terminates the application, and checks captured
output. It fails on any network endpoint, subprocess, or canary-bearing output.
Windows inspection uses process-scoped PowerShell/CIM,
`Get-NetTCPConnection`, and `Get-NetUDPEndpoint` queries.

```sh
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review
```

The foundation does not claim provider credential or mutation safety; those
become applicable when RFC-BPR-0002 and RFC-BPR-0008 introduce those surfaces.
