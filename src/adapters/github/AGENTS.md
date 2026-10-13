# GitHub Adapter Instructions

This scope implements narrow GitHub application-host ports. Provider records,
transport types, credentials, URLs, and Qt effect objects must not cross into
the functional core or Qt presentation adapter.

- Keep GitHub CLI process execution in
  `src/github_cli_process_runner.cpp`; it is the only GitHub source permitted
  to use `QProcess`.
- On Windows every launch through that runner takes the platform target's
  launch restriction (`bpr/platform/windows_standard_handles.hpp`), the one
  the Git adapter's unguarded Git takes: the child inherits its three standard
  handles and nothing else, a launch whose handle list cannot be built is
  refused at creation, and a child started without the restriction is refused
  after the start (BPR-1114). That restriction is the runner's only
  process-creation modifier; `architecture_boundaries` refuses any other.
- On Unix the runner's one child-process modifier sets the spool's file-size
  limit when there is one and then bounds the child's descriptor sweep
  (`bpr/platform/unix_descriptor_sweep.hpp`, BPR-1252), as Git's launches do:
  without it, every `gh` request on macOS made one `close` call per allowed
  descriptor.
- Keep GitHub CLI invocation and child-environment construction in
  `src/gh_cli_transport.cpp`. The exact `GH_TELEMETRY=0` hardening setting in
  that source is the sole permitted telemetry reference.
- Keep `github_cli_process.hpp`, `github_cli_process_runner.hpp`, and
  `github_wire_transport.hpp` standard-C++-only. The process result may expose
  bounded stdout only on clean success. The explicitly opted-in completed
  `gh api --include` HTTP response path may carry a bounded nonzero
  exit code so the adapter can normalize an HTTP error. The closed local
  `git clone -h` capability probe may preserve its bounded completed stdout
  and documented help exit 129; it carries no credentials or repository input.
  No other local Git request opts into nonzero output. Crashes, cancellation,
  timeouts, limits and unrelated process requests expose fixed categories, not
  stderr, `QProcess::errorString()`, arguments, environment, paths, exit
  diagnostics, or partial output. No raw response is logged.
- Construct the CLI child environment from nothing. Admit only typed
  platform credential-session and selected HTTPS proxy/trust facts plus the
  fixed noninteractive settings. Never read or pass token, host/repository,
  debug, pager/editor/browser, TTY/color override, update, spinner, prompt, or
  arbitrary parent variables. The one configuration-directory variable is a
  pinned profile's per-call `GH_CONFIG_DIR` ([[BPR-1373]]): `run_gh_cli` in
  `src/gh_cli_transport.cpp` creates that directory for one launch, owner-only
  and read-only, holding `config.yml` and a `hosts.yml` naming only the pinned
  host and login. It removes the directory when the launch ends and refuses the
  launch if any step fails. Every `gh` launch of a pinned configuration goes
  through `run_gh_cli`; local Git never does. RFC-BPR-0078 supersedes the
  tokenless rule only for the selected macOS CLI account: the closed
  `auth token --hostname HOST --user LOGIN` acquisition uses bounded sensitive
  capture, and one operation-owned credential is passed separately from generic
  request/environment/result records. Only the runner's exact four-name table
  may name token variables; generic requests reject all four and the runner
  selects one primary variable by stock-gh hostname classification. No parent
  token, cross-operation cache, persistent credential or `gh auth switch` is
  permitted. Keep the same bytes through API, gh-owned helper descendants and
  GET-only recovery, holding sensitive/configuration owners through reap and
  quarantine. Owned buffers are overwritten; Qt, OS and child memory copies
  are not claimed precisely eraseable. The [RFC-0078 native ledger](../../../docs/verification/RFC-BPR-0078.md)
  binds stock-gh named absence, selected API identity and an authentication-dependent
  gh-owned helper negative separately from help/version or fixture admission.
  Missing and inaccessible named lookups share `credential_unavailable`; tool
  failure and an attempted indeterminate write must remain distinct.
