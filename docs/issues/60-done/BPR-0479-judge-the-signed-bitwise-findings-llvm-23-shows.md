---
schema: "implement-rfc.card.v1"
id: "BPR-0479"
title: "Judge the signed-bitwise findings the LLVM 23 tidy gate shows"
status: "Done"
rfcs: ["RFC-BPR-0013"]
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

## Slice one landed, and the headline above was wrong — 2026-08-28, night

The worker's audit corrected this card's opening count: the gate's 96
errors are **52 signed-bitwise across 23 files** plus **44 in four other
families** the same LLVM tightening surfaced —
`bugprone-std-exception-baseclass` 23, `performance-faster-string-find` 9,
`readability-identifier-naming` 7, `performance-use-std-move` 4,
`performance-string-view-conversions` 1. The conflation is corrected here
rather than rewritten away.

All 52 signed-bitwise sites are repaired in nine documented per-family
patterns (one audited-script pass, each line's pre-image asserted), with
one justified deviation: mode masks take a single `static_cast<mode_t>`
around the macro pair, because on macOS `mode_t` is 16-bit and per-macro
casts integer-promote straight back to `int` — probe-verified, as was the
fact that the gate already drops macro-located inner-`|` diagnostics as
non-user code. Verification: all 22 flagged TUs re-tidied clean of the
family with zero new diagnostics against a full parsed-set diff; build
exit 0 under `-Werror -Wconversion -Wsign-conversion`; format gate green;
34 focused selectors green across the prescribed, edited-test, and
edited-source coverage sets. Two pre-existing findings in other families
remain byte-identical at their recorded sites, counted in the 44.

Closure evidence: the whole-tree strict `coding-tidy` gate then ran end to
end and **passed with zero errors** (1292 s) — its first clean whole-tree
run under LLVM 23.1.0 — and the full dev matrix ran 218 tests with the
only two failures being the [[BPR-0477]] app-source digest pins, drifted a
second time by this card's own `src/app` casts; both were repinned to the
committed tree's values and rerun green (101.7 s + 28.1 s). Every product
test in the final run passed.

The 44 remaining findings are this card's slice two, and it landed the
same night: the four local `test_failure` classes derive from
`std::runtime_error` with their redundant `message()` accessors removed
and catch sites routed through `what()` (printed text byte-identical;
`noexcept` necessarily dropped from the forwarding constructors), the nine
string-find literals become chars, the seven structured-binding locals take
their fix-it camelCase names, the four moves and one string_view
conversion follow their diagnostics with each move's dead-afterward
property read directly. All 16 touched TUs re-tidy to zero user-code
diagnostics of any kind — stronger than family-clean — build and format
gates green, and the owning selectors pass 25/25 serially. Slice one's two
carried-over findings closed with it. One portability note
kept: `designated_live_macos_supervisor.cpp:342,490` carry the same
mode-mask shape but sit outside the dev compile database, so no gate has
judged them; a platform lane may surface more of the family.
