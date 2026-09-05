---
schema: "implement-rfc.card.v1"
id: "BPR-0310"
title: "Verify device authorization on target-native Linux"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0282","BPR-0318"]
---

# Verify device authorization on target-native Linux

## Required final state

- A clean supported Linux target builds the registered tests-disabled Release
  path and runs the focused native HTTPS, vault/keyring, host polling/refresh,
  Qt presentation, zero-request, dependency, runtime, and secret-scan matrix.
- System trust, proxy, system-browser handoff, cancellation/shutdown, credential
  storage/rotation, and Release selection are target-native observations rather
  than macOS, cross-compilation, containers without the desktop services, mocks,
  or hosted workflows.
- Device capability stays unavailable on Linux until this card is terminal.
- After all target-native rows pass, this card owns the Linux production-
  selection flip in the same commit and reruns the complete Release/public/
  native/hygiene matrix against the enabled artifact before becoming terminal.
- The one-time flip record and the idempotent final-head course are distinct.
  BPR-0285 reruns the exact platform-course interface against already-enabled
  Linux selection after every required card is terminal.

## Host note — 2026-08-22

The native Secret Service fixtures do not currently pass on the Linux host, so
this card's vault/keyring rows cannot be taken as available without first
resolving that.

Observed while running [[BPR-0443]]. `linux_secret_service_native_setup`,
`_native` and `_native_cleanup` fail; `linux_secret_service_contract` passes.
Proven pre-existing rather than caused by that card's edits: `git stash`,
rebuild, and the same three fail identically at unmodified `HEAD`.

[[BPR-0156]] recorded all four passing on 2026-08-17 against a real
gnome-keyring 50.0, so this is a change in host state rather than a claim that
was never true. `org.freedesktop.secrets` is owned by `gnome-keyring-d` and
`~/.local/share/keyrings/login.keyring` exists, so the service is running; the
likely cause is that nothing unlocked the collection in this session and the
fixture refuses prompts by construction. Not diagnosed further here — it
belongs to whoever runs this card's native course.

## Pre-flip platform course green; the keyring rows named — 2026-09-02

Produced by `bpr-linux` on the JaegAW WSL2 host at `6fff0a53`, read-only,
and recorded here by the integrator. No flip was taken.

**The platform course passes.** `python3
scripts/verify_rfc0016_platform_course.py --platform linux --dev-preset dev
--release-preset app-release --build-root build/rfc0016-linux-enablement
--expected-device-selection unavailable --fresh` exited 0 in 235 s with
`RFC0016_COURSE_SELECTORS:16`, `RFC0016_DEVICE_STAGE:unavailable`,
`RFC0016_DEVICE_REQUESTS:0`, and `RFC0016_PLATFORM_COURSE_OK`. All eleven
stages passed in order; the sixteen selectors were enumerated from the
course's own Debug tree. The station re-derived the surface facts against the
shipped artifact (ELF x86-64 PIE, 10,945,016 bytes): `linux_secret_service`
present (99 symbol-table occurrences), `macos_keychain`,
`windows_credential_manager`, `QWebEngine`, `QWebView`, `QtWebEngine` all
absent. Zero requests from the shipped Release artifact is the "stays
unavailable until terminal" row.

**The Secret Service native rows are blocked by host state, now named.**
`linux_secret_service_contract` passes; `_native_setup` and
`_native_cleanup` fail and `_native` is not run, exactly the 2026-08-22 host
note. Over the session bus, `ReadAlias default` resolves to
`/org/freedesktop/secrets/collection/login`, whose `Locked` property is
`true`; the unlocked `session` collection is gnome-keyring's ephemeral one and
not what the vault targets. The vault resolves `SECRET_COLLECTION_DEFAULT`,
returns `github_credential_vault_failure::locked` when the collection is
locked, and installs a non-prompting service that dismisses every unlock
prompt — deliberate, and what makes the fixture non-interactive. Positive
proof: the native test binary's `--expect-locked` mode passes on the host
right now. Nothing unlocks the collection at login because
`pam_gnome_keyring.so` appears only in `common-password`, and both daemons
were started without `--unlock`; `login.keyring` is dated 2026-08-18, the day
after [[BPR-0156]] recorded the four passing.

