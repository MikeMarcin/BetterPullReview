---
id: BPR-0036
title: Run RFC-BPR-0002 Windows x64 target-native verification
status: In Progress
rfc: RFC-BPR-0002
depends_on: [BPR-0034, BPR-0347, BPR-0350]
---

# Run RFC-BPR-0002 Windows x64 target-native verification

On a designated real Windows 10/11 x64 machine, run the checked-in
`windows-dev` configure, build, and complete CTest matrix. Run the focused
Credential Manager setup/lifecycle/cleanup sequence and prove exact absence
of the integration credential and scratch directory afterward.

Once the dependent slices exist, also run the native HTTPS
TLS/proxy/redirect/certificate matrix, inspect runtime network and process
scope, exercise the public application fixture, and inspect the selected PE
imports and runtime dependencies. Retain only secret-free commands, fixed
output, and inspection evidence.

GitHub CLI process execution is not part of this card. Milestone one keeps
every CLI selection fixed at zero-launch `cli_unavailable` on all platforms.
If the upstream safety and identity contract ever becomes eligible,
RFC-BPR-0015 owns exact-environment, image, working-directory, Job Object,
descendant, PID, crash-artifact, stream-bound, redirect, configuration, and
account-binding evidence before any Windows CLI claim.

This card intentionally owns every deferred RFC-BPR-0002 Windows x64
target-native claim. Cross-compilation, mocks, another operating system, and
GitHub Actions are not acceptable substitutes. Its open state does not block
implementation, tests, documentation, security review, or simplification on
the other available targets or later RFC work; it blocks only the affected
Windows claims and final RFC-BPR-0002 Auto-Verified closeout.

## Procedure — host policy on a freshly linked image

Recorded from [[BPR-0424]]'s three samples on the designated host, which
runs Smart App Control in its evaluation state
(`VerifiedAndReputablePolicyState=1`). Before any result below is read as a
matrix failure:

- A launch refusal with `win32=4551` (`ERROR_VIRUS_INFECTED`, reported by
  the test as `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551`, by the
  uncontained control as `[WinError 4551]`, and by a nested build as "blocked
  by your organization's Device Guard policy", `MSB8066`) on an image linked
  in the current session is the host policy waiting for a reputation verdict.
  It is per image, not per source, and clears on its own between about
  fifteen and forty-five minutes after the link.
- Confirm it from the Code Integrity operational log: events 3033/3077 name
  the image. Re-run once no new 3077 appears for that image and a probe run
  is allowed.
- A relink of the contained launcher
  (`tests/network_isolation/windows_network_sandbox.cpp`) stalls the whole
  contained suite until the launcher clears; every test then reports the
  same `4551`, and none of those rows is evidence about the boundary.
- It also lands on CMake try-run probes (`cmTC_*.exe`) and build tools run
  inside nested configures; the launcher try-run and the syntax provider's
  nested build already retry for it, and a configure that fails on a
  `cmTC_*` refusal is simply re-run.
- Only a refusal that persists past the verdict is a failure, and then the
  decision is the owner's, written on [[BPR-0424]]: sign the executables,
  switch the policy off, or record the exclusion.

## Run on the designated host — 2026-08-23

The checked-in `windows-dev` configure, build, and complete CTest matrix
ran on the project's Windows 11 x64 host (10.0.26200, MSVC 14.51,
Qt 6.10.3, CMake 4.4.2, Git for Windows 2.55.0.windows.3), through the
AppContainer launcher for every test the boundary supports and uncontained
for the fifty-three registrations [[BPR-0362]] pins by measured cause.
The matrix result and every failure's attribution live in [[BPR-0412]]'s
closeout; the milestone-one deferrals this card names — native HTTPS
matrix, runtime scope inspection, PE imports — follow the cards that own
those slices, and GitHub CLI process execution remains out of scope
exactly as written.

What this card's own bullets asked:

