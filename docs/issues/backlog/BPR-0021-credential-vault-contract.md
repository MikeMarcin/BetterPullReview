---
id: BPR-0021
title: Implement the credential-vault capability contract
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0020]
---

# Implement the credential-vault capability contract

Define a narrow host-owned enrollment and credential-vault port with strong
host/account keying, move-safe secret lifetime, explicit store/retrieve/delete
results, and no token-bearing core or Qt types. Add fake-vault tests for
replacement, absence, deletion, failure, cancellation, and identity isolation.

Credentials must never enter profiles, settings, SQLite, environment files,
logs, arguments, diagnostics, fixtures, crash records, or presentation
projections.

