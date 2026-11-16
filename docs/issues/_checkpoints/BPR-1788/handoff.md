# Release preparation and installer source handoff

Mandate `release-prep-20261115-r2`; implementer `/root/release_prep`.
Plan `release-preparation-20261115`, milestone `installer-choice-source`.
Source baseline `0f0f803d4b374d2b63a9de35b6995fb76dcd37c1`.
Source candidate `b1c20df1cf97d517e99e2bfc5352d558d4bc475a` on
`codex/release-platform-prep-20261115`; tracker sync `9d9548b02` takes only
main's exact counter/reservation from `caa5a1ff24b8fdf811863afe4700c18d490a2130`.
No broad feature intake, card move, allocation, signing or publication.

## Result and verification

The source milestone is complete. Just me is the fresh default; Everyone
remains visible to a standard user. Choice precedes costing. Existing context
and directory restore before related-product search; first-sequence actions
avoid restoring them twice. Install root and Start menu marker use HKMU.
Current MSI producer metadata requires ALLUSERS=2/MSIINSTALLPERUSER=1;
previous actual MSI may be legacy per-machine or dual-purpose, with the
original digest/version/ProductCode/UpgradeCode floor checks retained.

Exact-source checks on the candidate:

- `python3 tests/scripts/package_runtime_tests.py`: 34 tests, OK, one
  explicitly skipped Linux producer row. Includes real CPack template
  generation and a changed-template refusal negative control.
- `python3 tests/scripts/native_release_tests.py NativeReleaseTests`:
  23 tests, OK. These run the maintained package driver with native-tool
  fixtures; they do not compile or install an MSI.
- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.
- Documentation verifier tests: 75 tests OK; Python compilation and
  `git diff --check` pass. BetterLeaks staged scan reports zero findings.

Two earlier broad release-script attempts also collected imported unrelated
ReleaseBuildTests. Each failed the cold/warm production observation because
this new checkout lacks offline `build/.cpm-cache/cpm/CPM-0.43.1.cmake`; that
test is not repaired or claimed passed. An initial new-test error-message
expectation was corrected; the focused final class passes. The broad attempts
ran 69/71 tests in 66.271/59.775 seconds, each with two failures and one native
Windows-only skip. Neither attempt supplies release qualification.

## Remaining acceptance and continuation

BPR-1788 remains In Progress: native CMake/WiX5.0.2 compilation, actual MSI
tables and ICE105, both ordinary-user installs, Everyone UAC with different
admin credentials, old per-user/per-machine same-scope upgrades, custom-path
repair/removal and no profile-data deletion remain open. In particular,
verify the installing user's HKCU identity across elevation. Changing scope
requires uninstall/reinstall; another user's per-user copy is not discovered.
No native review candidate exists, so no human review session is started.

The next independently eligible milestone is **Intel Ubuntu provisioning and
operational adoption of the post-0.4.2 workflow**, as the owner explicitly
requested. The [inventory](../../../development/release-preparation-20261115.md)
records the eleven cards, excluded Claude surfaces, process-improvement cards,
Intel prerequisites and native scheduling. Use existing pinned SSH alias
`intel-mbp`; no Ubuntu VM has been installed by this milestone. The coordinator
owns that read-only discovery. Windows/WSL share JaegAW; independent Intel
Ubuntu can qualify in parallel after its own compilation prerequisites.

## Ownership and helper state

All task commands have reached terminal status. No review/build server,
native task, VM or native host lease was started here. Shared jobserver and
Claude commands are left alone. Other worktrees and frozen candidates remain
protected; main integration waits the coordinator's writer reconciliation.
No owner desktop was driven or captured. `implement-rfc`, `publish-release`
and the frontend-design desktop skill were loaded.

Durable external state:
`/Users/jaeger/.codex/release-preparation/v0.5.0-20261115/installer-scope`.
It contains strategy selection, authored plan/provenance input, explicit
binding, exact-candidate command receipts and final checkpoint. The standard
`milestone_checkpoint.py bind` was attempted but refused because its target
discovery scans an unrelated historical plan containing an unsupported
`amendments` field. Supported `milestone_plan.py bind --plan ...` resolves this
plan and its AC1 obligation with no diagnostics; the wrapper refusal is not
reported as a passed check. No unrelated plan or skill was changed to bypass it.

## Native authoring continuation, 2026-11-15

Bounded follow-up `/root/windows_installer`, mandate
`release-prep-20261115-r2`, source correction `bf995128013b34dfa456468e2d2a832e00c3feb4`.
[Native authoring record](../../../development/windows-installer-native-authoring-20261115.md)
records CMake/CPack4.4.2, WiX5.0.2, actual MSI table evidence, explicit ICE105
and the original-source WIX0091 negative control. The production settings
compile with an inert payload after removing duplicate properties; AC2 remains
pending for the complete application MSI and installed-product course.

No product graph, install, owner desktop, signing, publication, cohort or station
takeover ran. Existing native station/WSL processes and Claude's feature work
are preserved. The composition tripwire remains a separate feature/common-
candidate prerequisite. External continuation:
`/Users/jaeger/.codex/release-preparation/v0.5.0-20261115/windows-installer-native`.
No human review candidate is prepared from an inert executable.
