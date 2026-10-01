---
schema: "implement-rfc.card.v1"
id: "BPR-1260"
title: "Linux Clang cannot build the dev graph or the fuzz path: -Werror turns -Wmissing-designated-field-initializers into errors"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux", "mac"]
metadata: {"found_by":"bpr-main-linux, BPR-0970's optional Linux Clang run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1260--BPR-1260--bb361cf7eed8.json","related_cards":["BPR-0970","BPR-0816"]}
acceptance: [{"criterion":"Linux Clang builds parser-fuzzers, and the eight bpr_fuzz_corpus rows pass","outcome":"pass","revision":"92144e838","evidence":{"kind":"command","detail":"Ubuntu clang 21.1.8: build/fuzz configured with BPR_BUILD_FUZZERS=ON; cmake --build build/fuzz --target parser-fuzzers builds with 0 warnings; ctest --test-dir build/fuzz -L bpr_fuzz_corpus passes 8 of 8. Negative control: a planted first-party leak fails fuzz_markdown_view_corpus."}},{"criterion":"The GCC build/dev graph and its flags are unchanged","outcome":"pass","revision":"92144e838","evidence":{"kind":"command","detail":"GCC 15.2 dev reconfigure never runs the flag check, and build/dev/compile_commands.json has no new flag. The full build has 0 warnings or errors. The agent and MCP rows pass 42 of 43; the failure is the known BPR-1245 workshop row. architecture_boundaries, coding_format_check and both network-isolation manifest rows pass."}},{"criterion":"The Apple clang build is unaffected, or the Mac station runs it","outcome":"pass","revision":"92144e838","evidence":{"kind":"freeform","detail":"Not run on macOS. The flag is added only when check_cxx_compiler_flag finds the warning, so an Apple clang that doesn't know it gets nothing. The C++ edits are portable, and MSVC returns before these options. The Mac and Windows build checks are handed to the integrator, who offered to run them."}}]
---

# BPR-1260: Linux Clang cannot build the dev graph or the fuzz path: -Werror turns -Wmissing-designated-field-initializers into errors

Found by bpr-main-linux on 2026-10-01, at `a9a486698`, while running
[[BPR-0970]]'s optional fuzz corpus on Linux.

## Observation

The configure command was `cmake --preset dev -B build/fuzz
-DBPR_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`,
with Ubuntu clang 21.1.8. `cmake --build build/fuzz --target parser-fuzzers`
stops at 200 of 657 steps. It reports 64 errors of the form `missing field '…'
initializer [-Werror,-Wmissing-designated-field-initializers]`, in eight core
sources: `appearance_contrast`, `diff_compute`, `diff_semantics`,
`epoch_transition`, `github_window_read`, `review_composition`, `review_cycle`
and `review_cycle_comparison`. These are ninja's first failures, not an
inventory.

The policy treats the two compilers differently:

- `cmake/Warnings.cmake` turns off GCC's counterpart,
  `-Wno-missing-field-initializers` for GNU, but does nothing for Clang.
- `_coding-standard/scripts/tidy.py` already passes
  `-Wno-missing-designated-field-initializers` to clang-tidy.
- `_coding-standard/STANDARD.md` has no rule that designated initializers must
  name every field.

The code relies on default member initializers for the fields it leaves out.
So the documented Linux Clang fuzz path, "Linux Clang needs no runtime
argument" in `docs/development/build-and-test.md`, does not build.

## Scope

- Match GCC's policy for Clang in `cmake/Warnings.cmake`, consistent with
  `tidy.py`. Add the flag only where the compiler knows the warning, because
  an unknown `-Wno-` option is itself an error under `-Werror` on an older
  Apple clang.
- MSVC is unaffected: its branch returns before these options.

## Acceptance

- Linux Clang builds `parser-fuzzers`, and the eight `bpr_fuzz_corpus` rows
  pass.
- The GCC `build/dev` graph and its flags are unchanged.
- The Apple clang build is unaffected, or the Mac station runs it.

## Fix (bpr-main-linux, 2026-10-01, `92144e838`)

