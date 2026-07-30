---
id: BPR-0024
title: Implement and verify Linux Secret Service credentials
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0021]
completed: 2026-07-30
---

# Implement and verify Linux Secret Service credentials

Implement the native Linux Secret Service adapter and directly prove
store/retrieve/delete of a disposable canary credential on a native Linux
desktop session. Verify secret absence from settings, SQLite, arguments,
diagnostics, logs, and UI text and that cleanup succeeds. Exercise locked or
unavailable service, access denial, replacement, and delete failure; every
failure must remain explicit and must never fall back to settings,
environment, or a plaintext file.

GitHub Actions is not an acceptable evidence surface.

## Result

- Added a Linux-only libsecret adapter with fixed, disjoint production and
  integration schemas and one exact 66-character portable-authority
  attribute.
- Restricted every operation to the exact default collection. The adapter
  neither creates nor unlocks a collection and has no alternate vault,
  settings, SQLite, environment, file, process, network, UI, logging, or
  plaintext fallback.
- Opened the service without loading collections, verified the exact canonical
  `org.freedesktop.secrets` owner, and only then established a session or
  touched a collection or item. Prompt requests are denied; a returned prompt
  receives at most one bounded, no-auto-start dismissal.
- Implemented replace, exact single-item retrieval, deletion, absence, locked,
  denied, unavailable, corrupt, allocation, invalid-request, and bounded
  operation-failure behavior. Retrieval accepts only the two exact content
  types observed across supported libsecret/gnome-keyring behavior while
  preserving the complete byte sequence.
- Added fake-backed contract coverage and a resource-locked native
  setup/lifecycle/cleanup fixture. Runtime canaries travel only over framed
  standard input and are checked against arguments, environment, fixed
  output, executable, source, and isolated scratch paths.
- Directly proved create/read/replace/read/delete/absence, including a
  2,560-byte replacement and a second opaque-byte lifecycle containing NUL,
  high-bit, and invalid UTF-8 sequences; no-session unavailability; a real
  locked default collection; hostile service-name redirection rejection
  before session or item methods; and exact integration-schema cleanup.
- Exercised access-denied and delete-failure outcomes through the injected
  backend plus direct native-status classification. Those two cases are not
  claimed as operating-system policy fixtures; the target-native fixture owns
  the real lifecycle, locked, unavailable, and hostile-routing evidence.
- Ran the Linux evidence in a full-system Ubuntu 26.04 x86_64 QEMU guest as a
  dedicated non-root login against the guest's libsecret and gnome-keyring.
  This is direct guest operating-system and native-library execution with an
  emulated x86_64 CPU, not bare-metal hardware evidence.
- Inspected the x86-64 ELF test and its selected libsecret, GIO, GObject, and
  GLib dependencies and imports. The current public application does not yet
  compose this vault, so public-path selection remains later work.
- Preserved local and target-native build ownership. GitHub Actions has no
  build, test, package, release, or verification role.

## Evidence

- Focused Linux contract/setup/lifecycle/cleanup matrix: 4/4 passed.
- Linux `dev` configure/build/test matrix: 28/28 passed.
- Linux `core-only` configure/build/test matrix: 16/16 passed.
- Hostile architecture-boundary suite: 74/74 passed; the direct CMake verifier
  also passed after enforcing open-without-load, exact-name check, then
  session establishment.
- AddressSanitizer and UndefinedBehaviorSanitizer fake-contract executable:
  25 consecutive runs passed.
- A live hostile D-Bus owner observed only
  `org.freedesktop.DBus.Properties.GetAll`; it observed no `OpenSession`,
  collection, search, create, read, or delete method.
- The real default collection reported locked and the adapter returned the
  locked failure; an environment with no session bus returned unavailable.
- Native ELF dependency/import and current public-application selection
  inspections passed; the latter correctly reported the vault as not composed.
- The exact integration schema was absent after cleanup and after the complete
  Linux matrix. Checked-in setup and cleanup independently verify the exact
  fixture authority; the schema-wide absence check was a separate controlled
  D-Bus audit.
- Documentation validation and `git diff --check` passed.
- GitHub Actions workflow files and tracked workflow files: zero.
