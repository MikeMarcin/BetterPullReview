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
shell, or expose credential-bearing environment state to unrelated code. It
must run from a trusted neutral working directory with an explicit minimal
environment allowlist, explicit hostname/endpoint/method, and an argument
allowlist that rejects token/status-output, response-cache, debug, and
credential-bearing flags. Inject unknown canary variables to prove they do not
reach the child.

Directly prove the selected CLI version rejects certificate failures and
unknown-authority redirects without forwarding a request body or credential.
If those semantics cannot be proved, CLI mode remains a closeout blocker rather
than inheriting the native HTTPS adapter's evidence.
