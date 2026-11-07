# Windows build investigation, 2026-11-06

[[BPR-1901]] investigates candidate retry cost in an isolated native checkout.
The paused release producer and its artifacts are unchanged. These are focused
development checks, not a new release qualification.

## Fingerprint compilation: BPR-1901

Source `04c3f2212` is the before baseline; `9d4654594` is the corrected candidate.
The two volatile definitions previously applied to every source in the app and
logging targets. Each fingerprint update recompiled `main.cpp`, installed
resources/runtime, the allocator probe, the generated icon, `quill_log.cpp` and
session storage. The candidate puts each value in a small private translation
unit with a source-specific definition. Immutable `constinit` string views
preserve the value and lifetime without dynamic initialization or extra allocation.
The existing digest calculations and release-cohort override are unchanged;
both new app identity files join the explicit attended source manifest.

Measurements use the same isolated Windows checkout, build tree, compiler,
Debug/Release options and disabled compiler cache as the investigation below.
A controlled comment in `src/app/src/designated_live_macos_supervisor.cpp`
changes both fingerprint inputs without changing Windows executable logic.
The checkout remains dirty throughout each timed series. Each sample explicitly
reconfigures first, then runs the generated app and logging projects sequentially
with MSBuild `/t:PrepareForBuild;CustomBuild;ClCompile`,
`/p:BuildProjectReferences=false`, `/p:Configuration=Debug` or `Release`,
`/p:Platform=x64`, `/m:4` and `/nr:false`. Object timestamps and actual compiler
command logs identify completed compilation, rather than counting the repeated
module-scan filenames as extra compilations.

| Configuration | Before, seconds (two retries) | After, seconds (two retries) | Objects per retry |
| --- | --- | --- | --- |
| Debug | 31.214, 31.078 | 4.936, 10.025 | 7 → 2 |
| Release | 35.433, 35.255 | 4.927, 4.902 | 7 → 2 |

The Release compilation phase saves about 30 seconds (86%); Debug saves about
21–26 seconds in these samples. Configuration is measured separately: before
38.40/42.43 seconds, after 33.22/45.21 seconds, with no claimed improvement.
Linking and the full release cycle are outside these timings. The retained
candidate-21 investigation still identifies linking as the largest cost.
The first build after this change recompiles the affected targets once because
their compiler definitions change; the table measures subsequent retries.

Native validation at `9d4654594fd8ce796b582eb02c492ccfa8b74358` passes:

- Each configuration preserves its complete 906-row CTest registration, not
  just the names. The seven existing compiler command lines are identical
  before/after after removing the relocated definition; diagnostics, runtime,
  optimization and hardening flags are unchanged.
- Changing `installed_resources.cpp` recompiles that Windows object and the
  diagnostic fingerprint in both configurations. Restoring the source and
  measurement anchor rebuilds their identities; the checkout finishes clean.
- Probes linked to the actual app and logger metadata objects in Debug and
  Release report exactly the values in their generated compiler definitions.
  An independent SHA-256 computation over the declared attended source manifest
  matches the app identity and includes both new identity files.
- Native Debug and Release logging builds pass, followed by six courses in each:
  `quill_logging`, `quill_logging_content_policy`, `logging_session_retention`,
  `logging_crash_recovery`, `platform_configuration_contract` and
  `cfg_complete_link_contract`. The logger checks record/export identity
  consistency through the real sink.
- The macOS Release application links and passes its offscreen
  `application_public_path` course plus the four logging courses. Architecture
  boundaries and their contract pass, clang-format 22 checks the touched C++,
  and clang-tidy 22 accepts both new translation units with warnings as errors.
  Documentation checks pass. These focused checks do not claim a complete
  release qualification or a full native Windows application link.

Raw receipts, exact command arrays, object changes and copied compiler commands
are in `compile-metadata-before` and `compile-metadata-after` under the external
evidence roots below; `metadata-validation` holds native value checks and test
logs. The controlled source edits are restored after the run.