- Compile only closed `gh api` argument shapes from revalidated canonical
  requests; a bounded local `gh --version` probe may precede opt-in use and
  carries no credential or request body, as do the closed local
  `gh <topic> --help` capability probes of the Git-object path. Native HTTPS remains the default production authentication
  transport. RFC-BPR-0015 authorizes an explicitly opted-in configured-CLI
  account profile with visible residual risks for reads and review/comment
  publishing. Its former blanket `cli_unavailable` rule is historical; a
  platform or operation remains unavailable until its revised direct
  acceptance checks pass. Preserve the eight original trust gaps as explicit
  limitations, never as guarantees established by a warning.
- For RFC-BPR-0015 Git-object reads only, the selected `gh` may perform a
  bounded exact-repository Git operation that internally launches Git and its
  own `gh auth git-credential` shell helper. BetterPullReview must not invoke
  the helper or a shell directly, obtain helper output, alter
  global Git/`gh` authentication, or fall back to native credentials. The
  macOS launch boundary may invoke `sandbox-exec` with a closed profile that
  denies file writes for every `gh` process, including version and REST calls;
  the bounded clone alone may write to its owned temporary object store.
  Refuse CLI availability when that boundary cannot be established. A
  before/after configuration check alone does not contain first-run `gh`
  migration of legacy configuration or symlinked targets. The helper's process,
  executable/configuration trust remains a visible limitation. RFC-BPR-0078
  binds all helper launches to the acquired operation credential; pre/post
  viewer checks continue to enforce the numeric account and chosen login. A GraphQL blob read instead selects `viewer { login databaseId }`
  in the same query and holds that account and login to the lease's bound
  account exactly as an observation is held, refusing the lease on a mismatch;
  only such a read may skip the observations, and a blob it cannot carry as
  text keeps the observed REST read ([[BPR-1370]]). This exception does not expand `gh api` request shapes or permit
  publication before its own acceptance milestone. Credential-free local Git
  may serve the bounded cloned object store through closed protocol v2
  arguments, without network, helper, hooks or repository-controlled executable
  configuration; it runs only in the GitHub CLI process runner.
  The RFC0065 owner amendments replace the former exact `gh` 2.100.0/2.101.0
  allowlist and exact Git vendor/banner equality with bounded required-operation
  checks: `gh` 2.96 or later (the RFC-BPR-0015 account/API floor; later majors
  admitted) and Git 2.49 or later for `clone --revision`. Before any
  credentialed `gh` use, write-denied local `gh api --help`,
  `gh repo clone --help` and `gh auth git-credential --help` probes (32 KiB,
  10 s, completed zero exit only, no standard error) must show every flag and
  usage the path invokes; RFC-BPR-0078 adds the local `gh auth token --help`
  hostname/user check. `gh_cli_help_topic` is the closed probe set. A failed
  check reports `cli_version` or `cli_capability_probe`, an unsupported tool
  `cli_qualification`; the host maps them to `gh_tool_probe_failed` and
  `gh_tool_unsupported`. Direct installed-tool evidence remains
  installation-specific; capability admission is not universal native
  qualification.
  It enforces a
  32 MiB per-file ceiling, and
  supervises a 96 MiB/128-entry temporary-store abort threshold. The latter
  is not a hard aggregate disk quota. Snapshot-derived local refs do not
  establish fresh remote refs; keep accepted-revision and pack verification.
- Keep the CLI trust gaps as real evidence, not telemetry-only assumptions.
  The official `gh` 2.96.0 binary followed a cross-authority `302`, honored
  persisted `http_unix_socket` for `gh api`, and routed an authenticated
  request through an AF_UNIX sentinel. `gh api` has no per-invocation
  `--user`; `gh auth token --user` exposes the token and
  `gh auth switch --user` mutates persisted active-account state. The latter
  remains forbidden. Historical tokenless private-config selection could fall
  back to the active token if the pinned login had no Keychain entry.
  RFC-BPR-0078 replaces it with explicit named acquisition and the sensitive
  operation environment above, refusing unavailable credentials before API or
  network Git. The [[BPR-1370]] viewer check still binds login and numeric ID. A fake
  child or native HTTPS evidence must never enable CLI mode.
- Keep native HTTPS execution in `src/native_https_transport.cpp`; it is the
  only GitHub source permitted to use `QNetworkAccessManager`.
