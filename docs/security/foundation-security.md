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
macOS direct-child inspection uses the process-scoped
`proc_listchildpids` libproc API with a fixed 1,024-child limit and fails closed
on unavailable APIs, invalid results, or overflow; it does not launch `ps` or a
shell inside the network sandbox. Windows inspection uses process-scoped PowerShell/CIM,
`Get-NetTCPConnection`, and `Get-NetUDPEndpoint` queries.

```sh
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

The foundation does not claim provider credential or mutation safety; those
become applicable when RFC-BPR-0002 and RFC-BPR-0008 introduce those surfaces.

## Dif asset security boundary

RFC-BPR-0019's raster pack is an offline Qt-adapter input. Its validator admits
only a closed relative-path inventory and rejects symlink components before it
reads provenance-bound source bytes. Source and generated PNGs are canonical,
non-interlaced RGBA8 with only `IHDR`, `IDAT`, and `IEND`; dimensions, encoded
bytes, decoded bytes, alpha, hidden RGB, gutters, edges, frame counts, rates,
and aggregate budgets are bounded before generation succeeds.

The asset tree contains no remote reference, active content, executable
payload, private path, credential marker, repository data, username, or runtime
loader instruction. The ImageGen prompt is retained as explicit creation
provenance in the asset README, not in PNG metadata and not as a regeneration
authority. The checked-in source bytes are authoritative. The offline tool uses
only its declared Python standard-library surface and contains no HTTP, model,
or subprocess capability.

Clean selected-graph and binary scans keep the generator, art dependencies,
manifest, catalog, and raster resources out of core-only and out of the current
tests-disabled application. RFC-BPR-0020 must repeat the resource and runtime
audit when it intentionally changes that application boundary. The attended
identity review, rather than a byte scan, owns visual originality, GitHub-mark
absence, and foreign-character exclusion.

## RFC-BPR-0016 device authorization boundary

The foundation selects no browser engine, listener, or embedded web view, and
the device grant does not add one. It was chosen over the authorization-code
redirect for exactly that reason: the redirect variant needs an inbound
loopback socket to receive its callback, and the device grant polls an
outbound endpoint instead.

- **Public client, no secret.** The shipped artifact carries one public GitHub
  App client identifier and no client secret; a public-client registration is
  what makes that possible. The release scan requires the recorded
  distributable client to be present and [[BPR-0276]]'s private owner-only
  verification client to be absent ([[BPR-0318]],
  `rfc0016_production_identity_security`).
- **Transient material stays inside the host.** The device code lives for the
  poll window only and is never written to settings, logs, diagnostics,
  SQLite, or evidence; the access and refresh tokens reach the platform vault
  and nothing else. The core, the projection, and the Qt adapter cannot name
  any of them ([[BPR-0277]], [[BPR-0282]], `rfc0016_test_inventory`).
- **Redirect and certificate rules are inherited, not restated.** Both device
  routes run on the RFC-BPR-0002 native HTTPS transport under system trust and
  proxy configuration, with no certificate-error bypass, unknown-authority
  redirects rejected before any credential or body is forwarded, and every
  query-bearing redirect failing closed ([[BPR-0278]],
  `native_https_device_authorization_contract`).
- **The browser is the operating system's.** `bpr::app::system_browser_opener`
  hands one already-validated address to the platform's own launch service and
  supervises nothing afterwards. It re-parses the address and refuses anything
  that is not an absolute `https` address with a host and no userinfo, so an
  opener cannot be talked into a `file:` or `javascript:` address. No suite in
  this repository binds it: a tests-enabled graph binds a recording opener
  instead ([[BPR-0516]],
  `application_device_authorization_browser_handoff`).

The device-flow service endpoints are deliberately not forbidden material. The
host owns them and a shipped binary that can run the flow must contain them,
so forbidding them would be a scan that fails on correct code.

## Designated-live replay boundary

The optional macOS designated-live verification lane uses one explicit
owner-private replay namespace below the application-data root. Initialization
or deliberate reset is a separate operator command and selects a fresh random
namespace identity. Attended startup has no create-on-missing fallback: every
approval binds the namespace schema, normalized absolute path, and identity,
and missing, unsafe, linked, substituted, full, mismatched, or unsynced state
fails closed. Approval and replay paths are walked descriptor by descriptor
without following symlinks; an exclusive spent record is committed with
`F_FULLFSYNC`, not bare `fsync`, and then checked against its retained inode
and link count before a grant can exist. A spend that has been reported
therefore survives power loss or a kernel panic.

This is a narrowed local-authority guarantee. Detected filesystem races remain
fail-closed, but intentional same-user or administrator deletion, rollback,
restoration, or mutation of replay state is outside the approved threat model,
as is loss the filesystem never reports, such as media corruption that removes
a spent record while leaving the namespace intact. The namespace and approval
contain no credential, and their path, identity, and bytes are not accepted
through environment variables or emitted in diagnostics or Qt state.

### Runtime image containment

The opt-in macOS lane starts through a fixed `/usr/bin/python3 -I -S`
preloader stored in the generated CTest execution plan. The preloader captures
one bounded launcher byte sequence, matches it to the producer source manifest,
and executes those captured bytes. Stage zero attaches the exact
literal-digest DMG read-only below a fresh owner-private root. It retains the
image and mounted runtime identities through the complete supervisor and
application process-group lifetime, revalidates them on every child exit, and
uses ordinary non-force detach after bounded TERM/KILL and reap recovery.

This boundary protects the mounted filesystem, not every representation of its
backing storage. The invoking owner can still open the DMG for writing while
it is attached. Post-child identity and digest checks detect persistent drift,
but they cannot prevent transient modified backing bytes from influencing a
running child. DiskImages also accepts a pathname for `hdiutil attach`; it does
not consume the already retained descriptor atomically. The generated CTest
manifest and cache are trusted against same-user rewriting. These limits are
why each attended decision binds one exact stored DMG and why no cross-build
DMG reproducibility claim is made.
