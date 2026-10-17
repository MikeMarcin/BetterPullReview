---
schema: "implement-rfc.card.v1"
id: "BPR-1571"
title: "Explain GitHub CLI discovery failures without exposing child output"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved Intel auth diagnosis and local-main integration 2026-10-17","governing_rfc":"RFC-BPR-0015","related_rfcs":["RFC-BPR-0078"],"reservation":"docs/issues/_reservations/BPR-1571--BPR-1571--a439bf305082.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"fbec1f08c","evidence":{"kind":"command","detail":"Process runner preserves only counts and completed exit while failed child content stays unavailable; reviewed closed stage/category diagnostics omit paths, output, inventory, arguments and environment."}},{"criterion":"AC2","outcome":"pass","revision":"fbec1f08c","evidence":{"kind":"command","detail":"Offscreen --gh-discovery passes all fixed error messages and selectable plain text; composed retry remains enabled for ineligible inventory."}},{"criterion":"AC3","outcome":"pass","revision":"fbec1f08c","evidence":{"kind":"command","detail":"gh_cli_transport, github_cli_process_runner and application_device_flow_cli_choice pass, including error/default parsing and actual signed-out projection."}},{"criterion":"AC4","outcome":"pass","revision":"fbec1f08c","evidence":{"kind":"observation","detail":"Provider contract records bounded macOS 15.1.1 failure and post-update/reboot 15.8.1 metadata success, unchanged BPR/gh, user-observed BPR success and deferred GUI retest; documentation gate passes."}}]
---

# BPR-1571: Safe GitHub CLI discovery diagnostics

Account discovery previously collapsed tool failures into a generic process
message and confused a completed inventory with no eligible account with an
empty inventory. The owner approved landing the missing diagnostics, actionable
messages and regression coverage on local main. No release or credential-access
redesign is authorized by this slice.

## Acceptance

- AC1: Discovery logs only fixed stage/failure categories, elapsed time,
  observed exit and stream byte counts; no child output, inventory, paths,
  arguments, account names or credential environment values.
- AC2: Missing tool, launch failure, timeout, nonzero exit, rejected output,
  malformed inventory and nonempty-but-ineligible inventory remain distinct;
  safe status text is selectable and discovery can be retried.
- AC3: Fixtures verify error content stays unavailable while counts survive,
  exit-zero inventory does not imply account eligibility, and the actual
  discovery completion keeps an inaccessible account signed out.
- AC4: Compatibility documentation distinguishes the observed macOS 15.1.1
  failure and post-update/reboot 15.8.1 success, with unchanged BPR/gh versions,
  from a proven Apple cause, universal qualification or a minimum fixed version.

## Scope

The unchanged sandbox and strict chosen-account credential path remain in force.
No credential, Keychain ACL, native broker, installed package or Intel developer
update changes. No generated binaries, private evidence or raw logs are committed.

## Verification

Implementation `fbec1f08c` passed the M4-local x86_64 test build (macOS host,
Qt offscreen; no Intel build or live desktop interaction):

- `architecture_boundaries`, `gh_cli_transport`, `github_cli_process_runner`,
  `application_device_flow_cli_choice`: 4/4 pass, 58.24 seconds.
- `better_pull_review_qt_workbench_window_tests --gh-discovery`:
  `github-cli-discovery:pass`.
- Touched C++ files formatted with clang-format 22.1.8 through the project
  script; `git diff --check` passes; documentation gate: `DOCUMENTATION_OK`.

The architecture runner initially could not apply its sandbox inside the tool
sandbox; its unchanged test passed in the permitted host execution context.
Intel native observations are bounded context, not an automated qualification
course. No Windows/Linux runtime qualification is claimed. The requested
frontend-design skill was unavailable in the installed catalog/files; this
slice changes existing status copy and selectability, with offscreen Qt and
composed tests rather than a new visual design.
