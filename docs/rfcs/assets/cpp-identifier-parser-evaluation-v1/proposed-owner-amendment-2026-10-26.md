# RFC-BPR-0081 proposed owner amendment: macOS-only measurements, 2026-10-26

**Status: proposed, not adopted.** Nothing in the live RFC, its cards or its
plan changes until the owner answers Accept. On Reject, BPR-1443 and BPR-1444
stay Ready under the original AC03, AC04 and AC06.

The owner asked, in chat on 2026-10-26: "Draft owner amendments for
RFC-BPR-0081 and RFC-BPR-0083 that drop the outcome-fixed Windows/Linux station
measurements (BPR-1443, 1444, 1462, 1463), and close those cards if I accept."
This record follows the author-rfc amendment convention of
[the RFC-BPR-0080 amendment](../hashing-replacement-v1/proposed-owner-amendment-2026-10-13.md).

## Question

**Accept or Reject:** RFC-BPR-0081's evaluation of tree-sitter-cpp (T) is
complete on macOS alone. AC03, AC04 and AC06 drop their Windows x64 and
Linux x64 halves, BPR-1443 and BPR-1444 close without running, and BPR-1445
may close the RFC on the macOS evidence.

## Why the stations cannot change the outcome

On macOS, T fails three gates at both pins: Contract, Bounded work and Memory
([report](../../../verification/RFC-BPR-0081.md)). The Contract failure comes
from the grammar, which accepts no attribute on an enumerator, so the platform
cannot change it. The decision rules reject T if any gate fails, so a
Windows or Linux result cannot reject T a second time or bring it back. The
outcome is already **extend**, and [[BPR-1474]] (Done) carries the heuristic
fixes. The report says so itself: the station numbers "cannot change the
outcome".

What the stations would add is the cost of a rejected candidate on two more
platforms: grammar-pass times, the Windows and Linux executable sizes, and a
second 4 CPU-hour fuzz campaign for each pin on Linux. None of it reaches
a shipped binary. The default build never acquires or links tree-sitter
(AC01).

The shipped classifier is already covered on every platform without these
stations. `syntax_classification_contract`, `qt_syntax_pane_composition`,
`application_cpp_identifier_appearance`, `application_appearance_host`,
`syntax_refinement_cancellation` and `application_large_review_benchmark`
pass in v0.3.4's complete Windows 11 and Ubuntu 26.04 Release suites at
`62bfc09f6` ([v0.3.4 results](../../../verification/release-v034-results.md)).
The contract rows pin exact spans, so the baseline's cross-platform span
identity is checked by those rows and does not depend on this evaluation.

## Live sources this would amend

| Source | Content identity before the change |
|---|---|
| `docs/rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md` at `7c2110199` | SHA-256 `6773bd1ba5e68ed8da01e7d45b4329ddaf3815d3e243424cd5b31c757ef1012b` |
| `docs/issues/20-ready/BPR-1443-measure-the-parser-evaluation-on-windows.md` | SHA-256 `cc24f805c720ac7e267a9b482ffbb9b4a6df1e2569f113f2e1b6e7e19ecf6a8a` |
| `docs/issues/20-ready/BPR-1444-measure-the-parser-evaluation-on-linux.md` | SHA-256 `586ce35f0a0977a243bebe38b38693724a47a4441fc691df544184200c849c8a` |
| `docs/issues/10-backlog/BPR-1445-close-rfc-0081.md` | SHA-256 `6daa8c9aa0059e2a91b7341cb13b6b033cf30cc423852284df9ad1da843cb0fa` |
| `docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json` | SHA-256 `e0ed268187989e962d3679e9970e0e0082edece336ba9002e4a05c7bf70f81b2`; unchanged |

## Before and after

**Measurements, stations.** Before: "Runs use the macOS reference station and
the Windows x64 and Linux x64 stations." After, for this evaluation: the macOS
reference station's runs are the measurement record. A later re-run under the
report's re-run conditions measures all three stations again, as originally
written.

**AC03.** Before: "The macOS, Windows and Linux measurements cover every size
class and every item under "Measurements", and they measure the baseline in
the same runs." After: "The macOS measurements cover every size class and
every item under "Measurements", and they measure the baseline in the same
runs. Windows x64 and Linux x64 are not measured for this evaluation (owner
amendment, 2026-10-26)."

**AC04.** Before, its fuzz results were owed on macOS and Linux ("run for
4 CPU-hours per candidate on macOS and Linux"). After: the macOS 4 CPU-hour
campaigns for the baseline and both pins satisfy it. The Linux campaign is
not run. The cancellation test and `fuzz_syntax` stay, unchanged.

**AC06.** Before: "the size added per slice", including Windows and Linux.
After: the size added per macOS architecture slice, which the report already
records. The licence, notice, SPDX, corresponding-source, archive-digest and
build-cost records are unchanged.

**Determinism gate.** "Spans are identical across runs and platforms" is
measured across runs on macOS only. T is already rejected on three other
gates, so this has no effect on the outcome.

## Preserved obligations

- AC01, AC02, AC05 and AC07, the decision rules and their thresholds, the
  recorded outcome (extend) and the re-run conditions are unchanged.
- No macOS result is reinterpreted, and nothing claims a Windows or Linux
  measurement of T.
- Q1 stays unanswered and moot.
- If the evaluation is ever re-run (the refinement reaches 800 lines, a
  category needs structural recognition, the grammar gains enumerator
  attributes, or the heuristic's limits are reached), that run owes all
  three platforms again.

## Consequences on Accept

- The live RFC gains an "Owner amendment, 2026-10-26" section that carries the
  AC03, AC04 and AC06 text above and marks those criteria amended.
- [[BPR-1443]] and [[BPR-1444]] move to Done, not run, with this amendment as
  their disposition.
- [[BPR-1445]] drops both from `depends_on`. Its checklist rows for AC03, AC04
  and AC06 cite the macOS evidence and this amendment. The card then becomes
  eligible to close the RFC, which remains a separate closeout step with its
  own documentation gate.
- The milestone plan is unchanged. Its `windows-measurement` and
  `linux-measurement` milestones end by amendment, not by measurement.