The documented sequence had three more gaps on Linux beyond the warning, so
the fix has four parts:

1. **Warning policy.** `cmake/Warnings.cmake` adds
   `-Wno-missing-designated-field-initializers` for Clang and Apple clang,
   matching GCC's `-Wno-missing-field-initializers` and `tidy.py`.
   `_coding-standard/STANDARD.md` has no rule that designated initializers
   name every field. The flag is added only when `check_cxx_compiler_flag`
   finds the compiler knows the warning, because an older Apple clang would
   reject an unknown `-Wno-` option under `-Werror`. MSVC returns before these
   options. GCC never runs the check: the dev configure log has no
   `BPR_CXX_HAS_MISSING_DESIGNATED_FIELD_INITIALIZERS` line, and
   `build/dev/compile_commands.json` has no new flag.
2. **Incomplete `std::pair` (found once the warning cleared).** With the
   warning off, the build stopped in `agent_launch.cpp` and
   `mcp_context_helper.cpp`. Each has a recursive JSON value that held its
   object members as `std::pair<std::string, json_value>`. The pair's
   constrained default constructor asks whether `json_value` is implicitly
   default constructible while `json_value` is still being defined. Clang with
   libstdc++ 15 refuses that as an incomplete type; GCC and libc++ accept it.
   Both now hold a named `json_member { Name; Value; }`, defined after the
   value. `std::vector<json_member>` of a then-incomplete type is fine, as the
   existing `std::vector<json_value>` already was. In the MCP helper,
   `json_node::member` moved out of the class so its body follows
   `json_member`. No behavior changes.
3. **The sandbox tool.** On Linux every corpus row runs under
   `better_pull_review_network_sandbox`, which `parser-fuzzers` did not
   build, so all eight rows were "Not Run". `parser-fuzzers` now depends on
   `tool_better_pull_review_network_sandbox` where that target exists.
4. **LeakSanitizer on `markdown_view`.** AddressSanitizer runs
   LeakSanitizer by default on Linux, though not on macOS. At exit it
   reported 152 records (447,055 bytes). Every record was allocated inside Qt
   Gui/Core, FreeType or HarfBuzz, which are font caches that live as long as
   the process; the target's only frame was the allocator interceptor. On
   Linux, `fuzz_markdown_view_corpus` gets
   `LSAN_OPTIONS=suppressions=tests/fuzz/markdown_view_lsan.supp`, which
   lists those four modules. Other rows and platforms are unchanged.

| Check (Linux, Ubuntu clang 21.1.8 unless noted) | Result |
|---|---|
| `cmake --preset dev -B build/fuzz -DBPR_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`, then `cmake --build build/fuzz --target parser-fuzzers` | Builds, 0 `warning:` lines. The configure finds the flag (`Success`), and 660 compile commands carry it. |
| `ctest --test-dir build/fuzz -L bpr_fuzz_corpus` | **8 of 8 pass** |
| Negative control: a leaked `new std::string` added to the `markdown_view` target (not committed) | The row fails: `Direct leak … SUMMARY: AddressSanitizer: 194 byte(s) leaked` |
| GCC 15.2 `build/dev`: reconfigure and full build | 0 `warning:` or `error:` lines; no new flag |
| `ctest --preset dev -R 'agent\|mcp' -j10` (GCC) | 42 of 43 pass. The one failure is `application_agent_finding_workshop_press_contract`, parked on [[BPR-1245]]. |
| `architecture_boundaries`, `coding_format_check`, `test_network_isolation_structure_contract`, `test_network_isolation_manifest` | Pass |
| `verify_conditional_branches.py`, `git diff --check`, `DOCUMENTATION_OK` | Pass |
| `tidy.py --paths` on the two sources | No finding on a changed line. The three existing `readability-identifier-naming` findings are at `agent_launch.cpp:1562`, `1577` and `1839`. |

**Not run here.** Apple clang on macOS, a full Apple-clang build, and MSVC.
The C++ changes are portable: a designated-initializer `push_back` and an
out-of-class member function. The flag is guarded by a compile check.
