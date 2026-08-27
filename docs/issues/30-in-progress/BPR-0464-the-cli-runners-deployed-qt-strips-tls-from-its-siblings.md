---
id: BPR-0464
title: The CLI runner's deployed Qt strips TLS from every sibling
status: In Progress
rfc: RFC-BPR-0002
depends_on: []
---

# The CLI runner's deployed Qt strips TLS from every sibling

`native_https_transport_fixture` passed thirty-six for thirty-six at
`b18b9336`, failed in the first full-graph matrix, and kept failing
alone — 0.31 seconds, `success-child-exit`, and under instrumentation
the child's own words: `fixture-success-unavailable`, with `--runtime`
reporting `tls=unavailable … supported=false`.

The regression is a build-graph interaction, not the transport.
[[BPR-0438]]'s post-build step copies the CLI process runner's Qt
runtime DLLs beside it — required, because the runner's Windows contract
hands its child an empty `PATH` — but "beside it" was the shared
`tests/github_contract/Debug/` output directory. The Windows loader
prefers the application directory over `PATH`, so every sibling in that
directory silently switched from the full Qt installation to a two-DLL
copy with no `plugins/tls/`, and `QSslSocket` lost its Schannel backend.
The fixture is simply the first sibling that ever needed one: the
`--contract` rows the earlier matrices ran never touch TLS, and a
two-target build that skips the runner's post-build step — exactly what
produced the `b18b9336` pass — never plants the DLLs.

The fix gives the runner its own `RUNTIME_OUTPUT_DIRECTORY`
(`cli_process_runner/`), so its deployed Qt sits where only it looks.
The stale pair in the shared directory is deleted by hand once, since no
build rule removes what an abandoned post-build step left.

`tests/git_contract/CMakeLists.txt` carries [[BPR-0437]]'s twin of the
same pattern for the two fake-Git contracts. It is recorded here rather
than repaired: those suites' siblings link QtCore alone, which loads no
plugin, so the trap is latent there — but it is the same trap, and the
next git_contract sibling that needs a Qt plugin will spring it.

## Required final state

- The CLI runner deploys its Qt into a directory only it occupies, and
  `github_cli_process_runner` still passes there.
- The shared `github_contract` output directory carries no deployed Qt,
  and `native_https_transport_fixture` passes target-native again.
