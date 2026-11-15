# Release preparation, 2026-11-15

Mandate `release-prep-20261115-r2`: the owner says 0.5.0 is not ready for
release, asks to take over available release/platform work outside Claude's
current features, and amends the Windows installer to offer **Just me** or
**Everyone** at installation time. For the fresh-install default the owner
says, “Do whatever is the standard normal behavior.” The later amendment
includes the post-0.4.2 process improvements and setting up Ubuntu on the
Intel Mac for qualification in parallel with the Windows PC. No publication,
signing, release number allocation or desktop promotional capture is authorized.

Source baseline: `0f0f803d4b374d2b63a9de35b6995fb76dcd37c1`.
Engineering lane: `codex/release-platform-prep-20261115`, checkout
`/Users/jaeger/.codex/worktrees/3434/BetterPullReview`. The bounded first
[plan](../rfcs/plans/release-preparation-20261115/milestone-plan.json) covers
installer source work and separately pending native acceptance. This document
is an execution inventory, not acceptance of the other cards or a new ordering
gate for their existing plans.

## Work and ownership

All eleven cards were In Progress at takeover. No card is moved by this slice.

| Card | Existing evidence and remaining work | Ownership/disposition |
|---|---|---|
| [[BPR-1788]] installer | Prior everyone-only source checks; no native MSI evidence. Superseded scope choice is amended below. | This lane owns packaging source and focused checks. Native acceptance requires a reconciled Windows host and exact candidate. |
| [[BPR-1789]] console | Windowed desktop target and console CLI source checks exist. Native Explorer/Start launch and redirected diagnostics remain. | Reuse source; combine objective native evidence with the installer course after host reconciliation. |
| [[BPR-1791]] Windows/Linux CLI | Discovery/composition implemented; target-native first launch and CLI read evidence remains per platform. [[BPR-1857]] supplies a later Windows live Git-object read, not every first-launch criterion. | Reconcile actual card/course inputs before rerunning. Do not redo source by default. |
| [[BPR-1857]] corporate Windows | Native focused and BPR-Test comparison evidence recorded; corporate-machine named refusal/recovery AC3 remains. | Agent-owned work precedes a wizard for the owner's corporate machine. Its credentials/data are outside this lane. |
| [[BPR-1978]] Linux UUID-less | Original Linux49 canary failure remains; approved diagnostics nonreproduced, no product correction. | Preserve checkpoint, frozen producer and observer proposal. Previous `bpr1964-native-session-boundary` tree is clean on `codex/bpr1984-windows-tour-containment` at `d7154d5d0`; that is not proof of the historical Linux lease being released. |
| [[BPR-2133]] latency | Six original optimized Linux misses remain. Conditional 0.4.2 waiver was not a fix or 0.5.0 acceptance. | Preserve `codex/responsiveness-latency`, clean at `031d893d3`; no duplicate work or timing waiver. Reconcile its owner and current requirement before scheduling. |
| [[BPR-1676]] promo | Fixture, serving and capture tooling already exist; actual requested capture and owner acceptance remain. | Prepare/check tooling independently. Capture only after a specific request, through the promo skill. |
| [[BPR-2177]] fixture guard | Implemented guard courses, later fixture fixes; Claude's active prerelease lane is running related tests. | Excluded from source edits until Claude hands back the exact candidate and residual obligations. |
| [[BPR-2180]] loading shapes | Several human accepts and subsequent rejection/fixes; v003 frozen candidate/session remain. | Claude-owned live/attended surface; preserve candidate and decisions. |
| [[BPR-2181]] session queue | Host/runtime/SQLite changes still actively implemented. | Claude-owned; no edits or duplicate implementation. |
| [[BPR-2184]] Ask/reports review | Critical-review fixes actively implemented with the queue work. | Claude-owned; no edits or duplicate implementation. |

The other Windows readiness tree is clean at `7a64b03c0` on
`codex/bpr1958-windows-readiness`. Existing release-preflight, accounting and
future-release-workflow trees are preserved. Clean trees and stale station
metadata do not establish native command quiescence. Local process inspection
found active Claude prerelease tests and the shared jobserver; neither is owned
or stopped by this lane. The 3434 checkout's station status is unmanaged/local.

## Sequence after the installer source milestone

1. **Operational adoption and host reconciliation.** Reuse main's completed
   [[BPR-2057]] structured diagnostics/compile-first, [[BPR-2058]] failure-first
   collect-all, [[BPR-2100]] mandatory Release/Debug risk preflight and
   [[BPR-2103]] source-only request admission/two-hour accounting. Verify the
   maintained entrypoints and prepare a source-only request and durable phase
   record; Done does not prove a fresh native rehearsal. Preserve prior failures.
