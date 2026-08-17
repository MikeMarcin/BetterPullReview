---
id: BPR-0050
title: Verify RFC-BPR-0003 Git and cache security boundaries
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0047, BPR-0048, BPR-0055, BPR-0058, BPR-0060, BPR-0061, BPR-0062, BPR-0063, BPR-0064]
completed: 2026-08-01
---

# Verify RFC-BPR-0003 Git and cache security boundaries

Audit executable and helper identity, argv/environment/cwd/config isolation,
native-HTTPS credential and authority binding, smart-protocol request
admission, TLS/proxy/redirect behavior, process descendants, cancellation,
output/pack/disk/time bounds, cache permissions, path/symlink/junction races,
manifest/config/alternates/replace-object drift, ref and pkt-line injection,
catalog/ref transactions, crash recovery, and exact prune/delete scope.

Use malicious canaries to prove repository or user/system configuration cannot
run hooks, helpers, filters, LFS, submodules, textconv, external diff, SSH,
file/ext transports, editors, pagers, fsmonitor, traces, or worktrees.
Inspect retained evidence for credentials, private repository content, remote
URLs, raw provider/process errors, and broad deletion.

Complete macOS and available Linux target-native checks locally. Windows
execution remains BPR-0053 and is nonblocking for later implementation while
blocking only affected Windows claims and RFC closeout. GitHub Actions has no
role.

The fresh Ubuntu x86_64 GCC matrix exposed BPR-0060's actor-capture evaluation-
order defect. BPR-0060 is done in `a34d1b4` with direct GCC, Clang, sanitizer,
and architecture evidence, so this target-native security matrix has resumed.
The resumed 26-case GCC matrix then exposed BPR-0061's eager stale-preparation
context dereference. This card is temporarily blocked on that separately
tracked core remediation. BPR-0061 is done in `02ed3e8` with direct GCC,
Clang, sanitizer, core-only, and architecture evidence, so this matrix has
resumed again.
The first slow target-native group then exposed BPR-0062's stale independent
review database-ID expectation. This card is temporarily blocked on that
separately tracked contract remediation. BPR-0062 is done in `65d95e5` with
direct Clang, sanitizer, GCC, and architecture evidence, so the remaining
slow target-native matrix has resumed.
The refreshed installed-Git/cache group then exposed BPR-0063's dangling
canonical-OID view in the force-push fixture. This card is temporarily blocked
on that separately tracked contract remediation. BPR-0063 is done in
`d21f869` with direct Clang, sanitizer, GCC, Qt OID, Git/cache/maintenance,
and architecture evidence, so the remaining matrix has resumed.
The fresh tests-disabled GCC Release composition then exposed BPR-0064's
optimizer-only protocol parser bounds and expected-conversion warnings. This
card is temporarily blocked on that separately tracked remediation.
BPR-0064 is done in `1132789` with direct Clang, GCC Debug, fresh GCC Release,
sanitizer, tests-off production composition, dependency/runtime, architecture,
and capability-critical review evidence, so this security matrix has resumed.

## Result

The available-platform security matrix is complete on native macOS arm64 and
an Ubuntu 26.04 x86_64 full-system QEMU guest. Installed Git remains a fixed,
credential-free local object processor; native HTTPS owns bounded smart
protocol traffic; cache, refs, catalog, epoch identity, maintenance, and stale
completion authority remain exact; platform vaults fail closed; and the real
application retains no forbidden child-process, listener, fixture, CLI, or
secret-bearing production seam. The target-native Windows claims remain
unchanged in BPR-0053 and are not counted here.

## Evidence

- The Ubuntu guest uses Linux 7.0.0-28, GCC 15.2, CMake 4.2.3, Git 2.53, Qt
  6.10.2, libsecret 0.21.7, and gnome-keyring 50. Its fresh warnings-as-errors
  build completed 128 targets; the broad fast security matrix passed 26/26 in
  435.29 seconds. The later review/normalization group passed 8/8 in 14.23
  seconds, and the final installed-Git/protocol/Qt/cache/maintenance group
  passed 8/8 in 64.56 seconds after the BPR-0063 repair.
