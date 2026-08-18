---
id: BPR-0343
title: Make the tests-disabled Release graph build and verify under GCC
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# Make the tests-disabled Release graph build and verify under GCC

`application_composition_contract` fails on Linux for two independent reasons.
Both are GCC-versus-Clang differences in code that is otherwise correct, and
neither is reachable from the Debug `dev` preset.

## The Release graph does not compile

The contract configures and builds a tests-disabled Release graph. That build
fails one translation unit under GCC 15.2 at `-O3` with warnings as errors:

```
github_mutation_contract.cpp:839: error: '...ServerCommentId..._M_p'
may be used uninitialized [-Werror=maybe-uninitialized]
```

inlined through `~basic_string` into `parse_review_comment_response`.

The guard is present and correct. Every optional the function dereferences,
`node` included, is checked at line 819 before the aggregate at line 841
reads it. GCC loses the correlation between the guard and the use after
inlining `std::optional` access, aggregate initialization, and string
disposal together, which is a well-known weakness of this diagnostic at higher
optimization levels. The Debug preset does not inline enough to trigger it, so
`dev` builds clean.

This is the same class BPR-0064 recorded: a defect visible only in a fresh
GCC Release composition.

Deciding how to answer it is the work, and it is a policy choice this card
must not make alone. Restructuring the return so the guard survives inlining
keeps the diagnostic enabled everywhere. Suppressing it narrowly admits a
known false positive. Whichever is chosen should be applied the way BPR-0064's
outcome was, so the project keeps one answer to this class rather than two.

## A seal assertion reads one compiler's wording

`test_production_mutation_authority_issuers_are_language_sealed` compiles
hostile snippets, requires each to fail, and then requires the diagnostic text
to contain `private`.

GCC rejects the hostile grant, so the seal holds. It reports
`invalid use of incomplete type 'review_mutation_execution_authority'`
instead, because the class is only forward-declared at that point and
incompleteness is diagnosed before access control. The assertion, not the
seal, is what fails.

The hostile-endpoint case still says `private` under GCC, so only the grant
case is affected.

## Required final state

- The tests-disabled Release graph builds under GCC 15.2 on Ubuntu 26.04 with
  the project's warning set intact.
- The seal assertions accept any diagnostic that proves the entity was
  unusable, and still reject a snippet that compiles. A test that merely
  checked for a nonzero exit status would pass if the snippet failed for an
  unrelated reason such as a typo, so the replacement must still name the
  sealed entity.
- `application_composition_contract` passes on Linux and macOS.
