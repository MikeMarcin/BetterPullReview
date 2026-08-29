---
id: BPR-0479
title: Judge the signed-bitwise findings the LLVM 23 tidy gate shows
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# Judge the signed-bitwise findings the LLVM 23 tidy gate shows

Found by BPR-0101's closeout tidy gate — the first whole-tree macOS strict
`coding-tidy` under Homebrew LLVM 23.1.0. The gate fails with 96
`bugprone-signed-bitwise` errors across 37 files, every one the same shape:
POSIX and SQLite flag macros (`S_IRWXG | S_IRWXO`, `O_CREAT`-family,
`W_OK`-family) expand to signed integer literals, and the newer check flags
any binary bitwise operator with a signed operand. The 2026-08-17
whole-tree pass at `4fc9a78` was green under the previous LLVM; the format
gate at 23.1.0 has been green throughout, so this is the tidy check
tightening, not source drift — none of the flagged lines was written today,
and they span RFC-0002/0003/0007-era surfaces. Hygiene under RFC-BPR-0013,
per the [[BPR-0471]] precedent, blocking RFC-BPR-0005's flip only because
the strict gate is a closeout gate.

Disposition: fix, not suppress — per-site unsigned casts in the house's
existing targeted-`static_cast` idiom, no `NOLINT`, no `.clang-tidy`
change, bit values and call types identical.