## Declared dependencies only: BPR-1903

Source `39ee4ea4a45387515d72103799228839644cf628`, Windows 11 x64,
Visual Studio 18 2026, MSVC 14.51.36231, CMake 4.4.2 and pinned Qt 6.12.0:
all 939 CMake-generated Visual Studio projects, including nested syntax/ECM
projects, contain unconditional `VcpkgEnabled=false` before their imports.
All 408 current compiler, linker and librarian command logs outside cached
CMake probes exclude vcpkg. The baseline had ambient vcpkg paths in all 214
observed compiler/linker logs. Upstream source examples and cached compiler-ID
probes are not current generated build targets and are excluded explicitly.

The native configure deliberately supplied
`BprVcpkgVerification=changed;VcpkgEnabled=true;SecondSetting=two`.
Unrelated globals survive, while the opt-out overrides the attempted enablement.
The generated-project gate passes and fails when one real Abseil project's
property is temporarily changed back to `true`; its original bytes are restored.
Portable CMake controls cover defaults, mixed-case duplicates, repeated include,
unrelated globals, a non-Visual-Studio generator and semicolon forwarding.

Native Debug and Release builds and four registered courses pass in the normal
Limited Interactive Windows task context: `platform_configuration_contract`,
`cfg_complete_link_contract`, `application_github_window_read_host` and
`syntax_classification_contract`. Network isolation remains enabled. An initial
service-session syntax run failed before main with `0xc0000142`; the same binary
passes in the supported context. That failure remains in the raw evidence.
The existing assertion probe separately accepts its passing case and refuses
both intentional failures in Debug and Release. Debug checked iterators/CRT,
Release options and CFG link requirements remain intact. The core-only native
before/after inventories contain the same 355 test names in each configuration.

This is dependency isolation, not a measured warm-build speedup. Two unchanged
selected-target builds before/after took Debug 30.15/31.08 versus 30.76/30.62
seconds, and Release 30.37/30.29 versus 30.51/31.17 seconds. Removing the ambient
include paths correctly causes a one-time rebuild.

## Nested regeneration race: BPR-1904

The fresh syntax build at `bb70bb05e` failed after 76.969 seconds: parallel
Visual Studio projects tried to restore `data/CMakeFiles/generate.stamp`
simultaneously and ran competing CMake configurations. The files were writable.
The parent discards an incomplete dependency tree on retry, repeating work.
The correction suppresses only automatic regeneration inside the parent-owned,
pinned Visual Studio syntax build; its existing source/option signature still
owns invalidation, including this new argument. Root `ZERO_CHECK` stays present.

At `39ee4ea4a`, a fresh parent configure completes both Debug and Release nested
builds in 258.686 seconds. An identical configure takes 56.395 seconds and changes
no compiled dependency objects or verified stamp. Changing a forwarded global
invalidates the stamp, rebuilds 83 objects across both configurations and finishes
in 255.418 seconds. Repeating that input changes no objects. The four native
functional courses above pass in both configurations.

Do not interpret this as a faster ordinary warm dependency build: paired direct
retries with regeneration enabled/disabled measured Debug 25.01/24.71 versus
24.64/25.16 seconds and Release 11.27/11.31 versus 11.28/11.58 seconds. The benefit
is avoiding the observed failure and discarded-build retry.

## Evidence and scope

Raw command arrays, elapsed times, object inventories, generated-property audit,
negative control and test logs remain under `D:/BPR-1901-gl2kqegv/` in `vcpkg`,
`vcpkg-qt`, `regeneration-compare`, `vcpkg-final` and `vcpkg-final-replay`.
The Mac evidence copy is
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-windows-rebuilds-gl2kqegv`.
No machine-wide vcpkg setting was changed. Later integration preserves these
tested CMake/test bytes apart from the BPR-1904 explanatory issue reference;
concurrent tour work is outside this native evidence claim. Local documentation
and focused Python policy checks pass; no whole-product qualification is claimed.
