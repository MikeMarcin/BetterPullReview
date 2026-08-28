---
id: BPR-0473
title: Remove the redundant empty initializer the tidy partition surfaced
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# Remove the redundant empty initializer the tidy partition surfaced

The [[BPR-0054]] closeout's partitioned `clang-tidy` audit failed one
translation unit: `run_isolation_policy_file_contract`'s non-Windows branch
declared `constexpr std::string_view expectedGlobalConfig = "";`, which
`readability-redundant-string-init` rejects under `--warnings-as-errors`. The
line arrived with `4761afe4` (2026-08-26, the four review-named target-native
adversaries) after the last full tidy audit ran, so no earlier gate saw it.

The fix is `constexpr std::string_view expectedGlobalConfig{ };`. Observable
behavior is byte-identical: the view reaches only
`write_text -> std::as_bytes(std::span{...})`, which writes zero bytes for an
empty view regardless of whether `data()` is null, and one content
comparison. The Windows branch keeps its real `longpaths` text and its
preprocessor branch is untouched, so the target-native Windows record is
unaffected. The macOS `git_process_contract` row was re-run against the
rebuilt binary rather than assumed, and the tidy chunk that failed re-ran
green; both results are in the RFC-BPR-0003 final-closeout-replay section.
