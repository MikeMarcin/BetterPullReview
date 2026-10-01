---
schema: "implement-rfc.card.v1"
id: "BPR-0970"
title: "Cover custom parser and mapping boundaries with fuzzing"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0970-parser-fuzzing","path":"docs/rfcs/plans/bpr0970-parser-fuzzing/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b54a8719e","evidence":{"kind":"artifact","detail":"docs/verification/BPR-0970-parser-fuzzing.md inventories every custom entry point in diff parsing/mapping, Markdown, settings, themes, API normalization, Git output and AI structured output, with input and trust, the dev-graph rows that reach it, and the target and selector that fuzz it; bound to aac225a4e plus this change and to the fuzz_<family>_corpus registrations. The designated live capsule's parser is named as not fuzzed."}},{"criterion":"AC2","outcome":"pass","revision":"b54a8719e","evidence":{"kind":"command","detail":"BPR_BUILD_FUZZERS=ON (Apple clang 21, libFuzzer runtime from Homebrew LLVM 23) builds eight targets with every first-party unit under fuzzer-no-link, ASan and UBSan; ctest -L bpr_fuzz_corpus replays 224 seeds, 8/8 pass; test_network_isolation_manifest passes in that graph; verify_live_mutation_isolation finds no live mutation provider in the fuzz binaries. Process-only readers moved unchanged into git_process_output.hpp and gh_cli_response.hpp; no test backdoor."}},{"criterion":"AC3","outcome":"pass","revision":"b54a8719e","evidence":{"kind":"command","detail":"Three 15-minute campaigns of all eight targets and a fourth of fuzz_github_api, seeds 970001 onward, -max_len per target, -timeout=10, -rss_limit_mb=2048, -malloc_limit_mb=512; executions, coverage and outcomes recorded. Four defects found and fixed with regression seeds (MCP identity cast UB, gh auth status over-read, markdown view freed-fragment read, exponential wildcard_match with discriminating core_review_cycle_manifest cases); each target's last campaign clean. macOS dev: 52/52 integration rows pass. No claim of absence of other defects."}}]
---

# Cover custom parser and mapping boundaries with fuzzing

## Transferred obligation

Original RFC0013 fuzzing goal and acceptance row 7, transferred in full.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Deterministic parser, malformed-input and property tests are valuable existing evidence, but the RFC0013 closeout inventory found no complete registered fuzz targets/corpora. They are not fuzz acceptance. This card preserves the entire unfulfilled requirement across diff parsing/mapping, Markdown, settings/themes, API normalization, Git output and AI structured output.

## Acceptance

- AC1: Inventory every custom entry point in the seven named families, existing deterministic tests and uncovered trust boundaries; bind the inventory to a source revision and actual build registrations.
- AC2: Add bounded runnable fuzz targets and representative/hostile corpora for uncovered custom parsers and mappings, preserving production validation and authority boundaries without test backdoors.
- AC3: Run and record reproducible sanitizer-backed campaigns with input/work/memory limits, seeds, duration and findings. Fix concrete defects with regression cases; no finite campaign claims absence of all bugs.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## Taken over and done — bpr-main-mac, 2026-10-01

The `codex/bpr0970-fuzz` worktree held this card's plan and claim (`ae639d59d`,
`ae8139d1a`, never merged) and six uncommitted draft harnesses with their
seeds. The plan was adopted on `main`; the harnesses were rewritten against
current `main`; the worktree was read and left as it was. The plan's "no
primary-checkout writes" bound that Codex session, not this one.

[The verification page](../../verification/BPR-0970-parser-fuzzing.md) holds
the evidence. In short:

