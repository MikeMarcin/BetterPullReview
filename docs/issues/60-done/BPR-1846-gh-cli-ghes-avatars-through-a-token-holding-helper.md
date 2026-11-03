---
schema: "implement-rfc.card.v1"
id: "BPR-1846"
title: "GHES profiles signed in through gh load people's pictures through a helper that holds gh's token"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"owner, 2026-11-03: use the gh token through a small wrapper so the token is never introduced into the app process","reservation":"docs/issues/_reservations/BPR-1846--BPR-1846--7b610d2a509e.json","related_cards":["BPR-1373","BPR-1807","BPR-1845"],"frontend_design":"not loaded: no layout, wording or colour change; only how a picture's bytes are fetched"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "ced4f09be", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_identity_avatar|core_operation_diagnosis|core_markdown_image|application_markdown_image_host|application_github_window_read_host)$'", "detail": "test_a_cli_profile_asks_through_the_helper: a gh CLI GHES profile (policy CliCredential, no credential) asks identity_avatar_api_prefix(<host>)<login>?s=<n> as a CliHelper hop with no credential and keeps the web route as an in-process fallback; credential-holding GHES, GitHub.com and every body image are unchanged. test_the_helper_admits_and_frames: the helper admits only the avatars API on its pinned lower-case host (another host, a look-alike host, another API path, a size outside the set, an upper-case login, a traversal, http, a port, an extra query and the web route are blocked); a redirect stays on the avatar origins and drops the token off the host; request and answer framing round-trip binary bytes and refuse wrong counts, trailing or missing bytes, loaded-without-bytes and over-bound answers. avatar_ghes_cli_helper: the window asks the helper hop, publishes its picture once, and after a failed helper asks the web route once, whose sign-in page keeps the initials."}}, {"criterion": "AC2", "outcome": "pass", "revision": "ced4f09be", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^gh_cli_avatar_helper_fixture$'", "detail": "tests/scripts/verify_gh_cli_avatar_helper.py, offline: a fake gh prints a fresh token only for `auth token --hostname ghe.bpr.invalid --user octo` under a private GH_CONFIG_DIR naming that account; a loopback CONNECT proxy and TLS origin answer as a private-mode GHES (avatars API: a PNG only to `Authorization: token <token>`; everything else a sign-in page). The application side is the real pinned gh configuration, the shipped gh process runner and gh_cli_transport::perform_cli_avatars launching the fixture build of the helper. private-mode: one launch answers loaded/200/image/png for octo, loaded for a redirect to avatars.<host> (which gets no token), blocked for a redirect off the avatar origins, blocked for another host's address and for the web route; the server saw the token only on the three host API requests and no CONNECT elsewhere. The token is in none of the launch's recorded program, arguments, environment, standard input or raw standard output, nor in the driver's output; the launch carries --user octo and GH_CONFIG_DIR. Direct helper runs: another host is blocked before gh is asked; a missing gh is cli_unavailable; a non-canonical pin is refused. Negative control: a helper that copied the token into its content type failed the course; the restored source is byte-identical and passes."}}, {"criterion": "AC3", "outcome": "pass", "revision": "ced4f09be", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(gh_cli_avatar_helper_fixture|github_provider|gh_cli_transport)$'", "detail": "signed-out (gh exits 1) answers cli_signed_out, a gh printing a non-token answers cli_unavailable, and gh removed after the profile was chosen launches nothing (configuration_changed); none sends a request. test_cli_avatars_go_through_the_helper: two avatars wait for one helper launch on an image lane, which serves both in order through the account's own transport with the configured helper; a credentialed policy is refused before any launch; a signed-out batch fails its avatar (so the window asks the web route), records exactly one content-free image.fetch line `action=avatar_fetch failure=credential_unavailable`, and holds the account's next avatar off without a launch. The existing gh_cli_transport course is unchanged and passes."}}, {"criterion": "AC4", "outcome": "pass", "revision": "ced4f09be", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target app_better_pull_review; ctest --preset dev -R '^(package_runtime_contract|native_release_contract|release_package_audit_contract|release_security_audit_contract|mimalloc_provider|mimalloc_tool_better_pull_review_gh_avatar_helper)$'", "detail": "tool_better_pull_review_gh_avatar_helper (better_pull_review_gh_avatar_helper) is a product executable with the product allocator and a Windows version resource; the application build copies it beside the executable (Contents/MacOS in the dev bundle, observed), Install.cmake ships it in every package's helpers (Linux install with $ORIGIN, Windows Authenticode via package-inputs helpers, macOS release.sh signs every Contents/MacOS Mach-O), and verify_release_package.py and verify_native_release.py require it; their tests and the mimalloc probe pass."}}, {"criterion": "AC5", "outcome": "pass", "revision": "ced4f09be", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors on the changed TUs; ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract|test_network_isolation_manifest)$'; python3 scripts/verify_documentation.py --root .", "detail": "Format check passed (clang-format 22.1.8); clang-tidy passed on every changed TU; architecture_boundaries, architecture_boundary_contract and test_network_isolation_manifest pass; DOCUMENTATION_OK."}}]
---

