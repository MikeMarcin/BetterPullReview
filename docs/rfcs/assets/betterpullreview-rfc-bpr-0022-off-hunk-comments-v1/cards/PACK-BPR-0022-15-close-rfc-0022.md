---
provisional_id: PACK-BPR-0022-15
live_id: allocate-on-import
title: Verify and close RFC-BPR-0022
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-13, PACK-BPR-0022-14]
---

# Verify and close RFC-BPR-0022

RFC base: `SET-ON-IMPORT`

RFC-BPR-0022 cannot close before RFC-BPR-0008 is Auto-Verified.

## Closeout checklist

Copy the fifteen canonical acceptance criteria from the RFC verbatim into the live closeout table. For each row record the final-head direct verification surface, exact command or attended interaction, expected and observed result, durable evidence location, gate class, and producing commit.

## Acceptance

- Every imported implementation card is Done.
- RFC-BPR-0008 is Auto-Verified.
- All fifteen criteria have direct final-head evidence, including designated native and fallback GitHub-only interactions.
- No in-scope critical defect, unsupported inference, or duplicate-publication course remains.
- RFC/index/verification/board/source truth agree.
- `python3 scripts/verify_documentation.py --root .` and `git diff --check` pass.
- Only then is RFC-BPR-0022 marked Auto-Verified.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
