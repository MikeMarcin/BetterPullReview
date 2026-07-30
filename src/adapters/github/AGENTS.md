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
- Use typed records and explicit failure/limit handling at the adapter
  boundary. Do not expose provider response types inward.
- Keep credentials inside the designated authentication transport or native
  vault capability.
- Add effectful sources explicitly to this directory's CMake target. Do not
  move provider effects into the application host, Qt adapter, or composition
  root.
