---
id: BPR-0023
title: Implement and verify Windows Credential Manager credentials
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0021]
---

# Implement and verify Windows Credential Manager credentials

Implement the native Windows Credential Manager adapter and directly prove
store/retrieve/delete of a disposable canary credential on a native Windows
environment. Verify secret absence from settings, SQLite, arguments,
diagnostics, logs, and UI text and that cleanup succeeds. Exercise locked or
unavailable storage, access denial, replacement, and delete failure; every
failure must remain explicit and must never fall back to settings,
environment, or a plaintext file.

GitHub Actions is not an acceptable evidence surface.
