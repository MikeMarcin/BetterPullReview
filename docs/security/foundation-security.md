# Foundation Security Baseline

The opening inventory describes the historical RFC-BPR-0001 foundation graph.
The current release/security inventory is the
[corporate security overview](corporate-security-overview.md), with exact
qualification in [RFC-BPR-0014](../verification/RFC-BPR-0014.md). Later hosts add
GitHub, vault, Git, SQLite, agent, profile activation, diagnostics and the
qualified updater; the foundation's absence claims are not current product claims.

RFC-BPR-0001 selects a deliberately small runtime:

- Qt Core, Gui, and Widgets
- native operating-system UI frameworks
- the C++ runtime

It selects no GitHub transport, credential vault, Git process, SQLite runtime,
AI process, browser engine, listener, plugin loader, telemetry, remote content
loader, or updater.

That is the RFC-BPR-0001 foundation graph. The later RFC-BPR-0035 composition
adds the narrowly authorized macOS updater described in the
[release updater security boundary](release-updater.md), including its fixed
network/process inventory and remaining qualification gaps. It adds no browser
runtime, telemetry, privileged service or update listener.

## Chosen CLI credential exception

RFC-BPR-0078 permits an exact chosen-account stock-gh lookup into bounded,
noncopyable adapter-owned transient storage. One operation holds those bytes
through identity checks, API calls and gh-owned Git/helper descendants; only the
runner supplies the correct primary token variable. No token crosses into core,
application/Qt projections, profile metadata or grants, and BPR creates no vault,
SQLite, configuration, journal, object-store, argument or log copy. Failed named
retrieval refuses before network work without another authority. Owned memory is
cleared after drain/quarantine; Qt, OS and stock-child copies, swap and crash
capture remain explicit limits. [The qualification ledger](../verification/RFC-BPR-0078.md)
binds native helper controls and actual descendant lifecycle evidence. Native
vault, crash and release gaps below retain their original scope.

## Enforced boundaries

- The core source scan rejects Qt, SQLite, curl, thread, future, filesystem,
  random, and Qt-symbol dependencies.
- The Qt scan rejects network, process, TCP/UDP/local server, plugin/library
  loader, desktop-URL, SQLite, and provider-adapter access.
- The selected-runtime scan rejects undeclared browser/server, telemetry and
  loader surfaces. Current declared hosts include bounded owner-authenticated
  AF_UNIX profile activation and the qualified macOS updater; the
  [release policy](release-policy.md) defines their scope and direct checks.
- Binary inspection rejects WebEngine, WebKit, Chromium, Electron, Tauri,
  Node, CEF, and HTTP-server dependencies. Windows PE inspection selects
  `dumpbin`, `llvm-readobj`, or `objdump` in deterministic preference order.

## Runtime audit

The macOS, Linux, and Windows process audit launches the real application with
the offscreen Qt platform, injects a canary secret, maps process-owned network
endpoints and child processes, terminates the application, and checks captured
output. Its current release course rejects TCP/UDP endpoints, undeclared
subprocesses and canary output, and separately inspects the declared owner-private
AF_UNIX activation endpoint. An idle sample does not qualify active provider,
vault, agent or updater traffic or the full installed-package matrix.
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
editable source, manifest, provenance, digest records, and review media out of
the runtime resource collection. They require the same ten accepted generated
strips in tests-enabled and tests-disabled applications and keep all Dif bytes,
Qt, and platform motion code out of `core-only`. The attended identity review,
rather than a byte scan, owns visual originality, GitHub-mark absence, and
foreign-character exclusion.

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
  and nothing else — the refresh token parked by a grant is flushed to the
  vault the moment composition binds the session's authority, and the
  probe that reads the refresh slot destroys what it read on the vault's
  own thread and passes only a yes ([[BPR-0536]]). Both slots share one
  erasure path — sign-out or a superseding enrollment deletes the refresh
  material with the access token under the
  same authority, the one exception being a rotation superseding the
  session it just renewed, where the erasure is held only while the
  incoming session may still name that same authority and runs the moment
  it does not ([[BPR-0547]], [[BPR-0549]]) — and the probe's yes renews a credential only when that
  credential's own provenance is a device grant, so a pasted token is
  never silently replaced by a session minted from an orphaned refresh
  token ([[BPR-0540]]). Under the owner's BPR-0541 policy, orderly shutdown
  retains only a settled device pair whose exact binding and lifetime were
  durably recorded; manual sessions still require exact deletion. Replacement
  tombstones the stored binding before vault changes and makes it resumable
  only after the new pair settles. Metadata effects and cleanup terminals are
  counted through shutdown quiescence. The core, the
  projection, and the Qt adapter cannot name any of them ([[BPR-0277]],
  [[BPR-0282]], `rfc0016_test_inventory`).
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
  opener cannot be talked into a `file:` or `javascript:` address. Both build
  graphs bind it for a person's run. An automated-test boundary or selected
  fixture instead binds the recording opener ([[BPR-0797]],
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

## Anonymous reading

RFC-BPR-0017 enables explicit credential-free native HTTPS on macOS only.
Activation performs zero requests; a selected public read uses the existing
TLS, proxy, redirect, response-limit, and cancellation boundaries with no vault
or Authorization effects. Public Git acquisition uses native smart HTTP; it
does not reuse the designated-live observer subprocess. Core, native transport,
provider, host, cache, Qt, and tests-disabled Release evidence are recorded in
[the RFC-0017 ledger](../verification/RFC-BPR-0017.md). A diagnostic dry run or
an idle runtime audit alone does not prove a completed public read.

## Semantic comment compatibility boundary

RFC-BPR-0022 adds no write authority. Native and file children share the existing
exact source/target/actor grant and serialized provider route. A durable
possibly-transmitted native attempt admits GET-only reconciliation; timeout,
lost/malformed success, generic 422 and GET absence cannot authorize fallback.
Production classifier2 admits only the exact owner-approved PR2/base/head/actor/
profile2 recorded contract with sealed source and provider-patch validation.
Older witnesses cannot acquire fallback authorization from that new contract.

A remote permalink is untrusted content. Same-authority canonical parsing plus
sealed exact-source validation grants a presentation anchor only. It cannot
start a network read, import history, acquire credentials or publish a comment.
The [round-trip contract](../architecture/review-round-trip.md#immutable-permalink-codec-bpr-0564)
owns URL/body bounds; the [RFC-0022 ledger](../verification/RFC-BPR-0022.md)
separates fixture recovery/isolation evidence, actual designated fallback and
GitHub-only acceptance, explicit owner amendments, and unobserved platform/
installed-removal qualification.
