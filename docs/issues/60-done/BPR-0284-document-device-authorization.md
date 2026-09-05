---
id: BPR-0284
title: Document device authorization and its authority boundaries
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0281, BPR-0282, BPR-0283]
---

# Document device authorization and its authority boundaries

## Required final state

- Product, system, authentication, review-round-trip, security, native-HTTPS,
  cancellation, build/test, operator, and verification documents state the
  implemented behavior and exact non-goals.
- The registered owner, public identifier, minimum permission set, installation
  implications, refresh behavior, GHES rule, manual fallback, and diagnostics
  are accurate without publishing secret or transient material.
- Every changed path from the immutable RFC base has an update or an explicit
  evidence-backed no-impact disposition; agent guidance changes only for a
  stable reusable workflow or boundary.
- The documentation gate and its focused negative tests pass.

## What this slice delivered — 2026-09-02

Documentation only. No behaviour changed, no source or test file was touched,
and every sentence added states what the tree does today with the card and the
check that proves it beside it.

**Where the ownership statement lives.** The typed flow and per-region
ownership went into `docs/architecture/system-architecture.md` as its own
section, in the shape the RFC-BPR-0003 epoch slice above it already uses,
rather than being spread through the GitHub host document. The GitHub host
document gets the things that are genuinely its own — the permission table,
the two `/login` routes and their admission rule, the polling numbers, the
refresh lifecycle, and the vault join point — because those are transport and
host facts, not allocation facts.

**Three claims were corrected while writing them.** The first draft cited
`application_device_authorization_release_contract` for the GHES rule. It does
not prove it: `--ghes-host` adds GHES profiles behind the GitHub.com profile,
the device path is composed against the *first* profile, and on a registered
platform that selector now expects `failed` or `cancelled` rather than
`unavailable`. What actually enforces the GHES rule is route admission —
`build_device_authorization_url` refuses every server contract but
`github_com_2022_11_28` — so the documents say that, and say the end-to-end
GHES row is still open under [[BPR-0285]].

The second: `application_device_authorization_unavailable` no longer means
what its name says on any shipped platform. It is registered-selection
evidence now (`DEVICE_FLOW_REGISTERED_SELECTION_OK`), and no document claims
otherwise.

The third: `DEVICE_FLOW_REQUESTS:0` counts submissions that reached the
refusing transport, not HTTP requests. Both the build/test and threat-model
sections say so explicitly, because on Windows the two readings come apart and
a reader who conflates them would conclude the uncontained gate sends nothing.

**One gap is recorded rather than papered over.** `qt::device_authorization_panel`
is referenced by nothing outside the Qt adapter and its own tests, and
`github_application.cpp` binds the projection port to a self-test recording
sink. So the panel is real and contract-tested at the adapter boundary, and no
shipped window mounts it. The UX document says exactly that instead of
describing a control a reviewer cannot reach.

**Not delivered, and recorded as such:** [[BPR-0516]]'s three target-native
browser observations (macOS, Linux, Windows x64) are all still pending;
[[BPR-0515]]'s adjudication of the uncontained Windows gate's network posture
is open; and the immutable final-head platform replay, the complete local
matrix, and the independent capability-critical review are [[BPR-0285]]'s.
Agent guidance was deliberately left alone — this slice established no
reusable cross-slice rule.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The documentation gate passes over the whole tree | `python3 scripts/verify_documentation.py --root .` | Exit 0, no output; run unpiped and the exit code read | this commit |
| The gate's own focused negative tests pass | `python3 tests/scripts/verify_documentation_tests.py` | All cases passed | this commit |
| Every selector named in the new prose is registered in the configured dev graph | `ctest --preset dev -N` filtered to the names used | All 19 present: `core_device_authorization`, `application_device_authorization_(host\|refresh\|public_path\|unavailable\|release_contract\|registered_course\|browser_handoff\|browser_absent)`, `application_github_credential_vault`, `github_device_authorization_(contract\|refresh_contract)`, `native_https_device_authorization_(contract\|fixture)`, `qt_device_authorization_scale_(100\|125\|200)`, `rfc0016_test_inventory`, `rfc0016_production_identity_security`, `composed_application_hosts`, `application_review_round_trip_composition_contract` | this commit |
| No whitespace defect in the change | `git diff --check` | Exit 0, no output | this commit |
| No touched file ends with a blank line | Final-byte check over every changed file | Every file ends with exactly one newline after a non-blank line | this commit |

## Integrator reproduction — 2026-09-02

`python3 scripts/verify_documentation.py --root .` → `DOCUMENTATION_OK`;
`python3 tests/scripts/verify_documentation_tests.py` → 54 OK; `git diff
--check` clean; the ledger's documentation table is the only ledger hunk. The
tree gap the slice surfaced — no shipped window mounts
`qt::device_authorization_panel`, and the composition binds the projection
port to a self-test sink — is carded as [[BPR-0526]]; the documents state it
as fact, which is right.