- Keep native HTTPS on its dedicated worker with manager-local ordered system
  proxy candidates, explicit system roots, `VerifyPeer`, TLS 1.2 or later,
  the completed-handshake signal, manual exact-authority REST redirects, and
  fixed request/header/body/redirect/deadline bounds. Do not add global
  proxy/TLS mutation, automatic redirects, certificate-error bypass,
  automatic content decoding, credential persistence, or unbounded reads. The
  loopback proxy and fixture-CA seam must remain private to test-enabled
  builds; Qt-owned Authorization and parser buffers are not claimed precisely
  wipeable or hard-bounded. Query-bearing redirects fail closed.
- Keep macOS Security framework effects in `src/macos_keychain.cpp`. Its v2
  contract is the exact default file-based Keychain, one generic-password
  item, and noninteractive `SecItem` operations. Do not add data-protection,
  synchronization, access-group, accessibility, alternate-keychain, UI,
  settings, database, environment, file, process, or logging fallbacks.
- Keep Windows Credential Manager effects in
  `src/windows_credential_manager.cpp`. Use only exact-target,
  wide-character generic-credential CRUD with the fixed production namespace,
  hostname-bound authority, `CRED_PERSIST_LOCAL_MACHINE`, bounded wiping, and
  `CredFree`. Do not add
  Credential UI, enumeration, Credential Locker, DPAPI, registry, settings,
  database, environment, file, process, network, logging, or alternate-vault
  fallbacks.
- Keep Linux Secret Service effects in `src/linux_secret_service.cpp`. The
  Linux target alone imports stable `libsecret-1>=0.20.0`. Open the literal
  canonical `org.freedesktop.secrets` service with `SECRET_SERVICE_NONE`,
  validate the exact proxy name before establishing a session, and then use
  only the default collection. Deny prompt vfuncs, never unlock or create a
  collection, keep production and integration-test v2 schemas disjoint, and
  store opaque bytes as `application/octet-stream`.
  Retrieval may also accept exact `text/plain` because the selected
  gnome-keyring/libsecret stack was directly observed normalizing that
  metadata while preserving the bytes. Do not read or mutate service-routing
  environment, or add UI, settings, SQLite, environment, file, process,
  network, logging, plaintext, or alternate-vault fallbacks.
- Use typed records and explicit failure/limit handling at the adapter
  boundary. Do not expose provider response types inward.
- Parse recorded HTTP methods only through `parse_recorded_http_method`.
  Accept exact uppercase `GET` and `POST`; never coerce a missing, malformed,
  differently cased, or unsupported method into either value.
- Pass every GraphQL read and capability-probe document through the shared
  bounded full-document operation scanner. Require exactly one explicit
  `query` plus bounded fragment definitions; reject top-level `mutation` or
  `subscription` definitions, multiple operations, malformed scanned
  structure or token framing, and byte, nesting, or definition-limit
  exhaustion. The scanner is not a complete GraphQL grammar parser. Comments,
  strings, block strings, field names, and operation names are data, not
  operation-type evidence, and canonical compiled request-body equality
  remains required for outbound read admission.
- Exhaust every required REST `Link` continuation and GraphQL cursor before
  returning a snapshot. Repository-wide open-PR search may instead stop after
  a bounded, semantically validated page whose well-formed total exceeds the
  configured search limit: retain zero search rows and mark search availability
  as `result_limit` while preserving the exact PR read. Its received
  rows, authority and declared continuation must validate before discard.
  Enforce cursor progress, page/record/byte ceilings, exact repository
  authority, expected result counts, and capability-shaped selection;
  incomplete or ambiguous pagination fails atomically. The personal
  discovery search (BPR-0988) reads one bounded page and never follows its
  continuation: a longer or incomplete answer is reported partial, and any
  failed request fails the whole answer.
- Preserve `core`, `search`, and `graphql` rate observations independently.
  GitHub.com requires the applicable exact tuple; GHES may omit a whole tuple
  but may not supply a partial, malformed, unknown, or wrong-resource tuple.
  Structured secondary and GraphQL `RATE_LIMITED` bodies identify the exact
  causal record without exposing provider text.
- Keep credentials inside the designated authentication transport or native
  vault capability.
- Add effectful sources explicitly to this directory's CMake target. Do not
  move provider effects into the application host, Qt adapter, or composition
  root.
- Do not add GitHub Actions for configuration, build, test, package, release,
  or verification. Those surfaces remain local or target-native.
