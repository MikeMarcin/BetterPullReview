---
id: BPR-0025
title: Implement hardened GitHub CLI broker transport
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0019, BPR-0020]
---

# Implement hardened GitHub CLI broker transport

Invoke a validated absolute `gh` executable through an injectable process
port, use an argument vector and standard input, disable prompts and ambient
repository selection, bound time and output, classify failures, and redact all
retained diagnostics.

The broker must never extract a token, place credentials in arguments, use a
shell, or expose credential-bearing environment state to unrelated code.
