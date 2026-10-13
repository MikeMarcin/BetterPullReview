# RFC-BPR-0080 idle-machine waiver, 2026-10-13

Mandate `hashing-replacement-r3`. The owner answered through the coordinating
session on 2026-10-13. This record follows the author-rfc amendment
convention; the integrator published it into the live RFC in the same change.

## Live sources amended

| Source | Content identity before the change |
|---|---|
| `docs/rfcs/RFC-BPR-0080-hashing-replacement.md` at `2049c2da6` | SHA-256 `9c3b161b7f654b980fd1aa399ca32ac7740bbb9bf30743193b4dd7c9f6e0b31e` |
| `docs/issues/50-blocked/BPR-1446-evaluate-hash-candidates-on-macos.md` | SHA-256 `7920856ea92054ffb488bb6fff390e2d1d79b3aac4da24a138e3f8c8a46a62ac` |
| `docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json` | unchanged |

## Owner answer (as relayed, 2026-10-13)

**Waive the idle-machine condition: Accept.** The loaded macOS speed
measurements are accepted for the adopt rules.

## Before and after

**Evaluation, throughput conditions.** Before: "a Release `-O3` tests-on tree,
an idle machine, `-j1`, and medians of 7." After: unchanged for future
stations, with a marked note that, for the macOS arm64 rows, the owner accepted
the measurements [[BPR-1446]] took on a loaded machine (thread CPU time,
medians of 7, two runs) for the sha1dc, HACL* and host-port throughput rules.

**BPR-1446 AC4.** The idle condition is satisfied by the waiver for the macOS
rows. The waiver covers load only: every row AC4 names is still measured,
under load.

## Preserved obligations

- The ratio thresholds (1.0x, 0.9x, 3x) and every other rule and criterion are
  unchanged.
- Windows and Linux stations still measure under the RFC's conditions; the
  waiver names the macOS loaded measurements only.
- AC05's before-and-after window-thread rows for an adopted change are not
  waived.

## Consequences

BPR-1446 can finish on the loaded numbers once its remaining rows are
measured. BPR-1450, BPR-1451 and BPR-1447 then have their dependency met.