**Operator action, not taken by the station.** The `login` collection must be
unlocked in the session the tests run in: either `gnome-keyring-daemon
--unlock` with the session bus address set (per boot), or adding
`pam_gnome_keyring.so` to the `auth` and `session` PAM stacks (durable,
root-owned). Re-aliasing `default` to the in-memory `session` collection
would turn the rows green while invalidating them and is recommended
against. Once unlocked, all four fixtures are expected to pass without a code
change; that is the observation this card's flip waits on.

**For the flip envelope.** The one-shot unlock is per daemon, not per run:
the collection re-locks whenever gnome-keyring restarts, so the flip's rerun
must read `Locked` on `/org/freedesktop/secrets/collection/login` as its
first row, immediately before the course starts, or green rows can quietly
stop being reproducible. The course's `--fresh` rebuilds both trees from
scratch (about four minutes on this host), so a warm tree buys nothing.
## Terminal — the keyring resolved, the flip taken, both courses green — 2026-09-02

Produced by `bpr-linux` on the JaegAW WSL2 host (Ubuntu 26.04, kernel
6.18.33.2-microsoft-standard-WSL2 x86_64, GCC 15.2.0, Qt 6.10.3) at
`52c5a4ef`, which is the commit this slice's own flip sits on top of.

### The operator's keyring resolution supersedes this card's operator-action text

The 2026-09-02 "Operator action, not taken by the station" paragraph above is
now history and is kept only as the record of what was asked for. The operator
resolved it differently and better: the old `login.keyring` was retired because
its password was unknown, a new `login` collection was created, and a systemd
user override at
`~/.config/systemd/user/gnome-keyring-daemon.service.d/unlock.conf` passes
`--unlock` with a password file on stdin so the collection unlocks at every
session start. Three things this card had not established made the simpler
routes wrong: gnome-keyring 50.0 rejects an empty keyring password, so the
"empty password auto-unlocks" shortcut was never available; `--unlock` on its
own does not reach an already-running daemon, so a one-shot invocation could
not have fixed the live session; and the daemon here is owned by systemd
socket activation rather than started from a login shell, which is why the fix
belongs in a unit override and not in a PAM stack or a per-boot command. The
operator verified it cold over D-Bus — `CreateItem` with prompt `"/"`,
`Locked` false, surviving a service restart — before handing the rows back.
The per-boot caveat this card recorded for the flip envelope no longer
applies: the override makes the unlock a property of the session, not of a
command someone must remember.

### Row 1 — the collection read as unlocked, before anything else ran

| When (UTC) | Read | Value |
|---|---|---|
| 2026-09-02T04:29:01Z | `Locked` on `/org/freedesktop/secrets/collection/login` | `(<false>,)` |
| 2026-09-02T04:29:37Z | the same, immediately after the fixtures | `(<false>,)` |

`ReadAlias default` still resolves to
`/org/freedesktop/secrets/collection/login`, so the collection the vault
targets is the collection that was read. Both reads were taken with the
station's own `gdbus` command from the 2026-09-02 diagnosis.

### Row 2 — the vault/keyring rows, target-native

`ctest --preset dev -R 'linux_secret_service' --output-on-failure`, exit 0,
**4/4 passed, 0.47 s total**:

| Test | Result | Time |
|---|---|---|
| `linux_secret_service_contract` | Passed | 0.01 sec |
| `linux_secret_service_native_setup` | Passed | 0.02 sec |
| `linux_secret_service_native` | Passed | 0.42 sec |
| `linux_secret_service_native_cleanup` | Passed | 0.02 sec |

This closes the 2026-08-22 host note. No code changed to make them pass, which
is what the 2026-09-02 diagnosis predicted: the only refusal the vault raised
was the one the lock caused.

### Row 3 — the pre-flip course at the merged head

The ledger's exact Linux command with `--expected-device-selection
unavailable --fresh`, exit 0 in 171 s: `RFC0016_COURSE_SELECTORS:16`,
`RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`,
`RFC0016_PLATFORM_COURSE_OK`. Release artifact 10,991,504 bytes, sha256
`474cfb0d3f8af041f74f77cc2a93c660e7f827510aa922141d99739c38b644ba`.

### The flip

Two coordinated edits in this commit, as `bpr-win` established for Windows.

`src/app/src/github_application.cpp` — `shipped_device_authorization_capability`
becomes `#if defined(__APPLE__) || defined(__linux__)`. This constant is the
only thing that enables the capability.

