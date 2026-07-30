---
id: BPR-0037
title: Resolve GitHub CLI production trust and identity binding
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0025, BPR-0027]
---

# Resolve GitHub CLI production trust and identity binding

Replace the BPR-0025 fixed `cli_unavailable` gate only after one selected
production CLI contract directly proves all four currently typed trust
properties:

- the executable actually launched is bound to the exact reviewed
  version/digest/source evidence despite path replacement or update races;
- an unknown-authority redirect cannot become an accepted result or forward
  credentials or a request body;
- persisted or otherwise ambient configuration, including
  `http_unix_socket`, cannot reroute the invocation, and selected
  `SSL_CERT_FILE`/`SSL_CERT_DIR` identity and contents are bound from profile
  validation through the CLI's eventual open despite replacement or mutation;
  and
- the invoked credential/account is bound to the exact correlated identity
  generation even if another process changes the host's active account.

Production enablement must also close three process-boundary risks left explicit
by BPR-0025:

- bind an owner-controlled working-directory identity through launch, reject
  unsafe owner/permission state, and prevent canonical/currently
  non-repository validation from being invalidated by directory replacement,
  repository creation, or another validation-to-launch race; and
- on Unix, contain or otherwise fail closed for a descendant that moves out of
  the child's initial process group. `CreateNewSession` makes the child the
  initial process-group leader, but signaling that group does not reach a
  descendant after `setpgid` moves it to another group in the same session;
  and
- bind the child and initial process-group identity through every cleanup
  signal, or otherwise eliminate the risk that a saved numeric PID/PGID is
  reused after leader exit and a later `kill(-pgid, ...)` reaches an unrelated
  process group. Initial-group cleanup is not considered race-free until this
  is directly proved.

The solution may select a different official CLI version or add a narrow
adapter-owned enforcement boundary, but it must retain the RFC requirements:
no token extraction, no credential in arguments or diagnostics, no shell,
bounded and redacted process handling, explicit authority, and no local
listener or alternate hosted service. A selected host string, fake process,
source-only inference, native HTTPS evidence, or successful read is not
sufficient.

Run direct macOS and Linux hostile executable, working-directory, CA-file, and
CA-directory replacement/content-mutation fixtures, directory
owner/permission and repository-creation cases, process-group escape,
PID/PGID-reuse, redirect/configuration, and account-switch fixtures against
the exact selected bytes and production invocation shape. BPR-0036 owns
equivalent Windows target-native process evidence. Until every trust and
process-boundary property is directly established, production CLI mode must
remain `cli_unavailable` and start zero processes.

GitHub Actions has no build, test, package, release, or verification role.