- After all remediations, the current Ubuntu public/runtime/render group
  passed 11/11 in 43.14 seconds: default and GHES CLI-unavailable public paths,
  worker execution, native-appearance contract, both recorded public paths,
  both two-process persistence paths, binary dependency surface, process and
  socket surface, and rendered application evidence. BPR-0064 separately
  records the fresh tests-off GCC Release composition and the tests-enabled
  GCC Release protocol/transport 4/4 run.
- The current Linux Secret Service v2 contract/setup/lifecycle/cleanup matrix
  passed 4/4 in 2.71 seconds inside an isolated Xvfb, session-D-Bus, and
  disposable unlocked gnome-keyring session. The adapter separately returned
  typed `unavailable` with no session bus and typed `locked` only after D-Bus
  reported the real default collection locked.
- A live hostile owner of `org.betterpullreview.Hostile.Secret` was present
  while the canonical `org.freedesktop.secrets` owner remained available. The
  dependency-redirection check passed; destination-scoped D-Bus monitoring
  observed exactly one `Properties.GetAll` and zero `OpenSession`, alias,
  search, create, secret-read, or delete methods. The disposable audit log and
  keyring home were removed with the session.
- Native macOS selected tests cover the same process, protocol, cache,
  catalog, maintenance, public composition, runtime, dependency, and security
  surfaces. Thirty-seven selected contract identities passed, with
  contention-sensitive cases rerun serially after concurrent host load. The
  current macOS Keychain v2 cleanup reported `macos-keychain-cleanup:pass`;
  the tests-disabled Release application rebuilt cleanly and passed its
  public self-test, dependency, and runtime audits. Focused ASan/UBSan groups
  and every remediation-specific sanitizer replay passed.
- Core and architecture boundaries passed 4/4 under macOS Clang and Ubuntu
  GCC; the GCC negative-compilation contract completed in 393.33 seconds.
  BPR-0060 through BPR-0064 retain the separately tracked failure,
  remediation, focused matrix, simplification, and review evidence exposed by
  this target-native run.
- The final retained-evidence scan finds zero tracked private-key filenames,
  secret-artifact filenames, private-key bodies, or GitHub token bodies. The
  sole credential-bearing URL form is the intentional
  `proxy.example.invalid` negative fixture. No credential, private provider
  body, or runtime canary is retained in checked-in evidence.
- Before teardown, a content manifest over all 240 tracked build/test inputs
  matched exactly between host and guest at
  `235315d78e8992a3c6c2677504570c1575241497aff40ff11af17cfc19ca5826`.
  The task-specific QEMU process was then stopped, port 22250 closed, and the
  SSH key, qcow/base images, guest build trees, and transient logs under the
  exact `build/bpr50-linux-vm` directory were permanently removed. The final
  workspace scan finds zero retained build credential or VM-image artifacts.
- The capability-critical independent review found the retained QEMU image,
  guest tree, SSH key, port forward, and overwritten guest logs blocked
  evidence closeout. The manifest binding and teardown above resolved both
  findings; the reviewer confirmed the disclosed GCC composition-wrapper
  timeout remains correctly non-passing and does not invalidate the separate
  direct production-artifact evidence. No high-risk implementation finding
  remained.
- The documentation validator and issue-graph tests pass; tracked and on-disk
  GitHub Actions workflow counts are both zero; and `git diff --check` passes.

## Residual limits

- The installed-Git policy trusts the selected supported Git executable while
  stripping credential, configuration, helper, transport, hook, and repository
  execution authority. It is not a kernel sandbox against a compromised Git
  binary.
- Process and socket audits combine exact admitted-child contracts, hostile
  canaries, provider/process counters, and live point-in-time runtime checks;
  they do not claim continuous operating-system telemetry over every smart
  fetch.
- Real Windows execution remains BPR-0053, and the owner-activated macOS focus
  transition remains BPR-0059. Neither is inferred from this available-target
  security result.
