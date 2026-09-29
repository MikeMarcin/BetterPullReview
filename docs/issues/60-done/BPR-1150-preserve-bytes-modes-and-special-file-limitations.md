---
schema: "implement-rfc.card.v1"
id: "BPR-1150"
title: "Preserve bytes, modes and special-file limitations"
status: "Done"
rfcs: ["RFC-BPR-0054"]
depends_on: ["BPR-1148"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-06", "RC54-07"], "pack_alias": "PACK-CYCLE-08", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC54-06","outcome":"pass","revision":"7419de7acf981e59791b99d11b90ec48bb7a3af7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_material classifies a byte-golden corpus (LF, CRLF, CR, mixed, BOM, no final newline, multibyte, invalid, UTF-16) identically under any chunking and maps byte offsets to columns only at scalar boundaries; review_cycle_capture (at the BPR-1149 revision 73d474240f9519adc59236a670064f7e469d4d03) reads every retained object back byte-for-byte equal to the original file with raw SHA-256 equal to the file bytes, keeps CRLF/BOM/missing final newline, maps the execute bit to 100755/100644 and treats a mode change alone as new material; paths are admitted as raw bytes without normalization (BPR-1148)."}},{"criterion":"RC54-07","outcome":"pass","revision":"7419de7acf981e59791b99d11b90ec48bb7a3af7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_capture on macOS arm64: binary, UTF-16 and invalid UTF-8 are visible file-level only; a link is kept as target bytes and not followed; a FIFO is visible as unsupported; a file over budget is visible as too large; in a Git root with index v4 a submodule gitlink is a boundary with its staged commit and not recursed; filter=lfs marks both a pointer blob (recognized) and materialized bytes (kept, never cleaned) as filter-managed. Platform-collision rows are BPR-1148's core rule. Windows link targets report unsupported (owed native run)."}},{"criterion":"BPR-1150:boundary","outcome":"pass","revision":"7419de7acf981e59791b99d11b90ec48bb7a3af7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_material passes on macOS arm64 dev and core-only; clang-tidy and clang-format clean; architecture_boundaries passes after keeping the LFS header check free of a URL literal."}}]
---

Pack alias: **PACK-CYCLE-08** (live `BPR-1150`, allocated at import).

# BPR-1150: Preserve bytes, modes and special-file limitations

## Outcome and bounded work

Implement raw byte versus Git-object identities, UTF-8 byte/display mapping, CRLF/BOM/no-final-newline and mode handling. Distinguish pointer/materialized LFS content, unsupported encoding, binary, symlink/gitlink and sparse unavailable material without executing transformations.

## Acceptance and direct evidence

Byte-golden corpus, Unicode multibyte boundaries, invalid encoding, executable changes, binary/LFS and unsupported backend cases. Compare retained hashes with original bytes and ensure no lossy normalized replacement.

Criteria: RC54-06, RC54-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror`, `build/dev` freshly configured at `467254573`, and the fix `5303ab394`. The source volume is ext4 and `/tmp` is tmpfs.

`core_review_cycle_material` and `review_cycle_capture` pass (after the GCC fix recorded on [[BPR-1149]]). Native row: the byte corpus, modes, links, FIFO, gitlink and LFS were preserved without transformation, with index version 4.

The logs are station records in `build/logs/bpr-station-snapshots/` and are not committed.
