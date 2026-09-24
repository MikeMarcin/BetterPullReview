---
schema: "implement-rfc.card.v1"
id: "BPR-1113"
title: "Linux activation: fall through an over-long XDG_RUNTIME_DIR, and pin the symlink-cleanup and other-owner cases"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-0906 (claude-fable-5-1, read-only, macOS from source), findings 1, 3, 4 and 6, with 2, 5 and 7 as documentation","reviewed_candidate":"03117e63","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision 96421f40-a5d6-4cbf-90b4-e855e8650f8b, 2026-09-24","related_cards":["BPR-0906","BPR-0881","BPR-0895"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1113: Linux activation: fall through an over-long XDG_RUNTIME_DIR, and pin the symlink-cleanup and other-owner cases

## Source

[[BPR-0906]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `96421f40`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `03117e63`. The review found no blocking
issue. It confirmed that the launcher only ever deletes its own `mkdtemp`
result, and that the product takes an `XDG_RUNTIME_DIR` only when the
directory is owned by `geteuid()` with mode `0700`. This card takes findings
1, 3, 4 and 6 as work, and findings 2, 5 and 7 as documentation. The review
is recorded on BPR-0906 under "Independent review (2026-09-24)".

Line references are the reviewer's, and the files are unchanged from
`03117e63` to the revision where this card was written. The product file is
`src/adapters/sqlite/src/profile_activation.cpp`. The launcher is
`tests/network_isolation/linux_network_sandbox.cpp`.

## Work

**1. An over-long private `XDG_RUNTIME_DIR` now fails instead of falling
through.** The product takes a private `XDG_RUNTIME_DIR` first
(`profile_activation.cpp:185-187`) and then checks the endpoint's length
(`:222`). The endpoint name adds 35 bytes, so on Linux a private directory of
73 bytes or more fails with `endpoint-path-too-long`. Before BPR-0906, such
a directory was never a candidate, and `/run/user/<uid>` or the next
candidate was used. The reviewer's fix is to skip the XDG candidate when its
size plus 35 is at least `sizeof(sun_path)`.

**3. The AC2 row does not catch a cleanup that follows symlinks.** The
runtime child (`tests/scripts/network_isolation_counterexamples.py:214-230`)
leaves a socket and a directory, but no symlink. A removal that followed
links would still pass the row. The reviewer suggests adding escape
symlinks: links in the child's runtime directory that point outside it.

**4. The other-owner claim is not pinned.** The Linux case in
`tests/sqlite_contract/profile_activation_tests.cpp:350-396` covers a
private, a widened and a relative `XDG_RUNTIME_DIR`. It has no case for a
directory owned by another user. The reviewer suggests
`XDG_RUNTIME_DIR=/root` or `/var/lib/private`.

**6. The launcher has small `/tmp` leak windows.** They are at
`linux_network_sandbox.cpp:328-331` and `:335`. A signal that arrives between
`mkdtemp` (`:284`) and the handlers installed at `:328` also leaves the
directory behind.

## Documentation

**2. Two launches with different private `XDG_RUNTIME_DIR`s no longer meet.**
Each resolves its own endpoint, so the second launch degrades to the lease's
`owner_busy` path. That is the symptom [[BPR-0881]] fixed. The reviewer
judges it intended, but BPR-0906 does not say so.

**5. The bus pin can send the Secret Service rows to the real bus.** On a
host with no `DBUS_SESSION_BUS_ADDRESS` but a live `$XDG_RUNTIME_DIR/bus`,
`pin_session_bus` (`linux_network_sandbox.cpp:138-151`) sets the address. The
Secret Service rows then run against the real bus
(`tests/github_contract/linux_secret_service_native_tests.cpp:1288-1291`).
BPR-0906's "Not observed on this station" says the Secret Service rows were
not run with the address unset. It does not say that they would then reach
the real bus.

**7. "Without following links" is inaccurate.** It holds only for the
`bpr-<uid>` child, which uses `lstat` (`profile_activation.cpp:197`). The
configured `XDG_RUNTIME_DIR` and the top-level candidates are checked with
`stat` (`:185`, `:193`). BPR-0906's scope repeats the phrase from BPR-0881's
guarantee.

## Scope

- Skip the XDG candidate when the endpoint would not fit, so the next
  candidate is used as it was before BPR-0906.
- Add escape symlinks to the runtime child, both a file link and a directory
  link that point outside the runtime directory. Assert that their targets
  survive the launcher's removal.
- Add an other-owner case to the Linux profile activation contract. When the
  host has no such directory, the case must say that it was skipped, never
  pass silently.
- Close or bound the launcher's leak windows. Two ways are to block the
  forwarded signals from `mkdtemp` until the handlers are installed, and to
  remove the directory on each early `fail(...)` path. If closing a window
  is not worth it, record why.
- Say what 2, 5 and 7 mean in live documentation. Two places describe this
  today: `docs/development/build-and-test.md` covers the launcher's runtime
  directory, and the header comment in
  `src/adapters/sqlite/include/bpr/adapters/sqlite/profile_activation.hpp`
  covers the runtime directory the product resolves. The Done cards keep
  their text, and this card's closeout points at the correction.

## Acceptance

- AC1 A private `XDG_RUNTIME_DIR` too long for the endpoint is passed over,
  and the endpoint resolves to the next candidate. `sqlite_profile_activation`
  pins it, and it fails on the unchanged product.
- AC2 The runtime row plants escape symlinks. Their targets are unchanged
  after the launcher removes the row's directory. A trial that removes the
  directory by following links fails the row.
- AC3 A directory owned by another user is passed over as `XDG_RUNTIME_DIR`,
  and a row pins it. A trial that drops the owner check fails it.
- AC4 The launcher leaves no `/tmp/bpr-run-*` behind on the paths the review
  names, or the card records why a window stays open.
- AC5 Findings 2, 5 and 7 are stated in live documentation.
- AC6 No regression: every `test_network_isolation_*` row,
  `sqlite_profile_activation`, the profile activation and owner admission rows,
  `application_attended_launch_contract`, the three Linux Secret Service rows,
  `msvc_secure_crt_contract`, and `verify_documentation.py`. If a row's
  registration changes, also run `test_network_isolation_manifest`.

Linux only. The product change is under `__linux__`, and the launcher is
Linux-only.
