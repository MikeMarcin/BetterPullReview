# BPR-1617 verification

Mandate: hashing-replacement-r5, expanding the exact r4 SHA-256 instruction with
the owner's chrono/random amendment. Candidate: `45b0d3528fdf159ad9050debe3bd0f9f28d59bcf`.
All product source hashes are bound by
[the platform evidence manifest](../../../verification/_BPR-1617-platform-sha256.json).

- Core-only native build: public SHA-256/Git object tests, CNG executable
  contracts and body-digest port built with the production core.
- `ctest --test-dir build/core-only -R
  '^(core_sha256|core_sha256_cng_.*|core_git_objects|application_body_digest_port|core_dependency_boundary|core_boundary_contract|architecture_boundaries)$'
  --output-on-failure`: **11/11 pass**, 29.10 seconds on the exact candidate
  sources. CNG modes: success, initialization fallback, duplication refusal,
  update refusal and finalization refusal.
- `cmake --preset core-only -DBPR_CORE_SHA256_PORTABLE=ON`; build the SHA-256,
  Git object and body-digest targets; corresponding CTest selector:
  **3/3 pass**, 1.06 seconds. Native default restored with PORTABLE=OFF.
- `python3 tests/scripts/core_boundary_tests.py`: **8 tests pass**.
- Full `architecture_boundary_tests.py`: 157/158 initially passed; the sole
  failure was the newly added allowed-owner fixture lacking core's source
  ownership list. Corrected fixture, then
  `python3 tests/scripts/architecture_boundary_tests.py
  ArchitectureBoundaryTests.test_platform_sha256_has_exact_native_and_adapter_owners`:
  **pass**. Unchanged 157 passing checks reused.
- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.
  Its initial canonical-closeout text failure was fixed by reconciling AC06
  in BPR-1453 under the adopted amendment.
- `python3 tests/scripts/verify_documentation_tests.py`: **67 tests pass**.
- Selected nine C++ files: clang-format **22.1.8** dry-run passes.
  `git diff --check`: pass.
- macOS runtime: LLDB breakpoint/stack proves CC_SHA256_Update reached by
  core::sha256. That test child exited after an explicit owned-process kill.
- Real Windows: exact source hashes match the committed manifest. MSVC
  19.51.36260.0, SDK10.0.26100.0, Release, /W4 /WX /permissive-;
  both CNG and HACL SHA-256 courses pass, including moves. bcrypt.dll's five
  hash APIs appear in the native executable import table. This is an isolated
  course, not a full Windows repository/dev or package build.
- Release bulk probe: 64 MiB, median seven, identical SHA-256 in all four
  native/portable runs. macOS 3143.0 vs 553.7 MB/s (5.68x), Windows 552.3 vs 271.5
  MB/s (2.03x). Loaded-machine observation; no old idle/station rule is claimed.

No source logic was changed after these exact-source observations. The
implementation was self-reviewed for independent copy ownership, move release,
startup fallback and fail-closed native errors. No independent or human review
is required for this bounded non-UX milestone. Original RFC-wide/native station
and release obligations remain on BPR-1448, BPR-1449 and BPR-1453.

Private raw command output remains under /tmp/bpr-platform-crypto for coordinator
inspection. Remote temporary source/build root was removed after all commands
finished; no remote repository checkout was changed.
