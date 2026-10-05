---
schema: "implement-rfc.card.v1"
id: "BPR-1077"
title: "Implement revisioned header edits and root-post classification"
status: "Done"
rfcs: ["RFC-BPR-0047"]
depends_on: ["BPR-1076"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-03","milestone":"semantics","acceptance":["R05","R07","R08","R41"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"All 5 header-edit-cases.json cases give their exact after bytes (CRLF + native suggestion:-1+0 patch relabel/removal, empty suggestion fence, Unicode subject, CRLF parse-only no-op); 3,924 relabel and remove/re-add round trips over 12 labels x 3 forms x 3 decoration lists x LF/CRLF/Unicode/patch tails restore the original bytes; no-op requests rewrite nothing; 30,000 generated edits change one span on code point boundaries and are idempotent. FEEDBACK_HEADER_EDIT_OK, macOS arm64 core-only Debug -Werror.","command":"ctest --test-dir build/core-only -R core_feedback_header_edit (tests/core/feedback_header_edit_tests.cpp against the pack's fixtures/header-edit-cases.json)"}},{"criterion":"AC2","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"All 24 input orders of a thread whose root has the largest id and comes last classify the same root and label; differently labelled replies (including a reply to a reply) never change it. A root website edit re-parses only the root and changes only its conversation; a reply edit re-parses only the reply and changes no conversation, and replies keep their own annotations.","command":"ctest --test-dir build/core-only -R core_feedback_conversation (tests/core/feedback_conversation_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"AC3","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"Edits chosen against another body digest or parser revision are stale_revision; composed with the real revisioned_review_draft a late edit command is refused with conflict and the newer draft is kept byte for byte, and another draft scope is refused with the draft untouched. Refreshes computed before another was adopted, or for a replaced account, are refused and the current annotation set is kept; nothing is reused across accounts or pull requests.","command":"ctest --test-dir build/core-only -R core_feedback_header_edit (tests/core/feedback_header_edit_tests.cpp against the pack's fixtures/header-edit-cases.json); ctest --test-dir build/core-only -R core_feedback_conversation (tests/core/feedback_conversation_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R05","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"Core share: header edits replace only the header span (or insert at the first line of a plain body, refusing quote/list/heading/fence/HTML/table/indented starts) and every unrelated byte, CRLF, UTF-8 and suggestion fence survives; recognition never changes bytes. Qt span conversion (BPR-1083) and draft binding (BPR-1082) remain.","command":"ctest --test-dir build/core-only -R core_feedback_header_edit (tests/core/feedback_header_edit_tests.cpp against the pack's fixtures/header-edit-cases.json)"}},{"criterion":"R07","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"Core share: roots from reply linkage alone; missing, deleted and ambiguous roots stay unknown with no reply promotion; 27 distinct pack counter units match their root label and classification_known.","command":"ctest --test-dir build/core-only -R core_feedback_conversation (tests/core/feedback_conversation_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R08","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"Core share: a refresh reuses annotations only on matching scope, post, body digest, segment and parser revision, re-parses only changed posts and reports only conversations whose root classification changed; the core performs no effect. Read-model adaptation is BPR-1087.","command":"ctest --test-dir build/core-only -R core_feedback_conversation (tests/core/feedback_conversation_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R41","outcome":"pass","revision":"79c171e036964c9b35d75cb6fc80c4c8d9c34f55","evidence":{"kind":"command","detail":"Core share: stale body revisions and draft revisions refuse old edits without losing the current draft; stale and cross-scope refreshes are refused by epoch and scope without changing the current set. Other R41 shares stay with BPR-1082, 1088, 1092, 1093 and 1101.","command":"ctest --test-dir build/core-only -R core_feedback_header_edit (tests/core/feedback_header_edit_tests.cpp against the pack's fixtures/header-edit-cases.json); ctest --test-dir build/core-only -R core_feedback_conversation (tests/core/feedback_conversation_tests.cpp against the pack's fixtures/counter-cases.json)"}}]
---

# BPR-1077: Implement revisioned header edits and root-post classification

Pack alias: **PACK-CC-03** (live `BPR-1077`, allocated at import).

## Outcome and work

Add pure header-edit decisions bound to the exact body revision, with byte spans suitable for deliberate Qt conversion. Resolve root posts using existing normalized identity/linkage. Update derived annotations when accepted bodies change; preserve reply-local annotations and missing/deleted root uncertainty.

## Acceptance

- No-op and round-trip source bytes, LF/CRLF, Unicode and patch suffixes are preserved.
- A root edit moves its own classification; a differently labelled reply does not.
- Stale revisions and scope replacements reject old edits/results without losing the current draft.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation evidence, 2026-10-05