- **AC1.** It inventories every custom entry point in the seven families
  (diff parsing and mapping, Markdown, settings, themes, API normalization,
  Git output, AI structured output) with its input, the deterministic rows
  that already reach it, and the target and selector that fuzz it. It names
  the one parser left out and why (the designated live capsule's).
- **AC2.** `BPR_BUILD_FUZZERS=ON` builds eight libFuzzer targets in
  `tests/fuzz`, with every first-party unit instrumented for coverage, ASan
  and UBSan, and registers eight `fuzz_<family>_corpus` rows that replay 224
  checked-in seeds. The targets call production entry points through the
  seams callers use. Two readers reachable only through a running process
  moved, unchanged, into headers their adapters include:
  `src/adapters/git/src/git_process_output.hpp` and
  `src/adapters/github/src/gh_cli_response.hpp`. No test backdoor was added.
- **AC3.** Three campaigns of 15 minutes per target, all eight targets in
  parallel, and a fourth for `fuzz_github_api` after its harness was
  corrected, with recorded seeds, limits (`-max_len` per target, `-timeout=10`,
  `-rss_limit_mb=2048`, `-malloc_limit_mb=512`), executions, coverage and
  outcomes. They found four defects, each fixed with a regression case:
  - `mcp_context_helper`: an out-of-range JSON-RPC number identity was cast
    to `int64_t` (undefined behaviour);
  - `parse_gh_cli_auth_status`: read one byte past a non-terminated view;
  - `markdown_body_view::refresh_formats`: restyled fragments under the
    iterator walking them, reading freed fragment nodes (`[_|_)]()`);
  - `wildcard_match`: exponential backtracking on a hostile `.gitignore`
    line (13 s for one match), now stopped as Git's `wildmatch` does, with
    three discriminating cases in `core_review_cycle_manifest`.

  No finite campaign shows the absence of other defects.

Integration on macOS: `build/dev` (Apple clang 21, Debug) builds with no
warning, and 52 of 52 rows covering the changed code pass (the Git, epoch,
GitHub, `gh`, MCP, Markdown view, manifest and capture rows, plus
`architecture_boundaries`, `coding_format_check`, `live_mutation_isolation`
and `test_network_isolation_manifest`). In `build/fuzz`, the manifest passes
with the eight new rows, and `scripts/verify_live_mutation_isolation.py`
finds no live mutation provider in the eight fuzz binaries.

The product changes compile on Windows and Linux only once a station builds
them; the card has no platform requirement, so those builds are requested of
the stations rather than owed here.

## Linux: bpr-main-linux, 2026-10-01

At `3dad44a25` (main, after `8fe4eafad`): WSL2, 20 cores, `build/dev` Debug.

- **GCC 15.2.** `ctest --preset dev -j1 --output-on-failure -R
  '^(git_|epoch_|github_|gh_cli_|application_mcp_context_helper|qt_markdown_body_|core_review_cycle_manifest|review_cycle_capture|workspace_source|architecture_boundaries|coding_format_check)'`:
  **40 of 40 pass**, 101.86 s, load average 0.5 to 0.8.
- **Clang (optional), did not build.** I configured `cmake --preset dev -B
  build/fuzz -DBPR_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++` with Ubuntu clang 21.1.8. `cmake --build
  build/fuzz --target parser-fuzzers` stops at 200 of 657 steps. There are 64
  errors `missing field '…' initializer
  [-Werror,-Wmissing-designated-field-initializers]` in eight core sources:
  `appearance_contrast`, `diff_compute`, `diff_semantics`,
  `epoch_transition`, `github_window_read`, `review_composition`,
  `review_cycle` and `review_cycle_comparison`. These are ninja's first
  failures, not an inventory.

  `cmake/Warnings.cmake` turns the GCC counterpart off
  (`-Wno-missing-field-initializers` for GNU) but not the Clang one, and
  `_coding-standard/scripts/tidy.py` passes
  `-Wno-missing-designated-field-initializers` to clang-tidy. So the Linux
  Clang path that `docs/development/build-and-test.md` documents ("Linux Clang
  needs no runtime argument") does not build at this revision. The macOS
  Apple-clang build that this card's evidence used is unaffected. The
  `bpr_fuzz_corpus` rows were not run on Linux. The warning policy was not
  changed, and that is left for a follow-up decision.
