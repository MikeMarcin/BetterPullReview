---
schema: "implement-rfc.card.v1"
id: "BPR-1434"
title: "Fold paths and decode UTF-8 with utf8proc instead of hand-written tables"
status: "Done"
rfcs: []
depends_on: ["BPR-1435"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c87cc77de","evidence":{"kind":"observation","detail":"Core links pinned utf8proc 2.12.0 via bpr/core/utf8.hpp; inventory, notice, packaging and dependency contracts pass.","path":"docs/issues/60-done/BPR-1434-unicode-with-utf8proc.md"}},{"criterion":"AC2","outcome":"pass","revision":"c87cc77de","evidence":{"kind":"observation","detail":"Path folding 1,589 -> 93 lines; workspace tests pin F5 80 80, kana, Hangul, sigma, Kelvin, mark order and sharp s; bundle, workspace_source and capture pass.","path":"docs/issues/60-done/BPR-1434-unicode-with-utf8proc.md"}},{"criterion":"AC3","outcome":"pass","revision":"c87cc77de","evidence":{"kind":"observation","detail":"Eight core helpers and two agent-stream helpers share utf8::decode/append; 103 core suites and application_agent_launch pass.","path":"docs/issues/60-done/BPR-1434-unicode-with-utf8proc.md"}},{"criterion":"AC4","outcome":"pass","revision":"c87cc77de","evidence":{"kind":"observation","detail":"MCP helper refuses a lone low surrogate as invalid_request; observer fixture passes with shared encoder and raw-byte check.","path":"docs/issues/60-done/BPR-1434-unicode-with-utf8proc.md"}}]
---

# Fold paths and decode UTF-8 with utf8proc instead of hand-written tables

The 2026-10-13 audit found about 1,400 lines of pasted Unicode 16 tables in
[review_cycle_path_folding.cpp](../../../src/core/src/review_cycle_path_folding.cpp),
covering only Latin, Greek and Cyrillic, with a decoder that read lead bytes
F5-FF as three-byte sequences (so `F5 80 80` met `E5 80 80`), and about 17
separately written UTF-8 decoders and encoders, two of which wrote invalid
UTF-8 for a lone surrogate escape. The owner chose utf8proc, one of the
approved pure core libraries.

## Acceptance

- AC1: Core links pinned utf8proc 2.12.0 and exposes one toolkit,
  `bpr/core/utf8.hpp`, for decoding, validation, encoding, canonical
  decomposition and one-to-one case folding. utf8proc appears in the release
  inventory with its notice and provenance.
- AC2: Collision keys decompose and fold every script (kana, Hangul, σ/ς, the
  Kelvin sign, combining-mark order), never apply full foldings such as ß to
  ss, keep invalid bytes as themselves, and never let an invalid sequence meet
  a valid one.
- AC3: The duplicated core decoders (review submission, profile window,
  writing validation, review-cycle JSON, workspace, material, fuzzy match and
  the source matcher) and the agent-stream helpers use the shared toolkit with
  their existing signatures. Deliberately different decoders stay: the
  metered diff validator, the lenient suggestion editor and the Windows WTF-8
  pair.
- AC4: The MCP context helper and the designated live observer refuse a lone
  surrogate escape instead of writing invalid UTF-8, and the observer checks
  raw string bytes.

## Evidence

- AC1: `cmake/BprUtf8procProvider.cmake` builds utf8proc 2.12.0 (Unicode 18.0.0
  data; SHA-256, size and license checked; offline cache honoured) and core
  links it privately; `bpr/core/utf8.hpp` and `src/core/src/utf8.cpp` are the
  one toolkit. `third_party/utf8proc`, `packaging/dependencies.json` and
  `NOTICE_FILES` carry it; `package_runtime_contract`,
  `release_security_audit_contract`, `binary_dependency_audit_contract` and
  `dependency_modernization_contract` pass.
- AC2: `review_cycle_path_folding.cpp` falls from 1,589 to 93 lines.
  `core_review_cycle_workspace` keeps its collision cases and adds `F5 80 80`
  against U+5000, が against か plus U+3099, a Hangul syllable against its jamo,
  σ/ς, the Kelvin sign, combining-mark order, and ß kept apart from ss;
  `core_review_cycle_bundle`, `workspace_source` and `review_cycle_capture`
  pass.
- AC3: the eight core helpers and the two agent-stream helpers keep their
  signatures over `utf8::decode`/`utf8::append`; all 103 `core_*` suites and
  `application_agent_launch` pass.
- AC4: `application_mcp_context_helper` refuses `{"method":"\uDC00"}` as
  `invalid_request` under a null identity (it was previously decoded to
  invalid UTF-8 and refused as an unknown method);
  `designated_live_public_observer_fixture` passes with the shared encoder and
  raw-byte check.
- Gates: `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract` and the
  documentation gate pass; clang-format 22 applied; clang-tidy 22 reports no
  finding on changed lines.
