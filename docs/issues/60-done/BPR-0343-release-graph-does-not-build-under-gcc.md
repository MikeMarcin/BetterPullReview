---
id: BPR-0343
title: Make the tests-disabled Release graph build and verify under GCC
status: Done
rfc: RFC-BPR-0013
completed: 2026-08-18
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

The hostile-endpoint case still says `private` under GCC, so it is unaffected.

## Corrected — three cases are affected, and the assertion never proved the seal

Two claims above are wrong and are corrected here rather than edited away.

**It is not only the grant case.** Three of the five hostile snippets construct
`review_mutation_execution_authority`, which the public header only
forward-declares. All three are therefore rejected for incompleteness before
access control is ever consulted, and none of the three diagnostics contains
`private`:

| Snippet | Required by the assertion | GCC 15.2 | Clang 21.1 |
|---|---|---|---|
| `hostile-endpoint` | `private` | `is private within this context` | `calling a private constructor` |
| `hostile-grant` | `private` | `invalid use of incomplete type` | `allocation of incomplete type` |
| `hostile-attended-authority` | `private` | `invalid use of incomplete type` | `allocation of incomplete type` |
| `hostile-bootstrap-definition` | `redefinition\|private` | `invalid use of incomplete type` | `allocation of incomplete type` |

The suite stops at the first failure, which is why only the grant case was
seen. The other two are behind it.

**It is not a GCC-versus-Clang difference.** Clang 21.1 on Linux produces the
same incomplete-type wording GCC does, so the wording the assertion demands is
not what either compiler emits.

**The assertion was passing on macOS for a reason unrelated to the seal.** It
searches the whole captured output, and every diagnostic line is prefixed with
the temporary source path. `tempfile.TemporaryDirectory()` on macOS returns a
path under `/private/var/folders`, so the substring `private` is present no
matter what the compiler said. Compiling `hostile-grant.cpp` from a directory
named `/private/var/folders/xy` reproduces this directly: `assertIn("private",
...)` passes while no diagnostic message contains the word.

So the repair is not only to widen the accepted wording. The assertion must
read the diagnostic messages rather than the raw output, or it can keep passing
on a path spelling after the seal is gone.

## Required final state

- The tests-disabled Release graph builds under GCC 15.2 on Ubuntu 26.04 with
  the project's warning set intact.
- The seal assertions accept any diagnostic that proves the entity was
  unusable, and still reject a snippet that compiles. A test that merely
  checked for a nonzero exit status would pass if the snippet failed for an
  unrelated reason such as a typo, so the replacement must still name the
  sealed entity.
- `application_composition_contract` passes on Linux and macOS.

## Result — 2026-08-18

Both halves are repaired, and each turned out to be larger than the card
described.

### The Release graph

The policy question the card refused to decide alone was already decided.
[[BPR-0064]] met this class and settled it: restructure so the proof is local,
do not suppress. That answer is applied here unchanged, so the project still
has one answer rather than two.

The GitHub adapter's success return built an anonymous aggregate and let it
convert into the enclosing `expected`. GCC named that temporary as the
uninitialized object, because after inlining optional access, aggregate
initialization and `~basic_string` together it could no longer tie the
temporary's string subobject to the one path the guard admits. Populating a
named, value-initialized observation puts construction, assignment and
destruction in one block.

Repairing it exposed a second site with the same cause, in
`src/adapters/sqlite/src/review_mutation_recovery_store.cpp`. It had never been
reached: the Release graph stopped at the GitHub adapter, so nothing downstream
of that object file had ever been compiled here. Naming the object was not
enough there — it was already named, and still aggregate-initialized in place —
so the same value-initialize-then-populate shape was applied.

No warning was disabled, no `-Werror` weakened, no accepted input changed.

### The seal assertion

Three of the five hostile snippets were affected, not one, and the assertion
was not proving what it claimed on either platform. It searched the whole
captured output for a word, and every diagnostic line carries the temporary
source path — which on macOS is under `/private/var/folders`. Compiling
`hostile-grant.cpp` from a directory named `/private/var/folders/xy` on Linux
reproduces the false positive exactly: `assertIn("private", …)` passes while no
diagnostic message contains the word.

Each hostile snippet now has to fail, have a diagnostic that **names the sealed
entity**, and have that same diagnostic give a reason the entity was unusable.
Reasons are read from the message text after `error:`, so a path can no longer
satisfy one.

## Evidence

- The tests-disabled Release graph configures, compiles and **links** under GCC
  15.2.0 at `-O3` with `-Werror -Wall -Wextra -Wpedantic -Wconversion
  -Wsign-conversion`, `_FORTIFY_SOURCE=3` and `-fstack-protector-strong`
  intact. Ubuntu 26.04 x86_64. Before the repair the same tree stopped with
  two `-Werror=maybe-uninitialized` diagnostics at
  `github_mutation_contract.cpp:839`; a `-k 0` sweep afterwards showed one
  remaining failing edge, the sqlite site, and none after that.
- `application_composition_contract` passes on Linux: `Ran 5 tests in 223.8s`,
  `100% tests passed out of 1`. Reaching that verdict also required
  [[BPR-0368]], two digest pins the same earlier slice had left stale behind
  this compile failure.
- The seal suite passes under **both** compilers on this host — GCC 15.2.0 and
  Clang 21.1.8 — which matters because Clang emits the same incomplete-type
  wording GCC does, so the repaired assertion is not merely GCC-shaped.
- A new control proves the assertion still rejects what does not prove a seal:
  a snippet that compiles, a snippet that fails over a typo, and a snippet that
  names the sealed entity on its way to an unrelated error. All three must make
  `assert_language_sealed` raise, and the control fails if any stops doing so.
- `github_mutation_contract` 1/1; the widened mutation and recovery set,
  including `sqlite_review_mutation_recovery_storage`, 10/10.
- Both changed translation units also compile clean under Clang 21.1.8 with the
  project's exact warning set, replayed from `compile_commands.json`. Neither
  change is GCC-specific.
- Both changed files are format-clean under clang-format 22.1.2, the version
  [[BPR-0367]] establishes the tree is formatted for. They were matched by hand,
  because the provisioned Linux toolchain supplies 21.
- `python3 scripts/verify_documentation.py --root .` reports `DOCUMENTATION_OK`;
  `documentation_structure` and `documentation_issue_graph_contract` pass 2/2;
  `git diff --check` passes.

### Claimed and not verified here

The card requires `application_composition_contract` to pass **on Linux and
macOS**. Only Linux was executed; this session has no macOS host, and the board
has no `blocked_by` value for macOS because macOS has been the ordinary
development host rather than an absent platform.

What reduces the risk, stated so it can be checked rather than assumed: the
seal suite passed under Clang here, which is the compiler family macOS uses;
both changed translation units compile clean under that Clang with the full
warning set; and the digest pins [[BPR-0368]] repaired are LF-normalized, so
they cannot differ by platform. What remains genuinely unproven is the
macOS-native Release composition and its byte scan. Whoever next runs the
contract on macOS should confirm it, and this card should be reopened if it
disagrees.
