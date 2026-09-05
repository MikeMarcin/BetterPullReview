---
id: BPR-0602
title: Implement basic Quill logging for application and GitHub failures
status: Done
rfc: RFC-BPR-0027
depends_on: [BPR-0600]
---

# Implement basic Quill logging for application and GitHub failures

## Scope and ownership

Owner authorized the basic foundation now. Follow RFC-BPR-0027's phase-1
amendment. Composition owns lifetime and path selection, a dedicated logging
adapter owns Quill/file effects, an application port carries bounded events,
and the GitHub adapter produces sanitized capability-error detail. No logging
or clock dependency enters core or Qt presentation.

## Acceptance

- Pin and verify Quill with license/provenance; dev and tests-disabled Release
  compose it without upstream tests/examples or implicit package substitution.
- Write startup/shutdown and viewer/capability outcome records with useful
  local request correlation, status and sanitized capability error detail to a
  documented fixed file that survives quit and is directly readable.
- Unknown GraphQL error code/message prose survives as useful bounded detail,
  while reflected active credentials, token-shaped values, authentication
  bodies, headers, source/review bodies and arbitrary response fields do not
  enter the logging queue/file. The production failure policy is unchanged.
- Bound record/queue/file work, refuse unsafe file targets and make
  unavailable/full sink behavior explicit without blocking core decisions or
  preventing Sign out/shutdown. File rotation and retention remain deferred.
- Focused sanitizer/logging/provider tests plus one credential-free composed
  application lane prove the above. Run relevant architecture/composition
  checks for changed public seams and scoped format/tidy; no live sign-in.

## Non-goals

Rotation, anonymization, privacy/verbosity preferences, export, Settings UI,
recent-context capture, profile support, broad transport dumps, full RFC closeout
and live credential/publication operations. No new UI approval is needed merely
to acquire the dependency the owner explicitly requested.

## Verification record

Implementation commit: `3d109fd4`. Native evidence: macOS arm64. All final
commands passed; the existing dev, app-release and core-only roots were reused.

- `cmake --preset dev` and `cmake --build --preset dev`: pass.
- `cmake --preset app-release` and `cmake --build --preset app-release`: pass,
  tests disabled.
- `cmake --preset core-only`: pass; selected dependency audit has no Quill/Qt.
- `ctest --preset dev --output-on-failure -R '^(native_https_transport_contract|github_provider|github_capability_probe|application_review_round_trip_composition_contract|application_logging|quill_logging|quill_logging_backpressure)$'`:
  7/7 pass, 51.19 seconds. Integrator reran the six direct runtime selectors
  excluding the unchanged source-composition contract: 6/6 pass, 6.33 seconds.
- `ctest --preset dev --output-on-failure -R '^native_https_transport_fixture$'`:
  pass, 16.48 seconds. The existing metadata scenario reflects a synthetic
  credential using JSON escapes and retains exact five-header assertions.
- `ctest --preset dev --output-on-failure -R '^(architecture_boundaries|application_review_round_trip_composition_contract)$'`:
  2/2 pass, 54.36 seconds.
- `python3 tests/scripts/dependency_modernization_tests.py --build-dir build/dev --cmake /opt/homebrew/bin/cmake --cpm-source-cache build/.cpm-cache --sqlite-source build/dev/_bpr/sqlite/3.53.4 DependencyModernizationTests.test_quill_provider_is_pinned_and_download_only DependencyModernizationTests.test_configured_dependency_audit_uses_supported_providers`: 2/2 pass.
- `python3 tests/scripts/architecture_boundary_tests.py ArchitectureBoundaryTests.test_quill_headers_are_owned_by_logging_adapter ArchitectureBoundaryTests.test_designated_github_effect_sources_are_allowed`: 2/2 pass.
- Scoped strict clang-tidy covered ten changed translation units. Initial
  analyzer/cast findings were corrected without new suppressions; affected
  logging, composition and test units were rerun and passed. Scoped formatting,
  code-simplifier review and `git diff --check` passed. Commands:
  `clang-format --dry-run --Werror <changed C++ paths>` and
  `python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths <changed translation units>`.
- `python3 scripts/verify_documentation.py --root .`: pass.
  `python3 tests/scripts/verify_documentation_tests.py`: 62/62 pass.

The real composed fixture supplies synthetic device/provider behavior under
network isolation and exercises startup, provider injection and shutdown to the
file. Sanitizer/provider/file tests retain unfamiliar useful prose and request/
status correlation while rejecting credential canaries. The file fixture proves
reopen/append, crossing-cap refusal, unsafe targets and closed ingress. A child
with a full non-draining stderr pipe exits during both full and unavailable sink
failures. The callback throwing-copy case verifies the repaired failure path;
source exception-safety review establishes removal of the original consuming
allocation path, rather than claiming that fixture reproduced its allocator.

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).
One bounded confidentiality/lifetime review and repair delta resolved blocked
stderr cleanup, callback preservation and selectively percent-encoded credential
reflection. No remaining blocker. Opposite-family review was unavailable.

## Delivered behavior and limits

Quill v13.0.0 is pinned with hash/size verification and MIT/bundled-fmt notices.
Composition injects a dedicated adapter through an application port and closes
the owned backend after producers quiesce. One owner-only append file at
`<ApplicationDataRoot>/logs/diagnostics.log` has a 16 MiB ceiling. Summary capacity
is 2,048 bytes, producer queues drop at 64 KiB, admitted thread identities are
capped at 16, and backend transit buffers are bounded. Typed availability/full/
drop states have no new UI or stderr fallback.

Native file evidence covers macOS. Windows explicitly reports unavailable;
Linux runtime, Windows support, rotation/anonymization/controls/export and full
RFC acceptance remain deferred. No hard filesystem-latency or crash-durability
claim. No live sign-in, credentials or publication were used; BPR-0559's live
root cause remains unknown. The implementation does not grant new live authority.

Temporary `/tmp/bpr0602-*` inspection/build logs are removed after integration.
Test temporary directories clean themselves. Existing build roots and the pinned
Quill dependency cache remain for ordinary development; no worktree was created.
Documentation reconciliation is recorded in the completion commit.