2. **Two independent native hosts.** The coordinator owns Intel Mac discovery
   and Ubuntu 26.04 x86_64 provisioning. The owner confirms the Intel Mac exists
   and Ubuntu is not installed. No ready Linux producer is claimed. Prefer a
   non-destructive VM under macOS; record actual hardware, access, virtualization
   support, resources and native GUI/vault prerequisites before qualification.
   Reconcile JaegAW Windows station identity, principal, prior task forests and
   physical-host lease before native work. Windows and WSL on JaegAW share one
   host and remain serialized; independently provisioned Intel Ubuntu may run
   in parallel with Windows after its own compilation prerequisites pass.
3. **Focused Windows installation/launch/CLI courses.** Use the exact committed
   installer source, native WiX validation and disposable install contexts.
   Exercise both installer scopes, same-scope old-package upgrades, repair,
   removal, console behavior and first-launch CLI offers. Keep corporate AC3
   and visual/accessibility judgments separate from automated evidence.
4. **Linux residual diagnosis and CLI courses.** Recover BPR-1978's actual
   source/image/failure and approved bounded-observer proposal before a new
   diagnostic. Do not use retries to turn nonreproduction into a fix. Reconcile
   the latency lane before scheduling BPR-2133. A new independent Ubuntu host
   supplies new evidence; it does not erase WSL's original failure.
5. **Feature intake and later qualification.** Intake Claude's accepted source
   after writer reconciliation. Prepare the common release delta and actual
   profile selection only then. Native trust, source/legal/security, signing,
   owner acceptance and publication remain their own obligations.

The coordinator's read-only Intel discovery reached the existing, pinned SSH
alias `intel-mbp`: MacBookPro15,1, x86_64 macOS 15.8.1, 32 GiB RAM,
12 logical CPUs, approximately 716 GiB free and `kern.hv_support=1`.
Homebrew exists at `/usr/local/bin/brew`; no hypervisor was discovered and
passwordless sudo is unavailable. The proposed next route is user-level Lima
with Apple's VZ backend, an Ubuntu 26.04 x86_64 image, a task-specific
`LIMA_HOME`, no host mounts/containerd and a bounded 6-CPU/16-GiB/150-GiB sparse
guest. These are prerequisites and a proposal, not an installed VM. The next
engineer should use the existing verified alias, preserve host data, verify
official release digests and finish actual boot/toolchain/GUI/vault readiness.

The historical external run root
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104` was not present at takeover.
Card summaries survive; absent raw evidence must be located or recovered before
claiming exact-candidate reuse. The board helper initially reported `NEXT_ID`
BPR-2189 below historical BPR-2190. This lane takes only the exact counter and
BPR-2189 reservation from main `caa5a1ff24b8fdf811863afe4700c18d490a2130`;
no IDs are allocated and no feature source is taken in. Further live-main
movement must be reconciled before later guarded board mutations.

## Installer design and acceptance boundary

Microsoft's [single-package authoring](https://learn.microsoft.com/en-us/windows/win32/msi/single-package-authoring)
recommends `ALLUSERS=2`, `MSIINSTALLPERUSER=1` for a fresh per-user default.
The native scope dialog appears before costing, so **Just me** uses the per-user
Programs known folder and **Everyone** uses Program Files and Windows' UAC
credential/consent flow. Both choices remain visible to an ordinary user.
Existing scope and directory are restored before costing; upgrades do not
change scope. To change scope, uninstall then reinstall. A dual-context MSI
does not promise upgrades across scopes or discovery of another user's copy.

Use the stable CPack/WiX InstallDir flow. WiX 5.0.2's
[Advanced source](https://github.com/wixtoolset/wix/blob/v5.0.2/src/ext/UI/wixlib/WixUI_Advanced.wxs)
declares active development and an unset application-folder TODO, so it is not
selected. CMake's generator accepts only per-user/per-machine component modes;
the generated template is narrowly adapted to WiX `perUserOrMachine`, while
the remembered root and the package-owned Start menu marker use `HKMU` to
follow the chosen context. Native MSI tables and ICE105 validation remain
required; configuration tests cannot replace them.

Durable external continuation:
`/Users/jaeger/.codex/release-preparation/v0.5.0-20261115/installer-scope`.
It retains selection, binding, exact commands/evidence and current handoff.
Authenticated review URLs are never stored here. No review-ready native
installer exists yet, so no installer human acceptance is requested by this
source milestone.