# BPR-1846: GHES profiles signed in through gh load pictures through a helper

Owner, 2026-11-03: use the gh token through a small wrapper so the token is
never introduced into the app process.

## Cause

[[BPR-1845]] asks a GHES picture of the server's avatars API with the
profile's credential, which answers under private mode. A profile signed in
through the GitHub CLI holds no credential in the application, so its pictures
still went only to the web route, which private mode answers with a sign-in
page, and every face kept its initials.

## Design

- Core decides. `admit_identity_avatar`, for GHES with
  `markdown_image_fetch_policy::CliCredential` and no credential, asks the
  avatars API as a `markdown_image_hop::CliHelper` hop without the credential
  and keeps the web route as the fallback. `core/gh_cli_avatar.hpp` admits
  what the helper may fetch (`admit_gh_cli_avatar_address`: only the avatars
  API on the pinned host) and frames a request batch and its answers. The
  avatars API address is written once, in `identity_avatar_api_prefix`.
- Application performs. The window read host sets `CliCredential` for a gh CLI
  profile; `identity_avatar_window` is unchanged and still falls back once.
- Adapters own the process. The GitHub provider queues helper hops per
  account transport and serves up to 16 with one launch on an image lane.
  `gh_cli_transport::perform_cli_avatars` launches the helper through the gh
  process runner (its supervision, Windows launch restriction and macOS write
  constraint) with the gh child environment and private pinned
  `GH_CONFIG_DIR`, and the arguments `--gh <gh> --hostname <host> --user
  <login>`. The composition's CLI transport gives these launches a gh runner
  of their own. A launch that finds no usable gh or helper fails its
  avatars, logs one `image.fetch` line and holds the account's helper hops
  off for a minute.
- The helper (`src/adapters/github/src/gh_avatar_helper.cpp`) links the
  core, Qt Core and Qt Network. It runs `gh auth token` once, fetches with
  `Authorization: token <token>` only on hops the core marks, follows only
  admitted redirects, bounds each picture to 1 MiB and ten seconds and the
  launch to thirty, and writes only the framed answers; nothing on standard
  error. `architecture_boundaries` admits it as a QProcess and Qt Network
  owner and as a writer of gh's `--hostname`, and checks its launch
  restriction, descriptor sweep, silent message handler and core admission.
- Documented in `docs/architecture/system-architecture.md` ("People's pictures
  on GHES"), `docs/ux/workbench-specification.md`, `packaging/README.md` and
  the packaging maintainers' guide.

## Open

- No live GHES is available, so a private-mode server answering this helper
  by login is checked only against the fake; [[BPR-1845]]'s open question
  (the API answering by login rather than `u/e?email=`) applies equally.
- The loopback course runs on macOS and Linux (it needs `sh` and `openssl`).
  The helper's Windows build (binary standard streams, launch restriction)
  is compiled only by the Windows lane.
- On macOS the course launches the helper without its `sandbox-exec` wrapper,
  because macOS refuses a nested sandbox inside the test runner's; the
  recorded command keeps the wrapper, and a manual run outside the test
  sandbox answered through it.
- Every other gh CLI operation still acquires the token transiently in the
  application ([[BPR-1373]]'s `acquire_gh_cli_credential`); this card keeps
  only the avatar path free of it.
