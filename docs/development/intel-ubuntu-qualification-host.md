# Intel Mac Ubuntu qualification host

Provisioned 2026-11-15 under mandate `release-prep-20261115-r2`. This supplies
an independent Linux execution host for release preparation alongside the Windows
PC. It supplies no product, desktop, installer, vault, performance or release
acceptance. No signing material or authenticated account was transferred.

## Observed installation

| Item | Observed value |
|---|---|
| Physical host | `intel-mbp`, MacBookPro15,1, native x86_64, macOS 15.8.1, 32 GiB RAM, 12 logical CPUs, `kern.hv_support=1` |
| Hypervisor | Lima 2.3.0, Apple's VZ hardware virtualization; no emulated CPU or WSL |
| Guest | Ubuntu 26.04.1 LTS x86_64, Linux `7.0.0-43-generic`, glibc 2.43 |
| Resources | 6 vCPUs, 16 GiB assigned RAM, 150 GiB sparse disk; approximately 145 GiB ext4 root filesystem |
| Isolation | No host mounts, no containerd, no SSH agent forwarding or host key import; guest account `bpr` has guest-only passwordless sudo |
| Lima home | `/Users/jaeger/.bpr-q/lima-20261115` on the Intel Mac |
| Tools and receipts | `/Users/jaeger/.local/share/bpr-qualification/v0.5.0-20261115` on the Intel Mac |
| Source | `/home/bpr/src/BetterPullReview` in the guest, clean shallow checkout of `47ce791012fcd3c056ee41ed3ed50a06ff92bec2`, with no remote |
| Evidence | `/home/bpr/evidence/release-prep-20261115` in the guest; copied into the external continuation below |