Product commits `c4d9971a8e05fb11a3640970dd2c7c2c04c7d5eb` (header edits) and
`79c171e036964c9b35d75cb6fc80c4c8d9c34f55` (roots and the derived annotation set).
They add `feedback_header_edit.hpp`/`.cpp` and `feedback_conversation.hpp`/`.cpp` to
the Qt-free core, `feedback_body_segment` to `feedback_convention.hpp`, and the rows
`core_feedback_header_edit` and `core_feedback_conversation`. Both read the pack's
own fixtures. The [execution ledger](../../verification/RFC-BPR-0047-0051.md) records
the decisions (edited span, canonical form, no-op, plain insertion refusals,
read-back, root rule, reuse key, epoch staleness) and the full evidence.

- **Bytes preserved (R05).** All five `header-edit-cases.json` cases give their
  exact `after` bytes, including CRLF with a native `suggestion:-1+0` patch, an
  empty suggestion fence, a Unicode subject and the CRLF parse-only no-op. 3,924
  relabel and remove/re-add round trips over every label, the three forms, three
  decoration lists and LF/CRLF/Unicode/patch tails restore the original bytes.
  A no-op request rewrites nothing. 30,000 generated edits keep every byte outside
  one replaced span on code point boundaries, and each applied edit is idempotent.
- **Root decides (R07).** All 24 input orders of a thread whose root has the
  largest identifier and comes last classify the same root. Differently labelled
  replies, including a reply to a reply, never change it. Missing and deleted
  roots stay unknown and no reply is promoted. Two roots, duplicates, an
  unobserved parent, cycles and self-replies are ambiguous. All 27 distinct pack
  counter units match their root label and `classification_known`.
- **Website edits (R08).** A reply's edit re-parses only that reply and changes no
  conversation. A root's edit re-parses only the root and moves only its own
  conversation, and the replies keep their badges. Deletion, disappearance and a
  new owner segment each invalidate exactly the matching post. Nothing is written
  anywhere; the core has no effect.
- **Stale revisions and scopes (R41).** An edit chosen against another digest or
  parser revision is refused. Composed with the real `revisioned_review_draft`, a
  planned edit that arrives after the author typed is refused with `conflict`, and
  the newer draft is kept byte for byte. A command for another draft scope is
  refused and the draft is untouched. A refresh computed before another was
  adopted, or for an account that has since been replaced, is refused and the
  current set is kept. Nothing is carried across accounts or pull requests.
- **Negative controls.** Twelve reverted mutations each failed at the expected
  check. They are listed in the ledger.

Limitations: these are macOS arm64 AppleClang core-only Debug runs only, and the
Linux/Windows station runs are owed. Nothing calls the new APIs yet: draft and
store binding is BPR-1082, read-model adaptation BPR-1087, the Qt byte-span
conversion BPR-1083, and partition and coverage BPR-1078. R05, R07, R08 and R41 are
met here for the core decisions only.

**Note: Linux run at `d75570840` (holds `b9c8f2587`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, the `core-only` preset, Debug, `build/bpr1077`).** The
whole tree built with no GCC warning or error, and 7 of 7 passed:
`core_feedback_convention` (2.2 s), `core_feedback_header_edit` (1.5 s),
`core_feedback_conversation`, `core_review_composition`,
`core_review_tour_markdown`, `core_dependency_boundary` and
`core_boundary_contract`. Nothing needed a fix. `build/bpr1077` was removed.
Done is not reopened.

### Windows core-only (bpr-main-win, 2026-10-05)

Windows 11 Home 10.0.26200, MSVC 14.51, at `34ab71337` (which holds
`8508f0c9a`): `cmake --preset core-only -B build/bpr1077`, the whole tree
built.

MSVC first refused `src/core/src/feedback_convention.cpp:500`, with errors
C2440 and C3535 on `const auto* found = std::ranges::find_if(catalog, …)`.
Over a `std::array`, MSVC's STL returns an iterator class, not the raw pointer
libstdc++ and libc++ return. This also broke every Windows build of `main`.
The fix, `const auto found`, is BPR-1077's MSVC fix.

With it, the whole core-only tree builds with no `warning C` or `warning LNK`
line. `ctest -R "core_feedback_|core_review_composition|core_review_tour_markdown|core_dependency_boundary|core_boundary_contract"`
passes **8 of 8**: `core_feedback_convention`, `core_feedback_header_edit`,
`core_feedback_conversation`, `core_feedback_partition`,
`core_review_composition`, `core_review_tour_markdown`,
`core_dependency_boundary` and `core_boundary_contract`.
