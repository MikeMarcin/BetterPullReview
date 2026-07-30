---
id: BPR-0024
title: Implement and verify Linux Secret Service credentials
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0021]
---

# Implement and verify Linux Secret Service credentials

Implement the native Linux Secret Service adapter and directly prove
store/retrieve/delete of a disposable canary credential on a native Linux
desktop session. Verify secret absence from settings, SQLite, arguments,
diagnostics, logs, and UI text and that cleanup succeeds.

GitHub Actions is not an acceptable evidence surface.
