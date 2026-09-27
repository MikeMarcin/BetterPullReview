---
schema: "implement-rfc.card.v1"
id: "BPR-0564"
title: "Implement the canonical immutable code-permalink representation"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0563"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"71a6e54b82812e4fbf657d358b4eb69f52949f00","evidence":{"kind":"command","detail":"Real app and standalone codec build pass. Canonical, hostile, SHA-256 and GHES codec fixtures plus core, composition, architecture and isolation selectors pass 5/5. Exact 64 KiB boundary and byte-preserved Unicode prose are exercised; parsing requires retained endpoint provenance and produces no mutation. Targeted tidy/format and documentation gates pass.","command":"ctest --preset dev -R '^(core_review_round_trip|github_review_comment_permalink|application_review_round_trip_composition_contract|architecture_boundaries|live_mutation_isolation_contract)$' --output-on-failure"}},{"criterion":"AC2","outcome":"pass","revision":"71a6e54b82812e4fbf657d358b4eb69f52949f00","evidence":{"kind":"command","detail":"Real app and standalone codec build pass. Canonical, hostile, SHA-256 and GHES codec fixtures plus core, composition, architecture and isolation selectors pass 5/5. Exact 64 KiB boundary and byte-preserved Unicode prose are exercised; parsing requires retained endpoint provenance and produces no mutation. Targeted tidy/format and documentation gates pass.","command":"ctest --preset dev -R '^(core_review_round_trip|github_review_comment_permalink|application_review_round_trip_composition_contract|architecture_boundaries|live_mutation_isolation_contract)$' --output-on-failure"}},{"criterion":"AC3","outcome":"pass","revision":"71a6e54b82812e4fbf657d358b4eb69f52949f00","evidence":{"kind":"command","detail":"Real app and standalone codec build pass. Canonical, hostile, SHA-256 and GHES codec fixtures plus core, composition, architecture and isolation selectors pass 5/5. Exact 64 KiB boundary and byte-preserved Unicode prose are exercised; parsing requires retained endpoint provenance and produces no mutation. Targeted tidy/format and documentation gates pass.","command":"ctest --preset dev -R '^(core_review_round_trip|github_review_comment_permalink|application_review_round_trip_composition_contract|architecture_boundaries|live_mutation_isolation_contract)$' --output-on-failure"}},{"criterion":"AC4","outcome":"pass","revision":"71a6e54b82812e4fbf657d358b4eb69f52949f00","evidence":{"kind":"command","detail":"Real app and standalone codec build pass. Canonical, hostile, SHA-256 and GHES codec fixtures plus core, composition, architecture and isolation selectors pass 5/5. Exact 64 KiB boundary and byte-preserved Unicode prose are exercised; parsing requires retained endpoint provenance and produces no mutation. Targeted tidy/format and documentation gates pass.","command":"ctest --preset dev -R '^(core_review_round_trip|github_review_comment_permalink|application_review_round_trip_composition_contract|architecture_boundaries|live_mutation_isolation_contract)$' --output-on-failure"}},{"criterion":"AC5","outcome":"pass","revision":"71a6e54b82812e4fbf657d358b4eb69f52949f00","evidence":{"kind":"command","detail":"Real app and standalone codec build pass. Canonical, hostile, SHA-256 and GHES codec fixtures plus core, composition, architecture and isolation selectors pass 5/5. Exact 64 KiB boundary and byte-preserved Unicode prose are exercised; parsing requires retained endpoint provenance and produces no mutation. Targeted tidy/format and documentation gates pass.","command":"ctest --preset dev -R '^(core_review_round_trip|github_review_comment_permalink|application_review_round_trip_composition_contract|architecture_boundaries|live_mutation_isolation_contract)$' --output-on-failure"}}]
---

# Implement the canonical immutable code-permalink representation

Implement bounded compile and parse contracts for the same-host, same-repository, full-commit line permalink and the visible fallback body grammar.

## Acceptance

- Compiler emits canonical web-origin URLs, one-based line fragments, and `?plain=1` only for Markdown source.
- Path segments are encoded without allowing authority changes, userinfo, alternate ports, encoded separators, dot segments, arbitrary query, or fragment injection.
- Parser accepts exactly one leading canonical link, one blank line, and non-empty byte-preserved prose.
- Authority, commit, path, blob, and line are validated before an anchor is produced.
- Invalid, foreign, ambiguous, oversized, or malformed input returns no anchor and performs no mutation.

## Verification

- Producing source: `71a6e54b82812e4fbf657d358b4eb69f52949f00`. `github_review_comment_permalink`
  passes within a 5/5 final course with core, public composition, architecture
  boundaries and live-mutation isolation. The real app and codec test build pass.
- Compiler/parser bind to an epoch-host sealed right endpoint, exact authority,
  comparison, path/blob and observed text-line count. Compiler requires current
  head; parser permits exact retained historical source. Caller still admits
  only a file-subject root in the expected PR/changed file; parse grants no write.
- Cases cover Unicode segment escaping, Markdown/plain URLs, SHA-1/SHA-256,
  connected GHES origin, first/last/out-of-range lines, exact body-size bound,
  byte-preserved prose, foreign authority/repository, abbreviated OIDs,
  userinfo/ports, encoded separators/dot/query/fragment injection, ambiguous
  second links, malformed UTF-8, and unavailable or mismatched material.
- Changed-source review found no provider calls or second mutation path. Narrow
  codec and core tidy, the prior host tidy checks, formatting and documentation
  gate pass. Existing legacy fixture-header tidy violations were observed in
  the initial broad test TU check; the isolated codec target avoids importing
  those unrelated headers. All introduced tidy diagnostics were corrected.
- [Acceptance evidence](../_checkpoints/BPR-0575/bpr0564-acceptance.json)
  records the focused command. Public provider/normalizer integration remains
  in the authored B/C milestones; live evidence remains the D obligation.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
