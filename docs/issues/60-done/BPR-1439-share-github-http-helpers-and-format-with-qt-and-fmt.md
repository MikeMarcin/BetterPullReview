---
schema: "implement-rfc.card.v1"
id: "BPR-1439"
title: "Share GitHub HTTP helpers and format with Qt and fmt"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1439--BPR-1439--31a62109ca4f.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit (item 10: replace with Qt, use fmt not std::format)"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"One Link tokenizer in github_http_helpers serves gh, native, paged read, mutation and publication readers with their own policies; publication refuses unquoted and multi-relation next; github_paged_read and github_publication_contract pass.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}},{"criterion":"AC2","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"percent_encode wraps QByteArray::toPercentEncoding (bytes form), keeping / for branch and permalink; existing contract outputs unchanged in read, mutation, publication, device and permalink suites.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}},{"criterion":"AC3","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"json_media_type serves paged read, native, capability probe, mutation and device checks; refuses application/jsonx and other charsets; release-update media set unchanged.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}},{"criterion":"AC4","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"Publication blob via QByteArray::toBase64; dates via pinned fmt 12.2.0 (sha256 a2f4a8d5...878f) acquired as a verified archive and listed in notices; pinned date outputs pass; dependency and package gates pass.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}},{"criterion":"AC5","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"bpr/core/hex.hpp replaces plain hex in core, application, SQLite and MCP; Qt targets use QByteArray::toHex; other renderings untouched; core_theme_exchange and core-only suites pass.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}},{"criterion":"AC6","outcome":"pass","revision":"8172a3a63","evidence":{"kind":"observation","detail":"Paged read refuses a NUL in a header name or value; device verification URL with userinfo, explicit port or fragment is refused; both tested.","path":"docs/issues/60-done/BPR-1439-share-github-http-helpers-and-format-with-qt-and-fmt.md"}}]
---

# Share GitHub HTTP helpers and format with Qt and fmt

The 2026-10-13 audit found small HTTP and formatting helpers copied across the
GitHub adapter and the rest of the tree:
- five `Link` header parsers, with three levels of strictness;
- five percent-encoders, three of them using locale-dependent `std::isalnum`;
- five JSON `Content-Type` checks, the loosest of which accepts
  `application/jsonx`;
- a hand-written base64 encoder and RFC 3339 formatter;
- about 30 hex tables.

Two latent faults turned up as well. A header check's `"\r\n\0"` set stops at
the NUL, so an embedded NUL passes. The device-login verification URL check
accepts userinfo and any port. The owner chose Qt for these helpers, and fmt
rather than `std::format` for formatting. fmt is an owner-approved core
library.

## Acceptance

- AC1: One `Link` header parser in the GitHub adapter serves the gh response,
  native transport, paged read, mutation and publication readers. Each caller
  keeps its own URL policy. The publication reader no longer misses an
  unquoted `rel=next` or a multi-relation value.
- AC2: Percent-encoding uses `QByteArray::toPercentEncoding`, keeping `/`
  where a branch or path keeps it today. Existing outputs are unchanged, and
  no encoder depends on the locale.
- AC3: One JSON media-type predicate serves the five checks: `application/json`
  with at most a `charset=utf-8` parameter, case-insensitive and bounded. The
  release-update download keeps its own media-type set.
- AC4: The publication blob uses `QByteArray::toBase64`, and the RFC 3339
  publication date is formatted with pinned fmt, which is acquired like the
  other verified archives and listed in the notices. Outputs are unchanged.
- AC5: One core hex helper replaces the plain lowercase hex copies in core and
  in Qt-free targets. Qt targets use `QByteArray::toHex`. Integer, pkt-line
  length, uppercase-escape and JSON-escape renderings stay as they are.
- AC6: A header name or value with an embedded NUL is refused. The device-login
  verification URL is refused when it carries userinfo, an explicit port or a
  fragment. Each fault has a test.

## Evidence

Implementation commit `8172a3a63`.

- AC1: `src/adapters/github/src/github_http_helpers.{hpp,cpp}` holds the one
  `Link` tokenizer (`parse_link_header`, `link_relations`). The gh response
  reader and native transport share `link_completeness` (the verbatim copy in
  `native_https_transport.cpp` is gone); the paged read keeps its four-link,
  one-quoted-relation, unique-relation policy and `page_number_from_url`; the
  mutation reader keeps its exact-URL page check; the publication reader
  refuses any `next` relation, quoted or not, alone or among several, and an
  unreadable header or a link without `rel`. Tests:
  `github_paged_read` (`test_shared_http_helpers`) and
  `github_publication_contract` (unquoted `rel=next`, `rel="next last"`,
  `next` in a second link, malformed values; a `prev first` last page reads).
- AC2: `percent_encode(bytes, keep)` wraps the byte form of
  `QByteArray::toPercentEncoding`; read, mutation, publication (branch keeps
  `/`), device form and permalink (keeps `/`, segment validation unchanged)
  use it. The existing contract outputs (`octo-org%3Abpr%2Fharden`, search
  queries, permalinks) pass unchanged; invalid UTF-8 and NUL are tested byte
  for byte.
- AC3: `json_media_type` serves the paged read, native transport, capability
  probe, mutation and device checks; bounded at 256 bytes; tested accepting
  case and spacing variants and refusing `application/jsonx`, other charsets,
  extra or empty parameters. The release-update media types are unchanged.
- AC4: `QByteArray::toBase64` and `fmt::format` (pinned fmt 12.2.0, SHA-256
  `a2f4a8d51178f954e4c339007f77edd76ba0cb2e36f87a48e5a5403d9be5878f`, size
  1486029, `cmake/BprFmtProvider.cmake`, header-only `BetterPullReview::Fmt`,
  `third_party/fmt/`, `packaging/dependencies.json`, notices, offline-cache
  test). The pinned `Z` and `+01:30` dates pass; a year before 1000 is now
  zero-padded to four digits (tested).
- AC5: `bpr/core/hex.hpp` replaces the copies in `review_cycle`,
  `review_cycle_promotion`, `review_cycle_publication`, `review_cycle_json`
  (`cycle_hex`), `pull_request_location`, `theme_exchange`, five application
  sources, the MCP helper and four SQLite sources (the SQLite adapter does not
  link Qt). The Git adapter, `github_mutation_contract`, `tour_owner_service`
  and `github_application` use `QByteArray::toHex`. Integer, pkt-line,
  uppercase-escape and JSON-escape renderings are untouched. Test:
  `core_theme_exchange`.
- AC6: `github_paged_read` refuses a header name or value with a NUL
  (`header_breaks` carries its length); `github_device_authorization_contract`
  refuses userinfo (including an empty one), ports 443 and 8443 and a
  fragment.

Verification on macOS (dev preset, targeted build): 23 focused suites, the
dependency, architecture, core-boundary, package, release-security and
binary-audit gates pass; core-only builds and passes 269 of 272 (the three
documentation and allocation suites need the MASK `.agents` scripts absent
from the worktree); `coding-format-check` and changed-line clang-tidy are
clean; the documentation gate passes.
