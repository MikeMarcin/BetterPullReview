# v0.3.0 release engineering

## Assignment and source

Mandate `release-v030-coordinated-r1` requests full coordinated stable 0.3.0
for macOS universal, Windows x64 and Ubuntu 26.04 x86_64, retaining agent
integrations and local automation. Entry source is clean main
`a426db419d393895020ab961509df46aba94ee46`; the change summary is grounded in
the committed delta from v0.2.2. [BPR-1623](../issues/30-in-progress/BPR-1623-prepare-coordinated-v030-release-source.md)
owns version, notes and exact-source readiness under the
[engineering plan](../rfcs/plans/release-v030-engineering/milestone-plan.json).

The sole engineering owner is `/root/release_engineering`. The coordinator
owns native producer dispatch, normal native gates, packaging, signing and
publication. No full build/test command or remote production is part of this
source-preparation card. Later gate repairs require an explicit bounded
coordinator handoff. The durable source/command checkpoint is
`.mask/sessions/release-v0.3.0/engineering-handoff.json`.

## Qualification and evidence limits

The [RFC-BPR-0038 ledger](RFC-BPR-0038.md) and blocked BPR-0958–0962 remain
authoritative. Historical M8/M9 and licensing approvals are candidate-specific;
they do not qualify 0.3.0 or another platform. Native clean-account/Intel,
Windows and Ubuntu desktop/authentication/vault, accessibility, crash/privacy,
independent final-artifact trust, licensing/source access and retention,
vulnerability and complete release-security obligations remain open. Historical
raw Grype failures are retained as failures, not current-candidate scans or
approved applicability dispositions. RFC0038 remains Proposed.

Every native gate must record actual failures and configured skips. The earlier
Debug cache-bytes skip and separate optimized Release pass keep their original
source and scope; neither establishes this candidate's result. One shared
source revision must bind final packages, SPDX, notices, separate versioned
corresponding-source assets and checksums. WSL production is distinct from clean
Ubuntu desktop/Secret Service acceptance. Profile purge excludes shared updater
and registry state and is not uninstall.

## Source preparation verification

The project version changes only from 0.2.2 to 0.3.0. The
[release notes](../releases/v0.3.0.md) summarize shipped changes, selected
platforms, stable release intent and the qualification limits above. Product
semantics, dependency pins, thresholds, test selectors and skip policy are
unchanged. No new human or independent acceptance criterion is introduced by
this bounded source-preparation slice. Final native results and package trust
remain coordinator obligations.

Source preparation passes `python3 scripts/verify_documentation.py --root .`
(`DOCUMENTATION_OK`), all 67 cases in
`python3 tests/scripts/verify_documentation_tests.py`, and `git diff --check`.
A direct version/notes check reads 0.3.0 from the CMake project and confirms
the selected platforms and qualification limitations. Self-review compared the
notes against the committed v0.2.2 delta and completed cards; the sole build
change is the project version. No native gate, new configured skip, package,
signature or publication result is claimed. Both documentation commands finished;
there are no task-owned background commands or review server.
