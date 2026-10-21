---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-J1"
title: "Decide generated files, file filters and the bulk label in the core"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M2","related_cards":["BPR-PENDING-J2","BPR-PENDING-J3","BPR-PENDING-J4"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"command","command":"cmake --preset core-only && cmake --build --preset core-only --target test_generated_files test_commit_filter && ctest --preset core-only -R '^core_(generated_files|commit_filter)$'","detail":"Generated precedence: Show as normal and Treat as generated outrank linguist-generated (set, a value other than false, unset, =false, unspecified); Review anyway returns a generated file without changing what it is; gestures normalize and refuse invalid files and malformed marks; every path decides once and equal inputs decide equally; nothing is generated without a rule or a mark; the attribute directories are the root and each ancestor, parents first. Attribute rules in the tests name exact paths only. Hide files these commits changed adds exactly the selected commits' touched paths, combines, ignores commits the span does not cover and refuses an empty, unlisted or malformed input; the bulk label needs 100 files. All under core-only, without Git or Qt. macOS."}}]
---

# BPR-PENDING-J1: Decide generated files, file filters and the bulk label in the core

RFC-BPR-0089 M2, the decisions behind the generated row and the file filters.

## Change

- `bpr/core/generated_files.hpp`: the reviewer's marks per review
  (`generated_file_mark`: Show as normal, Treat as generated, Review anyway),
  their gestures (`apply_generated_gesture`, normalized: a mark that says
  nothing is not kept) and menu availability, and `decide_generated`: a mark
  outranks `linguist-generated` at head, which is read behind the explicit
  matcher boundary `generated_attribute_rules`. Nothing else marks a file
  (owner, Q4). `decide_generated_files` decides a review's paths, with no
  rules while the attributes are unread. `generated_attribute_directories`
  names where the host looks for `.gitattributes`, within 64 files of 512 KiB.
- `bpr/core/commit_filter.hpp`: `file_filter_state` and
  `hide_files_changed_by` ("Hide files these commits changed": every path the
  selected commits change, through a merge for a side commit), and
  `bulk_commit` (100 files or more). The touch-set check is shared with
  `filter_paths`.

## Acceptance

- **AC1.** The generated precedence, the file filter and the bulk label are
  decided under `core-only` (RFC AC04's decisions, AC08's M2 part).

## Not in this card

Pattern matching for `linguist-generated`: [[BPR-PENDING-J4]], parked on the
owner's library answer.
