# Windows build investigation, 2026-11-06

[[BPR-1901]] investigates candidate retry cost in an isolated native checkout.
The paused release producer and its artifacts are unchanged. These are focused
development checks, not a new release qualification.

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
