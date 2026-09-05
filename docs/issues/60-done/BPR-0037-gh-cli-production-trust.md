---
schema: "implement-rfc.card.v1"
id: "BPR-0037"
title: "Resolve GitHub CLI production trust and identity binding"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0025","BPR-0027"]
completed: "2026-07-30"
---

# Resolve GitHub CLI production trust and identity binding

Determine whether the BPR-0025 fixed `cli_unavailable` gate can be replaced.
Replacement is permitted only after one selected production CLI contract
directly proves every trust property:

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

## Resolution

The product owner approved native HTTPS as the only milestone-one production
authentication transport. GitHub CLI choices remain visible, but selection
returns the stable `cli_unavailable` diagnostic before any CLI executable or
configuration discovery, CLI process creation, or descendant creation.
Positive CLI enablement is outside RFC-BPR-0002 and is deferred to
[RFC-BPR-0015](../../rfcs/RFC-BPR-0015-github-cli-production-enablement.md).
The fixed gate is therefore the completed milestone-one result rather than an
RFC-BPR-0002 closeout blocker.

The investigation also established that the current official GitHub CLI does
not satisfy the future eligibility gates:

- the official macOS arm64 `gh` 2.96.0 release dated 2026-07-02 was verified
  from its published ZIP SHA-256
  `f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`;
  the extracted binary SHA-256 was
  `b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`;
- `gh api` has no per-invocation account selector. `--user` is rejected,
  `gh auth switch --user` mutates persisted active-account state, and
  `gh auth token --user` exposes the token. The active-token implementation
  can fall back from a selected-user keyring error to the unkeyed host slot;
- an isolated `http_unix_socket` setting was loaded by both `gh` 2.95.0 and
  2.96.0. Each exact binary sent one authenticated `GET /user` request through
  the AF_UNIX sentinel, proving this is an effective ambient rerouting input
  rather than telemetry;
- both exact binaries followed a cross-authority `302`. The synthetic origin
  received the exact 47-byte body and authorization; the sink received
  neither, but accepting the foreign authority still fails the strict
  redirect contract. The `307` and `308` sink paths were not observed and
  remain unproved;
- both binaries rejected the disposable untrusted certificate before an HTTP
  request, but certificate rejection alone cannot establish the other trust
  properties; and
- the macOS keyring backend launches `/usr/bin/security`, so the selected
  credential cannot be used under the required no-descendant boundary.

The public application composes only the native HTTPS transport factory. A
clean-start CLI selection performs one bounded, noncredential
profile-resolution observation, then reaches the provider with no credential
material and receives `cli_unavailable`. Switching from an authenticated
native profile may first exact-delete that prior native credential binding;
it does not enroll or retrieve a credential for the CLI profile. Neither path
constructs or invokes a transport factory, performs wire or capability work,
schedules a CLI-specific effect, or launches a process. The dormant CLI
transport remains compiled and testable for future work, but is not linked
into the inspected tests-disabled Release executable.

The typed availability contract now reports the milestone-one product-scope
decision plus all seven unresolved technical properties:

1. executable-image identity;
2. cross-authority redirect preflight;
3. ambient configuration and trust-material identity;
4. exact account and credential-generation binding;
5. working-directory identity;
6. descendant-process containment; and
7. numeric process and process-group identity reuse.

Windows positive CLI execution was removed from BPR-0036. If RFC-BPR-0015 ever
becomes eligible, it owns target-native enablement on every proposed platform.
BPR-0036 continues to own the independent Windows native HTTPS, Credential
Manager, public-composition, runtime, and dependency evidence.

## Evidence

- The exact selected-binary trust probe passed for installed `gh` 2.95.0
  (binary SHA-256
  `798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`)
  and official `gh` 2.96.0. It recorded certificate rejection, the effective
  authenticated Unix-socket route, the followed cross-authority `302`, the
  bounded `307`/`308` observations, and
  `broker-availability: unavailable` without retaining the disposable token.
- `gh_cli_transport` passed and proved the exact eight reasons plus zero
  process-port calls for every supported read operation, cancellation, and
  invalid requests.
- `github_provider` passed and proved the CLI viewer observation produces
  `cli_unavailable` with zero transport-factory calls, wire requests,
  capability probes, CLI-specific effect posts, or queued work. The public
  clean-start selection path separately performs its bounded
  profile-resolution effect before that viewer observation. Host lifecycle
  coverage separately proves an authenticated native-to-CLI switch
  exact-deletes the predecessor binding before resolving the CLI profile.
- `application_public_path` and a direct tests-enabled `--self-test` passed
  with `PUBLIC_PATH_OK`, native-vault selection, and
  `CLI_UNAVAILABLE_OK`.
- The tests-disabled Release composition contract verifies the public CLI
  selection and fails if CLI transport, configuration, request-compiler, or
  process-runner symbols enter the executable.
- Python compilation, documentation validation, architecture boundaries,
  `git diff --check`, and tracked/on-disk workflow-absence checks passed.