The [official Lima installation instructions](https://lima-vm.io/docs/installation/)
allow a user-local binary installation. The [VZ backend](https://lima-vm.io/docs/config/vmtype/vz/)
uses Apple's native virtualization and runs a matching guest architecture.
The official Lima 2.3.0 Darwin x86_64 archive matched its published release checksum:
`d58bc7d6b8158d638720d8c8fdb17eb58527dfead6a29a5b60f48a35ee4a65a6`.
Lima verified the [official Ubuntu 26.04 amd64 cloud image](https://cloud-images.ubuntu.com/releases/resolute/release/)
against its downloaded `SHA256SUMS` entry:
`7b90593f5532b49b7e3fe3d2743f0d1f0d822e5b07cebcd8e00c0df8069ecab9`.
The receipts retain the exact URLs and configuration. The host was neither
repartitioned nor erased, and no host administrator password was needed.

The initial longer Lima home hit macOS's Unix socket path limit before a VM
was created. The shorter home above is the working installation. The source
transfer's macOS AppleDouble metadata was removed before verifying a clean
guest checkout. MASK's canonical public tap then fetched the exact committed
locks without credentials; a local transport rewrite was refused and removed.
The normal register preview, matching-plan application and readiness check
passed. No model or managed remote station was launched.

## Start and dispatch

Use the existing pinned SSH alias `intel-mbp`; do not bypass a host-key refusal
on a different hostname. The VM is stopped at handoff. Start it with:

```sh
ssh intel-mbp 'env LIMA_HOME=/Users/jaeger/.bpr-q/lima-20261115 /Users/jaeger/.local/share/bpr-qualification/v0.5.0-20261115/tools/lima-2.3.0/bin/limactl start --tty=false ubuntu26'
```

The external continuation contains `intel_guest.py`, a command-only bridge
through that alias and Lima. It preserves the actual remote exit and both
streams, and supplies the guest's pinned tool PATH. For example:

```sh
python3 /Users/jaeger/.codex/release-preparation/v0.5.0-20261115/intel-ubuntu/intel_guest.py cmake --version
```

An equivalent direct dispatch is:

```sh
ssh intel-mbp 'env LIMA_HOME=/Users/jaeger/.bpr-q/lima-20261115 /Users/jaeger/.local/share/bpr-qualification/v0.5.0-20261115/tools/lima-2.3.0/bin/limactl shell --workdir=/home/bpr/src/BetterPullReview ubuntu26 env PATH=/home/bpr/tools/build-venv/bin:/home/bpr/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin cmake --version'
```

Stop only this VM with:

```sh
ssh intel-mbp 'env LIMA_HOME=/Users/jaeger/.bpr-q/lima-20261115 /Users/jaeger/.local/share/bpr-qualification/v0.5.0-20261115/tools/lima-2.3.0/bin/limactl stop ubuntu26'
```

Ordinary guest service-port forwarding is disabled; Lima's own SSH/vsock route
remains usable. A future managed station needs its own registry row and the
pinned remote-station skill's launch/ownership procedure before launch. These
command-only checks confer no native model/station readiness.

## Toolchain and focused observations

CMake **4.4.2** is installed in `/home/bpr/tools/build-venv`; Ninja is 1.13.2,
GCC/G++ 15.2.0, Python 3.14.4 and Git 2.53.0. Ubuntu provides
clang-format/tidy **22.1.2**: the required format major is present, but the
build guide's exact 22.1.8 declaration is not claimed. The project Qt 6.12.0
provider/cache has not been acquired or qualified on this guest.

Native development/runtime prerequisites include libsecret 0.21.7, XCB 1.17,
X11/OpenGL/font libraries, `patchelf`, binutils, dpkg tools, ccache, Xvfb,
Xauth, D-Bus and gnome-keyring 50. The actual observations were:

- CMake/Ninja configured, built and ran a standalone C++23 executable using
  `std::expected` and ranges under GCC with warnings as errors.
- Xvfb accepted an actual Unix display-socket connection.
- A private D-Bus session exposed `org.freedesktop.secrets` from a test-only
  gnome-keyring process and answered a Peer Ping. Its data and runtime roots
  were disposable and its exact child was stopped. This checked prerequisites,
  without storing credentials or qualifying the product vault.
- Dispatch retained harmless stdout/stderr and exits **0** and **17**. The
  latter was intentional and remains a failed-command receipt in the ledger.
- The three cheap tooling checks passed. The runner contract's **38 tests**
  passed, including mandatory risk rows, failure-first collect-all and accounting
  controls. The release entrypoint contract's **36 tests** passed with **two
  native-macOS-only skips**, which receive no Linux coverage credit.
- Source-only `normal` plus `qt` scope correctly selected `comprehensive`;
  source-only `risk-preflight` admission also passed. Both explicitly reported
  `qualification_started: false`.

The phase ledger preserves commands, source/tool context, actual exits and raw
streams, including failures. Its accounting snapshot has no pending phase or
wait and grants no qualification credit. This is setup/rehearsal evidence,
separate from the next release's request clock or two-hour target.

## Test network-isolation launcher and AppArmor

Ubuntu 26.04 sets `kernel.apparmor_restrict_unprivileged_userns=1`. Under it
the tests' Linux network-isolation launcher,
`build/<preset>/better_pull_review_network_sandbox`, cannot map its identity
into the user namespace it creates, and every contained test fails with
`BPR_NETWORK_ISOLATION_UNAVAILABLE:identity-map:Permission denied`. On
2026-11-17 this stopped both risk preflights at their tooling prerequisites
before any risk row ran.

The owner chose a narrow profile over turning the restriction off. The
sysctl stays at `1`; only the launcher in this guest's build trees may create
user namespaces. Install it once in the guest:

```sh
sudo install -m 0644 -o root -g root bpr-network-sandbox /etc/apparmor.d/bpr-network-sandbox
sudo apparmor_parser -r /etc/apparmor.d/bpr-network-sandbox
```

`/etc/apparmor.d/bpr-network-sandbox`:

```text
# BetterPullReview test network-isolation launcher (qualification guest only).
# Grants user namespaces to the build trees' launcher; the global
# kernel.apparmor_restrict_unprivileged_userns=1 restriction stays on.
abi <abi/4.0>,
include <tunables/global>

profile bpr-network-sandbox /home/bpr/src/BetterPullReview/build/*/better_pull_review_network_sandbox flags=(unconfined) {
  userns,

  include if exists <local/bpr-network-sandbox>
}
```

The profile persists in `/etc/apparmor.d` and loads at boot. Check it with
`sudo aa-status | grep bpr-network-sandbox`, then confirm both sides:
`build/dev/better_pull_review_network_sandbox /bin/true` prints
`TEST_RUNNER_CHILD_EXIT:0`, while a plain `unshare -Urn true` is still refused
with `write failed /proc/self/uid_map: Operation not permitted`. The pattern
covers every tree directly under `build/`; a launcher built elsewhere stays
refused.

## Rejected source and continuation

The cheap public-composition tripwire **failed** on the exact clean `47ce7910`
candidate on both macOS and Ubuntu:

```sh
python3 tests/end_to_end/composition_contract_tests.py --cmake cmake CompositionContractTests.test_async_mutation_host_inventory_rejects_every_direct_bypass
```

The explicit app inventory omits
`src/github_application/review_agent_stack.cpp` and `.hpp`. Both are already
committed product composition from `32946c58e` and would also need to join the
production manifest. The manifest's content pin is separately stale even
before those additions: expected `b8d10513…`, actual `7da759c8…`; the full
application and mutation-branch pins still match. Since the last contract
update `9bf38f689`, 25 app files changed across 19 feature commits. A two-path
inventory correction alone cannot restore the gate. Current main and the
Claude prerelease lane still named that same contract update at this check.

Preserve this failure for the feature/common-candidate source review and
reseal; do not merely replace the hash. No product configure/build or native
risk suite followed the rejection. After the accepted common candidate is
transferred and its cheap checks pass, compile Release tests, Debug tests and
the tests-disabled distribution graph on this physical host, then run **both**
nine-row risk preflights before long qualification. Retain failure-first
collect-all, current comprehensive obligations and the standing Debug timing
policy. The Intel Mac can advance independently from the Windows PC; Windows
and WSL on that PC still share one serialized host lease. Signing remains on
the approved producer; no signing credential moves to this guest.

External continuation:
`/Users/jaeger/.codex/release-preparation/v0.5.0-20261115/intel-ubuntu`.
Its operational milestone plan is bound to the provisioning mandate, not a
live card selector; it closes no existing card/RFC acceptance. The setup
record, installer source and previous handoff require explicit main integration
after tracker-writer reconciliation. Windows MSI/ICE/schema and ordinary-user
scope courses can proceed as the next independent milestone once its native
host lease is reconciled. Linux CLI, UUID-less diagnostic, latency, desktop,
installed vault and final package/trust criteria retain their separate inputs
and unresolved obligations.
