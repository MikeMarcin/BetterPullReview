---
id: BPR-0022
title: Implement and verify macOS Keychain credentials
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0021]
---

# Implement and verify macOS Keychain credentials

Implement the native macOS credential-vault adapter and directly prove
store/retrieve/delete of a disposable canary credential. Verify that the
secret is absent from settings, SQLite, arguments, diagnostics, logs, and UI
text and that cleanup succeeds. Exercise locked, unavailable, access-denied,
replacement, and delete-failure states; every failure must remain explicit and
must never fall back to a profile, settings, environment, or plaintext file.
