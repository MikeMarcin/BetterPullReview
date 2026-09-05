---
schema: "implement-rfc.card.v1"
id: "BPR-0021"
title: "Implement the credential-vault capability contract"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0020"]
completed: "2026-07-29"
---

# Implement the credential-vault capability contract

Define a narrow host-owned enrollment and credential-vault port with strong
host/account keying, move-safe secret lifetime, explicit store/retrieve/delete
results, and no token-bearing core or Qt types. Add fake-vault tests for
replacement, absence, deletion, failure, cancellation, and identity isolation.

Credentials must never enter profiles, settings, SQLite, environment files,
logs, arguments, diagnostics, fixtures, crash records, or presentation
projections.

## Result

- Added a platform-neutral application vault port with exact
  profile/profile-revision/host/account/server authority and exact
  request/application-session/identity-generation correlation.
- Added a non-default-constructible, noncopyable, move-only credential owner
  that accepts nonempty material up to 2,560 bytes, exposes only a read-only
  byte span, empties moved-from objects, and overwrites storage before reset,
  replacement, or destruction.
- Added typed store, retrieve, absence, delete, prior-absence, bounded-failure,
  and cancellation terminals without raw operating-system errors or a text,
  formatting, comparison, hashing, serialization, or Qt surface.
- Added a queued fake-vault suite for replacement, exact identity isolation,
  malformed input, absence, deletion, failure preservation, reordering,
  duplicate work, cancellation, and late-completion suppression. Failed,
  rejected, and canceled stores destroy transient material before publishing
  their terminal.
- Reserved macOS Keychain, Windows Credential Manager, and Linux Secret
  Service APIs for their designated future GitHub-adapter sources and rejected
  the entire credential contract from core and Qt.
- Kept authenticated account discovery and native vault behavior assigned to
  BPR-0022 through BPR-0024 and BPR-0027; no native-vault claim was made.
- Preserved local and target-native build ownership. GitHub Actions has no
  build, test, package, release, or verification role.

## Evidence

- Targeted credential-vault suite: 1/1 passed.
- Application suites: 6/6 passed.
- Clean Qt-enabled `dev` matrix: 24/24 passed.
- Clean Qt-free `core-only` matrix: 16/16 passed.
- Architecture boundary checks: 27/27 passed.
- Core boundary checks: 4/4 passed.
- Capability/read fixture security checks: 12/12 passed.
- Platform configuration checks: 4/4 passed.
- Address/undefined-behavior sanitizer credential-vault run passed.
- Optimized-code inspection retained the overwrite loop before deallocation.
- Documentation and original source-pack validators passed.
- `git diff --check`: passed.
- GitHub Actions workflow files and tracked workflow files: zero.
