---
schema: "implement-rfc.card.v1"
id: "BPR-0909"
title: "Make GitHub CLI a discovered sign-in path"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"owner private v0.1.0 GHES test","milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
acceptance: [{"criterion":"The signed-out surface presents distinct native, GitHub CLI, and enterprise-server paths without eager CLI execution.","outcome":"pass","revision":"27efac98","evidence":{"kind":"command","detail":"Qt composed-GitHub and workbench-window checks passed at 100%, 125%, and 200% scale. Capability-absent and capability-present cases verify that Use GitHub CLI is a separate explicit gesture while Add enterprise server remains distinct."}},{"criterion":"One bounded token-free active-account inventory fails closed and admits exact active host/login pairs.","outcome":"pass","revision":"27efac98","evidence":{"kind":"command","detail":"gh_cli_transport passed. It pins auth status --active --json hosts without --show-token, applies process/output bounds, ignores unrequested metadata, and rejects malformed, oversized, noncanonical, mismatched, or multiply-active results."}},{"criterion":"A discovered GHES account reaches the existing exact-host CLI connection flow without a pre-added enterprise account.","outcome":"pass","revision":"27efac98","evidence":{"kind":"command","detail":"application_setup_destination_public_path passed with a synthetic token-free GHES inventory. It creates a CLI-only card naming the observed login, retains the existing consent gate, and selects the discovered exact-host profile."}},{"criterion":"Focused build, formatting, documentation, and board gates pass.","outcome":"pass","revision":"27efac98","evidence":{"kind":"command","detail":"The changed adapter, Qt, end-to-end, and app targets build; 8 focused tests pass; coding-format-check passes; DOCUMENTATION_OK and all 64 verifier tests pass; RFC-BPR-0015 board and milestone plan validate. The owner explicitly declined the broader heavy gate for this private patch build."}}]
---

# BPR-0909: Make GitHub CLI a discovered sign-in path

## Defect

The private v0.1.0 macOS build precomposes a GitHub CLI profile for GitHub.com,
but the same entry path cannot see a GHES session that `gh auth status` reports.
This makes the CLI look like another transport on an explicitly configured
account card even though the CLI already owns its host and account inventory.

The owner clarified the intended product model: `Use GitHub CLI` is a separate
sign-in path from explicit Native HTTPS/device-or-token accounts. Choosing that
path asks the installed official `gh` for its authenticated host/account
inventory, then offers those exact active sessions. It does not require the
reviewer to add the same GHES hostname separately first.

## Scope

- Offer `Use GitHub CLI` as its own signed-out entry path beside explicit
  GitHub and enterprise-account setup.
- Only after that explicit gesture, run the installed official `gh` through the
  existing constrained process boundary and request its active authentication
  status as bounded JSON. Never pass `--show-token`; admit only host, login,
  active and success state, and retain no credential material.
- Register one CLI profile for each admitted active host. GitHub.com may join
  its existing server card; a discovered GHES hostname may create a CLI-only
  card without first creating a Native HTTPS account. Selecting it remains
  bound to that exact hostname and passes through the existing CLI trust and
  read-consent warning.
- Do not launch `gh` merely because the screen opened, silently select a
  discovered account, change the active CLI account, or persist a token.

## Acceptance

- The signed-out surface presents distinct `Sign in`, `Use GitHub CLI`, and
  `Add enterprise server` paths; opening it launches no CLI process.
- Choosing `Use GitHub CLI` issues exactly one bounded active-status request,
  parses a hostile/truncated/malformed corpus fail-closed, and exposes every
  valid active successful GitHub.com or GHES host without exposing a token.
- A discovered GHES session can be selected and reaches the existing exact-host
  CLI connection flow without an explicit GHES account being added first.
- Focused adapter, Qt and composition/public-path checks, formatting and
  documentation verification pass.

## Implementation

- The signed-out Home surface and Accounts & hosts page now expose `Use GitHub
  CLI` as one capability-driven action distinct from native sign-in and adding
  an enterprise server. Merely opening either surface launches nothing.
- The action locates the installed `gh`, including standard Homebrew and
  MacPorts locations absent from a Finder-launched app's `PATH`, validates the
  supported version, then requests `auth status --active --json hosts` through
  the existing macOS write-denied process boundary.
- The bounded parser admits only canonical hostnames and one active successful
  visible-ASCII login per host. It ignores unrequested metadata, carries no
  token or scopes inward, and fails closed on malformed, oversized or
  ambiguous output.
- Each admitted host receives an exact-host CLI profile. A GHES host can create
  its own CLI-only setup card, while a later native enterprise account joins
  the same host card. Selection retains the existing CLI trust/read-consent
  gate and per-write confirmation.

## Verification

- `cmake --build --preset dev --target
  test_better_pull_review_gh_cli_transport_tests
  test_better_pull_review_qt_github_tests
  test_better_pull_review_qt_workbench_window_tests
  test_setup_destination_public_path better_pull_review`
- `ctest --test-dir build/dev -R
  '^gh_cli_transport$|^qt_(composed_github|workbench_window)_scale_(100|125|200)$|^application_setup_destination_public_path$'
  --output-on-failure` — 8/8 passed.
- `cmake --build --preset dev --target coding-format-check` — passed. The
  broader `coding-tidy` sweep was stopped when the owner requested the private
  patch build without the heavy gate; the focused changed-surface build and
  tests above remain the acceptance evidence.
- `python3 scripts/verify_documentation.py --root .` — `DOCUMENTATION_OK`.
- `python3 tests/scripts/verify_documentation_tests.py` — 64/64 passed.
