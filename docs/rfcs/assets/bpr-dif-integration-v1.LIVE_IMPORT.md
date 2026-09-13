# Dif integration v1: live import

Import-only record for dispatch `dif-import-20260913-1`, producer `/root/dif_import`, mandate revision 1. The original 56-file pack is retained byte-for-byte in [bpr-dif-integration-v1](bpr-dif-integration-v1/); `SHA256SUMS` and the pack verifier define its source integrity. Its incoming PNGs and review viewer are source and inspection aids, not selected runtime art or application acceptance evidence.

## Source, live inventory, and destinations

- Source: `/Users/jaeger/Downloads/BetterPullReview-Dif-Integration-RFC-Pack-v1`; original `milestone-plan.json` SHA-256 `7244e892ad21fe7e5df7ad163a214546a43ebe31396389753c5c8f13d724b806`.
- Pack snapshot: 2026-09-12 19:50:30 -07:00, complete dump SHA-256 `9694ccea15623c3f7ae6e5f45f642854d04301fc5639468c291d7ecfcb311e71`. The snapshot supplied no repository commit.
- Live pre-import `main` HEAD: `9ce46e3576a44441cfcd932fca128091275e0068`. All 26 recorded source paths existed; 24 matched their extracted SHA-256 exactly. `src/adapters/qt/src/workbench_window.cpp` changed from recorded `0344560b5cfce2239f66601877981dfaaedf600896859435cfdbc7ed80d71f98` to live `8c78f0b3d0e5e89eb9c7c95f3aa886ad65d0ee746ce5086b766ed75c8f7559db`; `src/app/src/github_application.cpp` changed from recorded `4a41db496e4dfcc70e4093479d31e3ba5c63456155194899bd697fc4957f2f86` to live `e49e77ed767bc19109a1a4ca2dc868ce10f0af22c638ec9456dc3d9db1a6133d`. Later implementation must reconcile those live seams; no application code was changed by this import.
- No live RFC-0030/0031 or destination pack/plan path existed at intake. Their IDs remain `RFC-BPR-0030` and `RFC-BPR-0031`; both [live RFCs](../RFC-BPR-0030-dif-production-animation-revision.md) are Proposed. Existing RFC-0019/0020 and their historical records remain intact. The 18 new cards are in `docs/issues/10-backlog/`, not claimed or finished.
- Allocation: `BPR-0752`–`BPR-0769`, next ID `BPR-0770`, from [the reservation](../../issues/_reservations/BPR-0752--BPR-0769--bebd5abb5692.json) committed at `1125a6e4e0e9bf5dc573cb3b8cfc2d24df0cd205`. No ID was recycled.
- Live [plan](../plans/bpr-dif-integration-v1/milestone-plan.json), [generated view](../plans/bpr-dif-integration-v1/MILESTONES.md), and [machine-readable alias map](../plans/bpr-dif-integration-v1/id-map.json) are adopted with two RFCs, all eighteen work refs, original card dependencies, six original milestone IDs, grouping, outcomes, dependencies, exits and boundaries.
- The overlay's consumer configuration was copied to `.mask/skill-config/sprite-sheet-animation-updater/project.md`; the bundled skill zip remains archival and was not installed. The already-dirty `.gitignore` and `.mask.json`, and unrelated BPR-0617, were not touched by this import.
- The live [combined closeout card](../../issues/10-backlog/BPR-0769-close-rfc-0031.md) retains the pack's independent-review work and both `closeout_for` relationships. Its filename/title follow the repository's `close-rfc` convention for the last dependent RFC, and its checklist records all A1–A8/B1–B12 criteria as **not run**. The live RFCs present those same twenty criteria as Markdown bullets so the existing documentation verifier can read them; the archived authored text is unchanged. Its immutable pre-implementation RFC base is the pre-import HEAD above. No closeout outcome is implied by this representation change.

## Complete alias mapping

| Pack alias | Live card |
|---|---|
| `PACK-DIF-01` | `BPR-0752` |
| `PACK-DIF-02` | `BPR-0753` |
| `PACK-DIF-03` | `BPR-0754` |
| `PACK-DIF-04` | `BPR-0755` |
| `PACK-DIF-05` | `BPR-0756` |
| `PACK-DIF-06` | `BPR-0757` |
| `PACK-DIF-07` | `BPR-0758` |
| `PACK-DIF-08` | `BPR-0759` |
| `PACK-DIF-09` | `BPR-0760` |
| `PACK-DIF-10` | `BPR-0761` |
| `PACK-DIF-11` | `BPR-0762` |
| `PACK-DIF-12` | `BPR-0763` |
| `PACK-DIF-13` | `BPR-0764` |
| `PACK-DIF-14` | `BPR-0765` |
| `PACK-DIF-15` | `BPR-0766` |
| `PACK-DIF-16` | `BPR-0767` |
| `PACK-DIF-17` | `BPR-0768` |
| `PACK-DIF-18` | `BPR-0769` |

## Plan representation adjustment

The installed `milestone_plan.py discover --source-root ...` rejects the original pack plan's eighteen `PACK-DIF-*` work references as `invalid ref`: its schema requires numeric issue IDs before `adopt --id-map` can run. The immutable source plan remains unchanged. [import-proposal.json](../plans/bpr-dif-integration-v1/import-proposal.json) replaces only those eighteen typed `issue` values with the allocated IDs; its SHA-256 is `03a33c2cd54edbf227785bef3db694fe658d8ab82d8a5967ce8403f9f0c72894`. `adopt` used this validated derived source with an empty helper-level map because the mapping was already applied; its [source receipt](../plans/bpr-dif-integration-v1/source.json) therefore records `mapping: {}`. That empty field does not imply no renumbering: the complete original-to-live mapping is above and in `id-map.json`. The derived plan preserves the authored six milestones and every outcome, work order, dependency, exit, and boundary. No additional execution plan was invented.

## Validation and exclusions

The original and retained source's `python3 scripts/verify_pack.py` passed: two RFCs, eighteen cards, six milestones, ten exact inputs, 71 target frames and 22.75 MiB complete decoded pixels; `sha256sum -c SHA256SUMS` passed for every archived file. The installed plan helper checked the derived plan as structurally valid, adopted it, and bound `intake-contract` with all three card obligations and all three exits resolved; semantic acceptance remains unknown. Target-only discovery resolved the live adopted plan. The repository documentation gate reports `DOCUMENTATION_OK` and its 64 behavior tests pass. No production derivatives, resource selection, app implementation, runtime/native verification, art decision, human decision, or formal RFC closeout occurred. All future obligations in the six-milestone plan remain open. The source archive has no discarded authored files; only transient caches/editor artifacts, which were absent, are excluded. Its preview image is retained solely as authored provenance, not runtime proof.
