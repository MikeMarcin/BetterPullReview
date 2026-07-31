---
id: BPR-0029
title: Wire the RFC-BPR-0002 public application path
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0027, BPR-0028]
completed: 2026-07-30
---

# Wire the RFC-BPR-0002 public application path

Select explicit host profiles, transport, credential adapter, provider facade,
application host, core transitions, and Qt projection in the composition root.
Prove a read-only fixture PR traverses the real QAction-to-provider-to-rendered
application path for GitHub.com and GHES 3.18.

The composition root is the only layer allowed to select concrete transports
and platform credential adapters.

Native credential persistence must not be composed until the public path can
recover its exact profile/profile-revision/host/hostname/account/server
authority after process restart without accepting a caller-supplied account
selector. Provide either a vault-provenance or integrity-backed durable
binding-rehydration boundary, or exact-delete every retained credential before
normal shutdown completes. Settings, SQLite, login text, profile text, and an
untrusted raw account value cannot seed that selector. If neither design is
available, native credential persistence remains unavailable even though
BPR-0027 proves same-process host recreation through a shared host-owned
catalog.

The composition must also keep the decision executor alive and accepting
posts until the application host is quiescent, and keep the credential
binding catalog, provider, transports, vault, and projections alive for the
lifetimes documented by their ports.

## Result

- Added an explicit multi-profile public composition. GitHub.com profiles have
  built-in provenance; an optional canonical GHES 3.18 hostname creates
  explicit-user-provenance profiles. The provider owns immutable validated
  profile copies and routes by exact profile identity.
- Selected native HTTPS for the usable GitHub.com and GHES paths, the target
  platform's native vault, separate provider and vault workers, the normalized
  provider, serialized application host, Qt event-loop decision lane, and
  provider-free GitHub window.
- Kept GitHub CLI choices visible and labeled unavailable. Selection reaches
  the public semantic path, reports bounded `cli_unavailable`, and starts zero
  processes while BPR-0037 retains the trust and identity contract.
- Added a Qt-owned password-echo credential prompt with character and UTF-8
  byte bounds, move-only sensitive storage, and explicit clearing. Composition
  converts it immediately to application credential material; Qt receives no
  vault or provider capability.
- Bound profiles, provider routing, transport preflight, credential authority,
  resume comparison, and platform-vault keys to the canonical hostname.
  Current vault namespaces use the disjoint 574-character v2 encoding and
  never read, enumerate, migrate, or fall back to historical v1.
- Selected `delete_retained_on_shutdown` for the public host. Exact deletion
  is part of quiescence; composition keeps the decision lane, vault, catalog,
  provider, and workers alive through its terminal, then closes and joins both
  workers. A failure remains visible and retains the exact tombstone.
- Made no durable restart claim. The catalog is process-local, and an abrupt
  process or host crash can leave a native-vault item that BetterPullReview
  cannot enumerate or resume.
- Added deterministic GitHub.com and GHES 3.18 public fixtures that traverse a
  real QAction, provider, host staging, core acceptance, incremental Qt
  projection, rendered window, and exact shutdown deletion.
- Kept every fixture-only type, source, path, argument, label, and evidence
  string outside tests-disabled Release builds. GitHub Actions has zero build,
  test, package, release, or verification role.

## Evidence

- Both macOS recorded public fixtures reported `PUBLIC_PATH_OK`, their exact
  GitHub.com/GHES fixture marker, and `RENDER_OK`. Their structural evidence
  confirmed complete recording and capability consumption, separate bootstrap
  and account transport selection, credential store/retrieve/delete, and no
  retained secret.
- The public CLI self-test showed the unavailable diagnostic and completed
  with zero process launches.
- A fresh macOS Debug Qt build passed 43/43, and a fresh `core-only` build
  passed 17/17. AddressSanitizer/UndefinedBehaviorSanitizer matrices passed
  12/12 for core/application and 11/11 for the Qt-focused group.
- A fresh tests-disabled Release self-test reported `PUBLIC_PATH_OK` and
  `NATIVE_PLATFORM_VAULT_SELECTED:true`. Dependency and runtime audits passed;
  the binary selected Security/CoreFoundation and imported
  `SecItemAdd`/`SecItemCopyMatching`/`SecItemUpdate`/`SecItemDelete`.
- The Release composition contract passed and independently proved the public
  header, compile commands, build graph, symbols, and raw binary contained no
  test conditional, fixture translation unit, argument/status text, recorded
  label/identity, path, or fixture-access seam.
- Both v1 and v2 integration Keychain services were clean after exact
  lifecycle cleanup. The skeptical closeout audit found no P0/P1 defect.
- In an Ubuntu 26.04 x86_64 full-system QEMU guest, a fresh tests-on Release
  build completed 81/81 targets. All 40 non-native registered test identities
  have passing current-tree evidence after the repaired dependency and
  composition gates.
- The current v2 Secret Service setup/lifecycle/cleanup cases passed 3/3.
  Independent exact cleanup and the hostile, unavailable, and locked cases
  also passed.
- Both Linux public fixtures passed with the exact GitHub.com/GHES host labels
  and rendered state. A fresh tests-off Release build completed 36/36 targets,
  the fixture seam was absent, the self-test reported `PUBLIC_PATH_OK` and
  `NATIVE_PLATFORM_VAULT_SELECTED:true`, and dependency, runtime, and ELF
  audits passed. The tests-off executable SHA-256 is
  `78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`.
- Windows 10/11 x64 target-native verification remains BPR-0036 and is
  nonblocking for later implementation; it blocks only affected Windows
  claims and final RFC-BPR-0002 Auto-Verified closeout.
- Deterministic offscreen rasters are layout/composition evidence, not final
  visual polish or native system-theme certification. BPR-0031 retains
  hierarchy, density, typography, native light/dark, and live appearance
  verification.

## Post-completion scope resolution — 2026-07-30

BPR-0037 closed the milestone-one CLI decision: the public application keeps
both CLI choices visible and fixed at `cli_unavailable`, while native HTTPS is
the sole production authentication transport. The tests-disabled
`--self-test` now emits `CLI_UNAVAILABLE_OK`, and the Release composition
contract rejects CLI transport, request-compiler, configuration-validator, or
process-runner symbols in the executable. Positive CLI enablement is deferred
to RFC-BPR-0015 and is not an RFC-BPR-0002 closeout requirement.