`tests/end_to_end/CMakeLists.txt` — the dev-tree selector expectation's
`if(APPLE)` gate becomes `if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")`.
The Linux value was **measured, not carried across from macOS**. Under the
course's isolation the shipped probe answers:

```text
DEVICE_FLOW_HOST_COMPOSED:true
DEVICE_FLOW_PRESENTED:true
DEVICE_FLOW_STAGE:failed
DEVICE_FLOW_REQUESTS:0
DEVICE_FLOW_REGISTERED_SELECTION_OK
```

Same terminal as macOS, different mechanism, which is why it had to be
measured. macOS denies the connect through `sandbox-exec`. Linux runs the test
under `unshare(CLONE_NEWUSER | CLONE_NEWNET)`
(`tests/network_isolation/linux_network_sandbox.cpp`) in a namespace whose only
interface is loopback — `ip -o addr` inside it shows `lo` with `127.0.0.1/8`
and `::1/128` and nothing else — so the first outbound attempt has no route out
of the namespace. It lands on `failed` in 0.15 s against a 10,000 ms budget,
three runs identical: an immediate refusal, not a budget that expired.
`DEVICE_FLOW_REQUESTS:0` holds either way, registered or not.

Two digest pins moved with the `src/app` edit and are repinned in the same
commit, the repair [[BPR-0510]] established for exactly this class:
`bpr_expected_full_application_digest`
`1be908db…` → `a1ab5c4d6a712fe8d9069e3242a4a32a8924d9438d4ea159cc25736572456688`,
and `bpr_expected_production_app_manifest_digest`
`ae1a89c1…` → `b86ebdbb35b6ebc746e170cc6c7ed2df2e9fbded8005685d49c705b64a507ea6`.
The second was again visible only once the first was repinned.

### Row 4 — the enabled course against the flipped artifact

`build/rfc0016-linux-enablement` was deleted whole first. The same command
with `--expected-device-selection enabled --fresh`, exit 0 in 242 s:
`RFC0016_COURSE_SELECTORS:16`, `RFC0016_DEVICE_STAGE:cancelled`,
`RFC0016_DEVICE_REQUESTS:0`, `RFC0016_PLATFORM_COURSE_OK`. Release artifact
10,991,504 bytes, sha256
`d6e21290975230803957b57b1ec23382d4de1e5499e434a41d22171f2794aca6`.

The two courses' artifacts are the same size and different content, which is
what a one-constant change should produce. The enabled course reports
`cancelled` where the dev-tree selector expectation above reports `failed`
because they are different contexts, not a contradiction: the course runs the
shipped binary directly and the run's own shutdown ends the started flow, while
the dev-tree test runs it inside the network namespace where the transport
fails first. `DEVICE_FLOW_REQUESTS:0` is the invariant both hold, and it is the
one the RFC's headline claim rests on — the enabled artifact still sent
nothing.

### Row 5 — the full matrix at the flipped head

`ctest --preset dev --output-on-failure`, exit 0: **223/223 passed, no
failures**, 994.92 s (`bpr_network_isolated` 994.85 s*proc over 223 tests,
`bpr_loopback_fixture` 43.46 s*proc over 6). The three Secret Service fixtures
that had failed on this host since 2026-08-22 are inside that green run, as are
both digest-pinned composition contracts. `python3
scripts/verify_documentation.py --root .` reports `DOCUMENTATION_OK` and
`git diff --check` is clean.

This card is terminal. [[BPR-0285]] owns the idempotent final-head replay of
the same platform-course interface against the now-enabled Linux selection,
once every required card is terminal; [[BPR-0311]] still owns Windows.

## Correction — 2026-09-03

The independent review of the RFC-BPR-0016 closeout head (BPR-0285, V4)
found this card's reading of `DEVICE_FLOW_REQUESTS:0` on the enabled course
wrong: the marker prints the unregistered refusing transport's `reached()`
count, which a registered build never references, so it is the negative
control that no enabled build fell back to refusing and not a request
count. The enabled course's `cancelled` stage is reachable only after
GitHub minted a device code, so the Linux enabled course did send one
device-code request and cancelled it, as every enabled course of this RFC
does. The course rows above stand; only the sentence that said the enabled
artifact "still sent nothing" is withdrawn.
