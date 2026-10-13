---
schema: "implement-rfc.card.v1"
id: "BPR-1433"
title: "Decode JSON with glaze behind one shared structural check"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1434"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Decode JSON with glaze behind one shared structural check

The 2026-10-13 audit counted eight hand-written JSON implementations (about
2,900 lines). Four GitHub structural pre-checks had drifted apart (some
refused `1e999`, one had no string limit, one reported an over-long string as
malformed), and the MCP helper's reader accepted `.5`, `01` and raw control
characters. The owner chose glaze, now an approved core library. glaze decodes
but cannot refuse duplicate keys, so one first-party structural check remains.

## Acceptance

- AC1: `core::json_preflight` is the one structural check (one value,
  well-formed strings and numbers below 10^308, no duplicate or escaped key,
  per-caller ceilings with string and other ceilings as structural limits); the
  three GitHub copies are gone and the GitHub shared header wraps it.
- AC2: The MCP context helper decodes requests and writes strings with glaze
  after the shared check, so non-RFC 8259 input is refused; the designated live
  observer reads its exact fields, including 64-bit identifiers, with glaze.
- AC3: glaze is part of the Qt-free core graph, and the two deliberate
  first-party parsers (the canonical review-cycle JSON and the byte-range agent
  stream parser) are documented with their reasons.

## Evidence

- AC1: `src/core/src/json_preflight.cpp` is the single check;
  `github_read_contract.cpp`, `github_paged_read.cpp` and
  `github_capability_probe.cpp` keep only their ceilings and call it, and
  `github_json_preflight.hpp` is a wrapper. `core_json_preflight` covers one
  value, `1e999` and `.5`/`01`, raw control characters, duplicate and escaped
  keys, and string versus member ceilings; `github_read_contract`,
  `github_anonymous_read_contract`, `github_mutation_contract`,
  `github_publication_contract`, `github_paged_read` and
  `github_capability_probe` pass unchanged.
- AC2: `mcp_context_helper.cpp` runs the shared check and UTF-8 validation,
  then `glz::read_json` into `glz::generic`, and writes strings with
  `glz::write_json`; `application_mcp_context_helper` passes.
  `designated_live_public_observer.cpp` reads its exact fields, with 64-bit
  `id` values, through `glz::meta` mappings;
  `designated_live_public_observer_fixture` passes.
- AC3: `cmake/BprDependencies.cmake` acquires glaze outside `BPR_BUILD_QT`;
  `dependency_modernization_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`, `package_runtime_contract`,
  `release_security_audit_contract` and `binary_dependency_audit_contract`
  pass. The two first-party parsers and their reasons are under "JSON
  decoding" in `docs/development/build-and-test.md`.
- Gates: 119 focused tests pass; changed-line clang-tidy 22 has no findings;
  the format check and `verify_documentation.py` pass.
