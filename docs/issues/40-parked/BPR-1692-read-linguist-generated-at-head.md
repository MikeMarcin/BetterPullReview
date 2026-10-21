---
schema: "implement-rfc.card.v1"
id: "BPR-1692"
title: "Read linguist-generated from the head's .gitattributes"
status: "Parked"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1689", "BPR-1691"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1689--BPR-1693--627bb0b13945.json","milestone":"RFC-BPR-0089 M2","owner_question":"attended session e081a94ec80b52c1ecb067710c8983cc, candidate RFC-BPR-0089-M2-attribute-matcher revision rfc-bpr-0089-m2-matcher-q1, external state /Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0089-m2-matcher-v001","related_cards":["BPR-1689","BPR-1691"]}
parked_on: "human-review"
---

# BPR-1692: Read linguist-generated from the head's .gitattributes

RFC-BPR-0089 M2, rule 2 of "Generated files are set aside": `linguist-generated`
at the pull request's head decides a file the reviewer has not marked.

## Parked on

AGENTS.md asks the owner before a general-purpose matcher is written in core.
The question is open in attended session
`e081a94ec80b52c1ecb067710c8983cc` (external state
`/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0089-m2-matcher-v001`),
one Accept/Reject question at revision `rfc-bpr-0089-m2-matcher-q1`: reuse the
core's existing first-party wildmatch and attribute reader
(`bpr::core::wildcard_match`, `workspace_attribute_rules`, kept for C5 and C6
by the [RFC-BPR-0083 decision record](../../verification/RFC-BPR-0083-decision.md)),
extended by one attribute, with no new library. The alternatives it names are
Git's GPL-2.0-only `wildmatch.c`, libgit2's `wildmatch.c` or libgit2 behind an
adapter, RE2 by translation, and fnmatch(3), Qt or gix-glob.

## Measured

GitHub's Files changed reads the head commit's rules everywhere, including
each per-commit view (BPR-Test #112; see the RFC's engineering questions).

## Ready now

- `core::generated_attribute_rules` is the matcher boundary;
  `core::decide_generated_files` takes it, or none while the attributes are
  unread (then only marks decide, and nothing is set aside before a rule or
  gesture says so).
- `core::generated_attribute_directories` names the root and every ancestor
  directory whose `.gitattributes` the host must look for at head.
- `core::maximum_generated_attribute_files` (64) and
  `core::maximum_generated_attribute_file_bytes` (512 KiB) are the bounds.
- `commit_filter_model::adopt_generated_attributes` takes the rules; the
  projection's `AttributesRead` says whether they were read.

## Work once the owner answers

- If accepted: `workspace_attribute_rules` answers `linguist-generated` as it
  answers `filter` (same precedence and macros), wrapped as a
  `generated_attribute_rules`.
- The epoch host reads the head's `.gitattributes` blobs (literal paths, the
  chain store for a sparse review) within the bounds, and says why when a
  bound is exceeded.
- A fixture with `linguist-generated` on C2's paths and `-linguist-generated`
  on one, plus a live BPR-Test course of the owner's shape (C2 regenerating
  at least 300 files).

## Acceptance

- **AC1.** RFC AC04's attribute half: with `linguist-generated` on C2's paths,
  Whole PR and every filter leave those paths out of the trees, Next file and
  remaining counts, behind the generated row, unprepared, and Clear all
  filters leaves them there; `-linguist-generated` returns a path to normal.
  Fixture and live, through the shipped composition.
- **AC2.** The matcher agrees with `git check-attr linguist-generated` over a
  corpus that includes nested files, macros, `**`, anchored and directory
  patterns (the RFC-BPR-0083 harness, extended).
