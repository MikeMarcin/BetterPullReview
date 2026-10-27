# RFC-BPR-0083 proposed owner amendment: drop libgit2's station measurements, 2026-10-26

**Status: adopted 2026-10-26.** The owner answered **Accept** in chat on
2026-10-26, through a one-question Accept/Reject prompt. The live sources
listed below were changed in the same commit that recorded the answer. The
live RFC carries the adopted criterion wording, marked *Amended 2026-10-26.*;
the "After" text below states its substance.

The owner asked, in chat on 2026-10-26: "Draft owner amendments for
RFC-BPR-0081 and RFC-BPR-0083 that drop the outcome-fixed Windows/Linux station
measurements (BPR-1443, 1444, 1462, 1463), and close those cards if I accept."

## Question

**Accept or Reject:** RFC-BPR-0083 drops the Windows and Linux measurements of
the rejected candidate L (libgit2): its canary results and its size and
build-time deltas there. K, the shipped readers, keeps its canary obligation
on all three platforms. BPR-1463 closes on v0.3.4's Linux evidence for K.
BPR-1462 stays open, narrowed to K's Windows containment canary.

## Only half of these cards is outcome-fixed

The [decision record](../../../verification/RFC-BPR-0083-decision.md) keeps
K for every component. L fails H1, H2, H3 and H6 on macOS, V fails H6, G is
rejected, and no candidate is eligible anywhere. L's Windows and Linux
results therefore cannot change a verdict, and that part of both cards is
outcome-fixed.

K is different. K is what ships. The decision record lists "K's H2 on
Windows and Linux" first under "What could change a verdict", because "a
failure there is a defect to fix in K". That is a real product check, so
this amendment does not drop it.

- **Linux.** K's half has in effect already run. `git_metadata_harness`,
  `review_cycle_git_readers`, `review_cycle_git_reader_scale` and
  `core_review_cycle_manifest` pass in v0.3.4's Ubuntu 26.04 Release and
  Debug suites at `62bfc09f6`, against Git 2.53. That source includes
  BPR-1460's harness. The registered row runs the canary course with no
  `--no-canary`, and it passes only when there are no unclassified
  divergences and no writes and the canary passes
  (`tests/workspace_contract/git_metadata_harness.cpp`: `Unclassified == 0
  && Writes == 0`, and `expect(canary, "no canary fired")`). CTest truncates
  the passing output at 1 KiB, so the per-component `GMR summary` and
  `GMR canary` lines were not kept. The pass proves those results, but their
  counts are not on record.
- **Windows.** K's harness and divergence classification pass in v0.3.4's
  Windows 11 Release and Debug suites, against Git 2.55. The canary course
  does not run there: on `_WIN32`, `run_canary` prints "GMR canary skipped:
  the FIFO and symlink canaries are POSIX" and returns success. So
  containment, no-execution and no-write for the shipped readers on Windows
  have never been observed. Nobody has tried an `include.path` that names a
  named pipe, or `.gitignore` and `.gitattributes` replaced by a symbolic
  link or junction that points outside the root. That is the work BPR-1462
  keeps.

## Live sources this would amend

| Source | Content identity before the change |
|---|---|
| `docs/rfcs/RFC-BPR-0083-git-metadata-readers.md` at `7c2110199` | SHA-256 `47772c23a8934eeddece83f1ca89bf5051b800f7c76a73fa3a4f2f4876579bdb` |
| `docs/issues/20-ready/BPR-1462-windows-run-the-git-metadata-harness-and-canaries.md` | SHA-256 `060e6ff4153876dcb11ae6c0ab16fbb5431e2b8439604afcf97d45567a0ce6b6` |
| `docs/issues/20-ready/BPR-1463-linux-run-the-git-metadata-harness-and-canaries.md` | SHA-256 `f58fb63960c8212fe24255ca06eacb941dc57c25da1f885478845cb932d0cbc7` |
| `docs/issues/10-backlog/BPR-1464-close-rfc-0083.md` | SHA-256 `c5e46d43b27c73c3fa6f7bd0c778bfa3a0b28a751d7c1018116d9b42f5ddd9de` |
| `docs/verification/RFC-BPR-0083-decision.md` | SHA-256 `f9f106cceda330890eed6d4d080887eff5bb0bccc8e3280eda0fa448556c18c5` |
| `docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json` | SHA-256 `ad0563d023800ec51a50cd4c7441d76493505f5adf07b5711c2be47bf98f149c`; unchanged |

## Before and after

**AC03.** Before: "The extended canary course records containment,
no-execution and no-write results for K and for every L component evaluated,
on macOS, Linux and Windows." After: "The extended canary course records
containment, no-execution and no-write results for K on macOS, Linux and
Windows, and for every L component evaluated on macOS (owner amendment,
2026-10-26)."

**AC05.** Before: "the binary-size and build-time deltas for each platform".
After: the macOS deltas already recorded for L. The licence, SPDX, trial
packaging, SHA-256, reftable status and integration-size records are
unchanged. V's and G's desk-check records are unchanged.

**H2 and H5, as applied to L.** These are measured on macOS only. L fails
H2 there already, and H5 cannot make an ineligible candidate eligible. H2
for K still applies on all three platforms.

**Decision record.** The sentence "the Windows and Linux halves of H2 and H5
belong to BPR-1462 and BPR-1463 and are pending" becomes: K's Linux H2 is
observed by v0.3.4's suites as described above; K's Windows H2 is pending on
BPR-1462; L's station halves are not measured, by this amendment.

## Preserved obligations

- AC01, AC02, AC04, AC06 and AC07, the decision rules, every verdict and the
  re-run triggers (reading reftable, libgit2 2.0) are unchanged.
- Q3 is still not asked.
- No macOS result is reinterpreted. Nothing claims a Windows canary result
  for K, or any station result for L.
- If a later re-run admits a new candidate, that candidate owes all three
  platforms, as originally written.

## Consequences on Accept

- The live RFC gains an "Owner amendment, 2026-10-26" section with the AC03
  and AC05 text above. The decision record's pending note is updated as
  above.
- [[BPR-1463]] moves to Done. AC1 passes for K on the v0.3.4 Linux evidence,
  with the truncated-output limit stated; its L half and AC2 are dropped by
  amendment.
- [[BPR-1462]] stays Ready, retitled "Windows: extend the Git metadata canary
  course to K". AC1 becomes: on native Windows, the canary course has
  Windows equivalents of the FIFO and symlink canaries (a named-pipe or
  outside `include.path`, and rule files replaced by a symbolic link or
  junction pointing outside the root), and it records containment,
  no-execution and no-write for K, with a negative control showing that each
  canary fires. Its libgit2 build and AC2 are dropped. A containment failure
  is a K defect fixed under this card or under a card it names.
- [[BPR-1464]] drops BPR-1463 from `depends_on` and keeps BPR-1462.
- The milestone plan is unchanged.
