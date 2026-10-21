---
schema: "implement-rfc.card.v1"
id: "BPR-1692"
title: "Read linguist-generated from the head's .gitattributes"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1689", "BPR-1691"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1689--BPR-1693--627bb0b13945.json","milestone":"RFC-BPR-0089 M2","owner_decision":{"date":"2026-10-21","session":"e081a94ec80b52c1ecb067710c8983cc","candidate":"RFC-BPR-0089-M2-attribute-matcher","revision":"rfc-bpr-0089-m2-matcher-q1","decision":"62bc634e-549e-4956-9f52-2ebbc6a0d99f","outcome":"accept","external_state":"/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0089-m2-matcher-v001","lifecycle":"closed; host stopped"},"frontend_design":"frontend-design:frontend-design loaded before the Overview heading's not-read reasons were written","related_cards":["BPR-1689","BPR-1691","BPR-1693"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"879fda2fbbb0281459db0093fa15386278ac126f","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|application_commit_filter_model|core_generated_files|commit_filter_fixture_contract)$' && python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix bpr1692-live1 --generated --cleanup","detail":"Fixture, shipped composition, offscreen: commit-filter-attributes-v1 commits `gen/** linguist-generated` at the root and `item_007.txt -linguist-generated` in gen/.gitattributes. Whole PR lists 3 of 98; the Overview and Files drawer trees end with '95 generated files hidden', each because the attribute marks it, and gen/item_007.txt is back in the tree; the heading says 95 generated set aside; Next file walks the three ordinary files; after a preparation round src/generator.in's head blob is read and no set-aside head blob is. Hide C1 and Show only C3 keep the marked paths behind the row; Clear all filters leaves the 95 there; Show as normal and Treat as generated still outrank the attributes. Negative control: without the composition's attribute read the course fails at opening (98 files, nothing set aside). Live, BPR-Test #113 (sparse gh review, signed in as MikeMarcin, closed and both branches deleted): 325 files, C2 regenerating 320; git check-attr --source C3 marks 319 (the nested -linguist-generated returns gen/0007.txt, the deeper file outranks a root =false). Whole PR, Show only C1, C2 and C3, Hide C2 and Clear all filters each list exactly the shown set less the 319 and set aside exactly the rest; the Files drawer walks the 6; GitHub's Viewed state is unchanged for 325 files; X-RateLimit-Remaining stayed above 4,870. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"879fda2fbbb0281459db0093fa15386278ac126f","evidence":{"kind":"command","command":"ctest --preset dev -R '^git_metadata_harness$'","detail":"The RFC-BPR-0083 harness gains component C6-generated: the shipped head reader (core::read_generated_attribute_rules over every ancestor's committed .gitattributes) against git check-attr linguist-generated. It agrees on all 43 questions of the new linguist-generated case (nested files, a root macro, a macro a nested file may not define, `**`, anchored, directory and quoted patterns, -linguist-generated, !linguist-generated, =false, =true and a case-different path) and on the attributes (40) and ignorecase-false (110) cases; no checkout written. core_generated_files checks the same corpus and the bounds (64 files, 512 KiB, sorted normalized directories) under core-only. macOS."}}]
---

# BPR-1692: Read linguist-generated from the head's .gitattributes

RFC-BPR-0089 M2, rule 2 of "Generated files are set aside": `linguist-generated`
at the pull request's head decides a file the reviewer has not marked.

## Owner decision

AGENTS.md asks the owner before a general-purpose matcher is written in core.
In attended session `e081a94ec80b52c1ecb067710c8983cc` (external state
`/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0089-m2-matcher-v001`),
at candidate revision `rfc-bpr-0089-m2-matcher-q1`, the owner chose **Accept**
with no note (decision `62bc634e-549e-4956-9f52-2ebbc6a0d99f`, 2026-10-21):
reuse the core's existing first-party wildmatch and attribute reader
(`bpr::core::wildcard_match`, `workspace_attribute_rules`, kept for C5 and C6
by the [RFC-BPR-0083 decision record](../../verification/RFC-BPR-0083-decision.md)),
extended by one attribute, and adopt no library. Git's GPL-2.0-only
`wildmatch.c`, libgit2's `wildmatch.c` or libgit2 behind an adapter, RE2 by
translation, and fnmatch(3), Qt or gix-glob were not adopted. The session is
closed: its observation records the decision's use, its protection is released
and its host is stopped.

## Measured

GitHub's Files changed reads the head commit's rules everywhere, including
each per-commit view (BPR-Test #112; see the RFC's engineering questions).

## Change

- **Core.** `workspace_attribute_rules` answers `linguist-generated` as it
  answers `filter`, through one attribute lookup with the same precedence and
  macros. `core::read_generated_attribute_rules` wraps the head's files as the
  M2 `generated_attribute_rules`: case-sensitive, no `info/attributes`, at most
  64 files of at most 512 KiB, sorted normalized directories, and says which
  bound refused them.
- **Host.** `epoch_application_host::resolve_generated_attributes` looks up
  `<directory>/.gitattributes` at the accepted head by literal path for the
  root and every ancestor of the review's paths, in the retained cache or a
  sparse review's chain store, and reads each blob; a sparse review's missing
  blob is read exactly through its own lease and checked against its OID. It
  answers which bound stopped it (and the directory of a file past 512 KiB).
- **Model and composition.** The commit-filter model asks once per head and
  directory set (the comparison's paths and those the chain's tree diffs
  name), adopts the core's rules, and forgets them with the head. When the
  rules are not read, the Overview's files heading says why
  ("linguist-generated not read: more than 64 .gitattributes files", and so
  on); only the reviewer's marks set files aside then.
- **Evidence.** Fixture `commit-filter-attributes-v1`
  (`scripts/build_commit_filter_fixture.py --attributes`, checked against
  `git check-attr --source`), a third course in
  `application_commit_filter_public_path`, the `C6-generated` component in
  `git_metadata_harness`, and `--generated` for the M1 live course
  (`live_second_account_course --course generated`).

Shared files touched: `github_application.cpp` (pins resealed),
`epoch_application_host.cpp`, `pull_request_overview_page.cpp`.

## Acceptance

- **AC1.** RFC AC04's attribute half: with `linguist-generated` on C2's paths,
  Whole PR and every filter leave those paths out of the trees, Next file and
  remaining counts, behind the generated row, unprepared, and Clear all
  filters leaves them there; `-linguist-generated` returns a path to normal.
  Fixture and live, through the shipped composition.
- **AC2.** The matcher agrees with `git check-attr linguist-generated` over a
  corpus that includes nested files, macros, `**`, anchored and directory
  patterns (the RFC-BPR-0083 harness, extended).

## Not in this card

- Opening a set-aside file's diff from the row on request: [[BPR-1693]].
- Windows and Linux are unmeasured.
