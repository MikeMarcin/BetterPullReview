# GitHub Adapter Instructions

This scope implements narrow GitHub application-host ports. Provider records,
transport types, credentials, URLs, and Qt effect objects must not cross into
the functional core or Qt presentation adapter.

- Keep GitHub CLI process execution in
  `src/github_cli_process_runner.cpp`; it is the only GitHub source permitted
  to use `QProcess`.
- Keep GitHub CLI invocation and child-environment construction in
  `src/gh_cli_transport.cpp`. The exact `GH_TELEMETRY=0` hardening setting in
  that source is the sole permitted telemetry reference.
- Keep `github_cli_process.hpp`, `github_cli_process_runner.hpp`, and
  `github_wire_transport.hpp` standard-C++-only. The process result may expose
  bounded stdout only on clean success; failures expose fixed categories, not
  stderr, `QProcess::errorString()`, arguments, environment, paths, exit
  diagnostics, or partial output.
- Construct the CLI child environment from nothing. Admit only typed
  platform credential-session and selected HTTPS proxy/trust facts plus the
  fixed noninteractive settings. Never read or pass token, host/repository,
  config-directory, debug, pager/editor/browser, TTY/color override, update,
  spinner, prompt, or arbitrary parent variables.
- Compile only the closed `gh api` argument shape from a revalidated canonical
  read request. The current production trust gate remains
  `cli_unavailable`: production cannot bind the launched bytes to the reviewed
  executable digest; selected `gh` 2.95.0 follows cross-authority redirects,
  can honor ambient persisted `http_unix_socket` configuration, and has no
  per-invocation account selector that binds the child to the correlated
  credential generation. A fake child or native HTTPS evidence must never
  enable CLI mode.
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
  wipeable or hard-bounded.
- Keep macOS Security framework effects in `src/macos_keychain.cpp`. Its v1
  contract is the exact default file-based Keychain, one generic-password
  item, and noninteractive `SecItem` operations. Do not add data-protection,
  synchronization, access-group, accessibility, alternate-keychain, UI,
  settings, database, environment, file, process, or logging fallbacks.
- Keep Windows Credential Manager effects in
  `src/windows_credential_manager.cpp`. Use only exact-target,
  wide-character generic-credential CRUD with the fixed production namespace,
  `CRED_PERSIST_LOCAL_MACHINE`, bounded wiping, and `CredFree`. Do not add
  Credential UI, enumeration, Credential Locker, DPAPI, registry, settings,
  database, environment, file, process, network, logging, or alternate-vault
  fallbacks.
- Keep Linux Secret Service effects in `src/linux_secret_service.cpp`. The
  Linux target alone imports stable `libsecret-1>=0.20.0`. Open the literal
  canonical `org.freedesktop.secrets` service with `SECRET_SERVICE_NONE`,
  validate the exact proxy name before establishing a session, and then use
  only the default collection. Deny prompt vfuncs, never unlock or create a
  collection, keep production and integration-test v1 schemas disjoint, and
  store opaque bytes as `application/octet-stream`.
  Retrieval may also accept exact `text/plain` because the selected
  gnome-keyring/libsecret stack was directly observed normalizing that
  metadata while preserving the bytes. Do not read or mutate service-routing
  environment, or add UI, settings, SQLite, environment, file, process,
  network, logging, plaintext, or alternate-vault fallbacks.
- Use typed records and explicit failure/limit handling at the adapter
  boundary. Do not expose provider response types inward.
- Exhaust every REST `Link` continuation and GraphQL cursor before returning a
  snapshot. Enforce cursor progress, page/record/byte ceilings, exact
  repository authority, expected result counts, and capability-shaped
  selection; incomplete or ambiguous pagination fails atomically.
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