- **Complete matrix**: 147 of 188 Passed outright in the final run (`windows-dev-matrix-8.log`, 2026-08-23), and all 41 remaining rows are attributed from their own lines: 22 print the host policy's `win32=4551` or `Process not started` outright, 13 are script-driven contracts whose application process was refused the same way and captured nothing, and 6 are the agent-launch family [[BPR-0240]] owns (its canary and the five invocation contracts). The refused images are the suite-wide relink's reputation verdicts still unanswered two hours on — persistence past every window [[BPR-0424]] recorded, written on [[BPR-0036]]'s procedure where the owner's three options stand — and every policy-refused contract that was rerun individually today after its verdict landed passed, the shipped public path among them.
- **Credential Manager sequence**: `windows_credential_manager_native_setup`,
  `windows_credential_manager_native`, and
  `windows_credential_manager_native_cleanup` pass in order, uncontained as
  registered (the lowbox refuses the vault, measured on [[BPR-0362]]), and
  cleanup proves exact absence afterward — the sequence's own final
  assertions.
- **Public application fixture**: `application_public_path` and
  `application_github_public_path_github_com` pass; `PUBLIC_PATH_OK`,
  `GITHUB_FIXTURE_OK:github.com`, `RENDER_OK`.
- **Host policy**: the "Procedure — host policy on a freshly linked image"
  section above, written from [[BPR-0424]]'s three measured samples, is
  part of running this matrix unattended.

Evidence retained: the matrix log, the per-card closeouts in `60-done/`,
and the secret-free command transcripts in each; nothing here required a
credential and none was written.

Implemented by the integrator.

## Procedure addendum from the independent review — 2026-08-23

`longPathAware` in the executables' manifests is necessary but not
sufficient: it takes effect only while the machine-wide `LongPathsEnabled`
policy is 1, which this host has and stock Windows does not. On a host
without it, the adapters' filesystem walks over the staged refs fail with
`ERROR_PATH_NOT_FOUND` and the epoch pipeline reports `failed` with
nothing named — BPR-0441's original symptom. A named refusal at
composition time when the policy is off would keep the claim honest; until
one exists, the policy value is part of this procedure's preconditions.

## The matrix on the reputation-settled tree — 2026-08-23, evening

The complete matrix reran on the unmodified tree built 2026-08-22 21:25, so
no image was fresh and yesterday's reputation verdicts stood. **175 of 188
passed in 1199.56 s** (`windows-dev-matrix-9.log`), converting 28 of the 35
policy-refused rows — among them every RFC-BPR-0002 selector: both GitHub
public paths, `application_public_path`, CLI-unavailable,
`application_render`, `runtime_process_surface`, and both epoch-persistence
contracts.

The 13 remaining rows are fully attributed:

- **6** are the agent-launch family [[BPR-0240]] owns, unchanged.
- **4** are still-active Smart-App-Control refusals —
  `application_agent_workspace`, `sqlite_queue_enrollment_storage`,
  `sqlite_epoch_catalog_behavior_contract` ([[BPR-0053]]'s row), and
  `application_public_path_wait` — confirmed live in the Code Integrity log
  at 21:11–21:16, roughly 24 hours after their images were linked and far
  past every window [[BPR-0424]] measured. Disposition below.
- **2** were this harness's environment, not the tree: the nested
  KSyntaxHighlighting configures inside `dependency_modernization_contract`
  and `application_composition_contract` die on `Could NOT find Perl` when
  `PATH` lacks Git's `usr/bin`; both pass in a focused rerun with it
  appended. Recorded in the procedure below.
- **1** is real and new: [[BPR-0447]], the epoch-maintenance pruned-restart
  failure, reproduced three times.

### Owner disposition of the persistent refusals — recorded verbatim

> What is this even tyring to test? Do I need it right now? Can we delay this
> signing until much later? I don't want to permanently have something
> unsigned or drop system wide ssecurity, but I also don't care to do it
> right now when the app isn't even useful yet.

Applied as the reversible middle option [[BPR-0424]] wrote down: no signing
setup now, no Smart App Control change. The four rows stay policy-refused
with the Code Integrity evidence, none of them is an RFC-BPR-0002 closeout
selector, and the deferral lifts whenever the owner signs the executables or
revisits the policy.

### Procedure addendum: PATH for nested configures

Every matrix or focused `ctest` invocation must append Git's `usr/bin`
(for `perl`) to `PATH` after importing the MSVC environment. The nested
verified-KSyntaxHighlighting configures require it; a run from Git Bash
carries it implicitly, which is why earlier matrix runs never exposed it.
