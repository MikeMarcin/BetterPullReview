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
- Keep native HTTPS execution in `src/native_https_transport.cpp`; it is the
  only GitHub source permitted to use `QNetworkAccessManager`.
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
- Use typed records and explicit failure/limit handling at the adapter
  boundary. Do not expose provider response types inward.
- Keep credentials inside the designated authentication transport or native
  vault capability.
- Add effectful sources explicitly to this directory's CMake target. Do not
  move provider effects into the application host, Qt adapter, or composition
  root.
