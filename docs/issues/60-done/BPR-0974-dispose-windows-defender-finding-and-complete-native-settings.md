---
schema: "implement-rfc.card.v1"
id: "BPR-0974"
title: "Dispose Windows Defender finding and complete native Settings qualification"
status: "Done"
rfcs: []
depends_on: ["BPR-0730","BPR-1110"]
priority: "P1"
platforms: ["win"]
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Windows full native menu/shortcut/window-lifetime course","AC-11 current Windows inventory refresh"],"related_cards":["BPR-0960","BPR-0962","BPR-1110"],"owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0974-defender-disposition-v001","decision":"3482f766-1945-42d2-b175-b2e89434c4e6","outcome":"reject","note":"Explain why this is being triggered."},"owner_disposition":{"date":"2026-09-25","session":"7083474240820608cdf9ff551ddd298f","candidate":"BPR-0974-defender-disposition-v002","decision":"5c30287a-e458-4c2c-89fd-d952174016bb","selected":"Lower the static signals: add a VERSIONINFO resource, keep a PDB reference in Release (/PDBALTPATH:%_PDB%), and move the 3.6 MB of compressed resource blobs out of the executable","not_now":"Authenticode signing"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"85e522d86f29230454c72b4d965fa2e06986e394","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-0974-dispose-windows-defender-finding-and-complete-native-settings.md","detail":"Owner's amended disposition (2026-09-25, attended session 70834742): lower the static signals and go ahead. Candidate 85e522d8 (windows-app-release, MSVC 19.51, Qt 6.11.2) carries the three changes; exe 6aa27c8d..., rcc ca3bc2b1...; first launch 2026-09-25T02:21Z drew no Defender detection or operational event; eight launches recorded nothing. The 2026-09-22 Bearfoos.A!ml finding stays recorded and is not called a false positive. The second candidate 7c8402fc (exe 1947fa54..., rcc fd94a0e6...) also drew no detection at its first launch (2026-10-06T02:30Z, no event after record 7317)."}},{"criterion":"AC2","outcome":"pass","revision":"7c8402fca46b638fb589866ad5dc23ba73807a22","evidence":{"kind":"command","path":"docs/verification/BPR-0974-windows-native-settings/windows-course-7c8402fc.json","detail":"Cleared tests-disabled candidate 7c8402fc (carries BPR-1136) on the owner's Windows input desktop, one launch bound to exe 1947fa54.../rcc fd94a0e6... and PID 4140: native menu/Ctrl+comma/modeless/singleton/profile label, Escape and shortcut reopen, same-profile sibling via Profiles > New Window with its menu, shortcut and retirement, seven categories, File > Quit exit 0; private profile, no process left. Bound by settings_inventory_comparison_tests.py (23/23) with stopped-course and wrong-image negative controls. The 85e522d8 stop at the second Application menu was classified and fixed by BPR-1136."}},{"criterion":"AC3","outcome":"pass","revision":"85e522d86f29230454c72b4d965fa2e06986e394","evidence":{"kind":"command","path":"docs/verification/BPR-0974-windows-native-settings/windows-inventory-85e522d8.json","detail":"All seven pages captured natively and compared with the Linux b651554f capture page by page; differences only the named WINDOWS_DELTAS; updater, CLI-capability and seven maintenance rows inventoried; missing-control and changed-state negative controls in settings_inventory_comparison_tests.py. Note: on the 7c8402fc course capture five pages match 85e522d8 control for control; Agents & context and Storage & privacy changed with later product work (writing-hook and diagnostics controls), not judged here."}},{"criterion":"AC4","outcome":"pass","revision":"7c8402fca46b638fb589866ad5dc23ba73807a22","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-0974-dispose-windows-defender-finding-and-complete-native-settings.md","detail":"Both images' runs used a private temporary profile (AUTOMATED_PROFILE_PRIVATE_OK), image/bundle/build-copy digests unchanged before and after every run, no better_pull_review.exe left after any run, no Defender event caused; no sign-in, provider mutation, updater installation or destructive maintenance. Release qualification remains RFC-0038."}}]
---

# Dispose Windows Defender finding and complete native Settings qualification

## Owner transfer, distinct from observations

Owner amendment r2 to run `rfc-0018-0026-closeout-20260920` explicitly transfers
the remaining Windows security disposition, full Settings native course and
current inventory refresh from RFC-0018 terminal acceptance. This standalone
card remains **Blocked**, not accepted. The
[amended RFC](../../rfcs/RFC-BPR-0018-robust-settings-center.md#owner-amendment-r2--2026-09-21)
and [native evidence](../../verification/BPR-0593-native-settings-continuation.md)
retain the original scope and results. No detection is called a false positive.

## Exact finding and dependency

Configured native station `stn-a5155427` / `bpr-win`, reached through SSH
`aito-main-win`, is `C:/code/git/bpr-windows` on `windows-lane`. Its observed
HEAD is `b9cf4edb316dceaf8c7ea75f4c7321ce353651e8`; its existing dirty MASK lock
was preserved. The task-owned source was clean at the candidate above.

The MSVC 19.51 / Qt 6.11.2 x64 Release, tests-OFF, distribution-ON candidate
passed the View/Settings, Ctrl-comma, modeless, Escape and reopen prefix, then
stopped at a missing Profiles menu. The next launch found no image. Defender
quarantined these exact paths:

- `C:/Users/mikem/bpr0018-native-280167ac/installed/better_pull_review.exe`
- `C:/Users/mikem/bpr0018-native-280167ac/b/src/app/better_pull_review.exe`

Threat: `Trojan:Win32/Bearfoos.A!ml`, ID `2147731250`. Detection timestamps are
`2026-09-22T04:07:40.551Z` and `2026-09-22T04:09:45.239Z`. Defender reports
successful action; the staged image is absent. The previously generated SPDX
records SHA-256 `341e25b20111f48b7882d8c0a288de477cbcfd41eefc24b3bbbf861f1281e768`;
this is not a post-quarantine executable hash. `IsActive=false` and
`DidThreatExecute=false` do not establish that no earlier prefix ran.

The external prerequisite is a security-owner disposition of this exact finding
and a cleared, provenance-bound candidate authorized for execution. Do not
restore quarantine, add exclusions, disable security, rename/rebuild around the
detection, or execute a different image as a substitute. A vendor submission or
other external disclosure requires its own authorization. The missing-menu
observation remains unclassified while the intervention overlaps the course.
BPR-0960 and BPR-0962 own broader native release/security qualification; their
full closure is not a fabricated dependency of this narrowly scoped follow-up.
BPR-0730 is the already-Done baseline inventory prerequisite.

## Acceptance

- AC1: Record the authorized security disposition with exact source, build,
  dependency provenance, detection identity and cleared executable digest.
  Preserve any unresolved security finding; a successful rebuild alone is not
  a disposition or false-positive determination.
- AC2: On the cleared tests-disabled candidate in the actual ordinary-user
  Windows input desktop, complete the native menu/Ctrl-comma, modeless focus,
  singleton, Escape, same-profile sibling close/reopen and Quit course. Bind
  every observation to the image/PID and require terminal process exit 0.
  Classify and correct any reproduced product failure, including Profiles,
  without substituting Qt unit tests or the earlier prefix for the full course.
- AC3: Capture all seven current pages and compare their actionable names,
  enabled/check states and ordering to the comparable Mac/Linux evidence.
  Explicitly inventory later updater, CLI-capability and seven maintenance
  rows; document only real structural capability differences. Retain missing-
  control and changed-state negative controls in the comparison.
- AC4: Preserve private-profile isolation and unchanged live-station hashes;
  account for every app/observer/task, release the lease and retain no writer.
  No sign-in, provider mutation, updater installation or destructive maintenance
  is authorized by this Settings course. Full release qualification remains
  RFC-0038, not a consequence of this card's success.

## Ready commands and retained evidence

Only after AC1 supplies an authorized cleared image, rebind the task-local
launcher to that exact source/digest and a fresh output path. The retained guard
uses `station_lease.py` plus `qualification_guard.py`, a limited interactive
scheduled task, a 180-second deadline and waited cleanup. It currently names the
quarantined path and must **not** be run before clearance:

```sh
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_windows_guard.ps1
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/tests/scripts/settings_inventory_comparison_tests.py
```

The checked-in observer is
[`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1).
For a new current capture, extend the comparison with the exact newly recorded
path and platform capability assertions; the existing eleven passing tests
alone do not cover that new Windows candidate. The historical launch used the
existing `--render-evidence` flag without an output value for its private
profile, not an ordinary no-argument launch or a new application test hook.

The inert root `C:/Users/mikem/bpr0018-native-280167ac` and local
`build/rfc0018-windows-{fixed-course,fixed-course2,defender}.log`,
`build/rfc0018-windows-final-probe.json` retain the precise partial course and
security evidence. No Windows process is left running. Recheck station identity
and leases before any future execution; no old scheduled task is authorized to
continue. This card supplies no security verdict or native pass at creation.

## Station refresh — bpr-main-win, 2026-09-23

Rechecked from the `bpr-main-win` station, an agent session in the owner's
interactive desktop (session 1, `JaegAW\mikem`, not elevated) with the
`main` checkout, under the owner's mandate relayed by the Mac integrator (run
`bpr-main-win-20260923`). The card stays **Blocked** on
`security-owner-disposition`: AC1 is the security owner's decision, and AC2
and AC3 run only on the image that decision clears. This station can run the
course, but it cannot supply the disposition. It did not restore quarantine,
add an exclusion, change Defender, SmartScreen or any other setting, build a
tests-disabled image, or run one.

### Current record, read-only

Read from `Get-MpThreatDetection` and `Get-MpComputerStatus` at
2026-09-23T17:03:51Z:

- The finding is unchanged. Threat `2147731250` at the two paths and times
  above, both with `ActionSuccess` true. No other Defender detection names
  BetterPullReview on this host, including during the day's tests-enabled
  Debug `better_pull_review.exe` runs for [[BPR-0980]].
- Defender client 4.18.26080.4, signatures 1.459.343.0 (updated
  2026-09-22T20:50Z), real-time protection on, tamper protection on.
- `C:/Users/mikem/bpr0018-native-280167ac` is still inert: it holds no
  `better_pull_review.exe`, and no scheduled task names BetterPullReview.

### What the owner decides (AC1)

Any one of these would unblock the card:

1. Authorize a fresh tests-disabled Release image at a named `main` commit,
   built in a new task root and scanned on demand by Defender. If Defender
   does not detect it, authorize running that exact digest for AC2/AC3. The
   old finding stays recorded and is not called a false positive.
2. Authorize a false-positive submission of the quarantined sample to
   Microsoft. That is an external disclosure, and its answer becomes the
   disposition.
3. Leave the disposition to RFC-0038's release-security qualification
   ([[BPR-0960]]/[[BPR-0962]]), and keep this card Blocked until then.

### Ready path, revised

The commands under "Ready commands and retained evidence" run from a Mac
worktree through the SSH station `bpr-win` and a scheduled task. On
`bpr-main-win` a cleared image needs neither: run the course in the session
itself. Before
AC2's exit-0 Quit can be judged, two things in
[`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1)
need attention:

- Its `Focus` calls `SetForegroundWindow` from a background process. On this
  host the foreground lock timeout reads 2147483647 ms, so that call takes
  the foreground only when the caller already holds the foreground right.
  [`windows_profile_activation_course.ps1`](../../../tests/scripts/windows_profile_activation_course.ps1)
  shows a launcher method that works in session.
- It chooses File > Quit through UI Automation. [[BPR-0980]] found that this
  path destroys the window while the strip's stack-allocated menu is still
  executing. The Debug CRT asserts `_CrtIsValidHeapPointer(block)` in 4 of 4
  probes. In a tests-disabled Release image the same deletion corrupts the
  heap. Fix that defect, or drive Quit another way, before treating a
  nonzero Quit exit as a finding about Settings.

One observation bears on the unclassified missing-Profiles-menu stop. On the
tests-enabled Debug build at current `main`, the strip's Application menu
opens Profiles > New Window through the same posted click and UI Automation
path in every [[BPR-0980]] run. That is not AC2 evidence: it was not the
cleared tests-disabled image, and it was not the full course.

## Owner decision, 2026-09-24: explain the detection first (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0974-defender-disposition-v001`,
revision "Defender threat 2147731250 on source 2d3bdbdf, rechecked read-only
2026-09-23", fingerprint `7f01c8b7…`, decision
`3482f766-1945-42d2-b175-b2e89434c4e6`, sequence 577, 2026-09-24T19:00:23Z,
`actor: human`: **reject**, with this note, verbatim:

> Explain why this is being triggered.

None of the three dispositions under "What the owner decides (AC1)" is
authorized:

- no fresh scanned image;
- no false-positive submission;
- no deferral to RFC-0038.

The owner first wants to know why Defender classifies the tests-disabled
Release image as `Trojan:Win32/Bearfoos.A!ml`. That explanation is
[[BPR-1110]], a read-only investigation on the Windows station, and this card
now depends on it. The card stays **Blocked** on `security-owner-disposition`.
Once BPR-1110 reports, the disposition is asked again, informed by what it
found, as one question. Every prohibition above still applies:

- do not restore quarantine, add exclusions, disable security, or rename or
  rebuild around the detection;
- do not execute a different image;
- make no external disclosure.

[[BPR-0653]]'s Windows capture needs the same kind of tests-disabled image and
waits in the session; it is not this card's question.

## BPR-1110 reported, 2026-09-24

The explanation the owner asked for is on [[BPR-1110]]: a cloud-delivered
machine-learning verdict on the image's first launch, against a file
structurally identical to an unflagged sibling. The card ends with the
reversible options the disposition question can offer. This card stays
**Blocked** on `security-owner-disposition` until the owner is asked again.

## Owner decision, 2026-09-25: lower the static signals and go ahead (attended session 70834742)

Source: attended session `7083474240820608cdf9ff551ddd298f`, packet
[owner-decisions-20260924-scoped-v001](../../verification/reviews/owner-decisions-20260924-scoped-v001/operator-packet.md),
hand-back at sequence 32. Candidate `BPR-0974-defender-disposition-v002`,
revision "BPR-1110 finding at 94a09016 (finished 657ca3bb); threat 2147731250
on source 2d3bdbdf", fingerprint `debb579e…` equal to the prepared
candidate's in
[session-identity.json](../../verification/reviews/owner-decisions-20260924-scoped-v001/session-identity.json),
decision `5c30287a-e458-4c2c-89fd-d952174016bb`, sequence 24,
2026-09-25T00:01:50Z, `actor: human`: **select**, one of the six choices,
verbatim:

> Lower the static signals: add a VERSIONINFO resource, keep a PDB reference
> in Release (/PDBALTPATH:%_PDB%), and move the 3.6 MB of compressed resource
> blobs out of the executable

Note, verbatim:

> We will do the authentiode signing eventually, but not at this moment. Just
> go ahead with this option and lets hope defender doesn't flag our next runs.

**What this authorizes.**

- The three static-signal changes to the Windows images, as [[BPR-1110]]
  lists them:
  - a `VERSIONINFO` resource;
  - a PDB reference kept in Release (`/PDBALTPATH:%_PDB%`);
  - the 3.6 MB of compressed resource blobs moved out of the executable.
- Then a fresh tests-disabled Release image carrying them, built in a new
  task root, and AC2 and AC3 on that exact digest. The owner's "go ahead"
  covers launching it.

**What it does not authorize.**

- Authenticode signing. The owner will sign eventually, not now; it stays
  RFC-BPR-0038's release work ([[BPR-0960]]).
- A false-positive submission. It was not chosen, so nothing is disclosed.
- Deferral to RFC-BPR-0038. It was not chosen, so the card leaves Blocked.
- Restoring the quarantined sample, adding an exclusion, turning protection
  off, or renaming around the detection. These stay forbidden.

**If Defender flags the new image.** BPR-1110 says the static-signal changes
lower the risk without removing it, and the owner's note says the same. A
new detection is recorded the way the first was: threat identity, image
digest and the Defender events. Nothing is restored or excluded, and the
disposition goes back to the owner as one question.

AC1 now reads against this disposition: it is met when the three changes are
in the image, the image's source, build and digest are recorded, and Defender
has not detected that digest. The 2026-09-22 finding stays recorded and is not
called a false positive.

`security-owner-disposition` is answered, and both dependencies are Done, so
the card leaves Blocked for **Ready** on Windows, for bpr-main-win.
[[BPR-0653]]'s Windows capture waited on this answer and needs the same kind
of image.

## The three static-signal changes, bpr-main-win, 2026-09-24

Run `bpr-main-win-20260923`, `main`, dispatched by the coordinator under the
integrator's relay of decision `5c30287a`. Windows only: macOS and Linux build,
embed and package exactly as before.

**`VERSIONINFO`.** `cmake/BprWindowsImage.cmake` gives the application and both
helpers a version resource from `packaging/windows/version.rc.in`:

- `CompanyName` GameCoreTech, the vendor CPack already names (now one
  `BPR_VENDOR_NAME` for both);
- `ProductName` BetterPullReview, and a `FileDescription` per image;
- `FileVersion` and `ProductVersion` 0.1.3, the project version (numeric
  0,1,3,0);
- `InternalName` and `OriginalFilename`, for example `better_pull_review.exe`.

It states no copyright or licence, because the owner has not approved
distribution terms.

**A program database reference in Release.** `cmake/Hardening.cmake` links the
Release configuration of every first-party executable with `/DEBUG
/PDBALTPATH:%_PDB%`. It adds `/OPT:REF /OPT:ICF`, because `/DEBUG` alone would
turn off unreferenced-code removal and folding. The image gets a CodeView
record naming only `better_pull_review.pdb`. The database stays in the build
tree and is not installed. The Debug link is unchanged, and so is its
`/INCREMENTAL:NO` ([[BPR-1027]]).

**The compressed Qt resources out of the executable.**
`cmake/BprQtResources.cmake` builds one binary resource file,
`better_pull_review.rcc`, and in the tests-disabled Windows graph it ships
beside the executable. It holds:

- the application icon;
- the ten Dif strips, stored uncompressed as the runtime requires;
- KSyntaxHighlighting's syntax definitions. That graph now builds the framework
  with `QRC_SYNTAX=OFF` and bundles the framework's own `syntax-data.qrc`
  instead.

The build compiles the bundle's SHA-256 into the image.
`src/app/src/installed_resources.cpp` reads the bundle once, before
QApplication, and requires that digest. It then registers the bundle and keeps
it registered until QApplication is gone. A missing, unreadable, altered or
foreign file ends startup with exit 1 and this line on standard error, instead
of an image without its icon, Dif or highlighting:

```text
INSTALLED_RESOURCES_REFUSED:better_pull_review.rcc:<missing|unreadable|digest-mismatch|not-a-resource-file>
```

KSyntaxHighlighting's theme data, 53,776 of BPR-1110's 3,572,480 bytes, stays
in the image. The pinned framework initializes it unconditionally, and moving
it would mean patching the upstream library.

**Packaging and audits.**

- `packaging/Install.cmake` installs the bundle beside the image, before the
  deployment script.
- `packaging/package_runtime.py` declares it as Windows runtime data. It
  refuses a runtime without it, does not inspect it as a PE image, and records
  it under `data` in the runtime closure. The SBOM hashes it like every
  installed file.
- `scripts/verify_release_package.py` requires it in a Windows package and gives
  it the fixture and canary byte controls.
- `scripts/verify_dif_asset_boundaries.py` accepts the bundled layout: exactly
  the ten strips, uncompressed, with their bytes in the bundle and not in the
  image.
- `scripts/verify_live_mutation_isolation.py` passes over a helper's compiled
  version resource (`.res`), which has no symbols for `dumpbin` to read. It
  recognises one by its content, never by its name.

**Debug stays embedded, deliberately.** Every tests-enabled Windows graph, Debug
and `windows-dev` Release alike, still embeds its resources, so no existing
resource-dependent row changes. Those graphs also build the same bundle, and
the new contained row `application_installed_resources_contract` holds it:

- the digest the image compiles in is the bundle's;
- a missing file, a directory, an empty file, one bit altered, a truncated
  file, and a foreign file named by its own digest are each refused for the
  right reason, and none registers anything;
- the bundle beside the executable registers through `load_image_resources()`,
  the function the image calls;
- it carries the nine icon PNGs, the ten strips uncompressed and byte for
  byte, and every file KSyntaxHighlighting's collection names, index included;
- releasing the owner unregisters it.

**Reseal.** `application_review_round_trip_composition_contract`'s
production-manifest pin, for `main.cpp` and the two new sources. The review is
in the contract's header. The full-application and mutation-branch pins are
unchanged.

**Verification** (`dev` Debug and `core-only` builds on JaegAW, MSVC 19.51,
Qt 6.11.2). All pass:

- `application_installed_resources_contract`, `application_icon`,
  `qt_dif_runtime`, `qt_syntax_pane_composition` and
  `syntax_classification_contract`;
- `application_icon_assets_contract` and `dif_assets_contract` (with a new
  bundled-layout case);
- `package_runtime_contract`, `release_package_audit_contract` and
  `release_source_assets_contract`;
- `cfg_complete_link_contract`, `platform_configuration_contract` and
  `dependency_modernization_contract`;
- `live_mutation_isolation` and `live_mutation_isolation_contract`;
- `architecture_boundaries`, `architecture_boundary_contract` and
  `application_review_round_trip_composition_contract`.

`test_network_isolation_manifest` fails only on its known [[BPR-1108]] rows,
and none of them is new.

**The remaining lever.** The owner has deferred Authenticode signing ("We will
do the authentiode signing eventually, but not at this moment"). It stays
RFC-BPR-0038's release work ([[BPR-0960]]). These three changes lower the
static signals, and BPR-1110 already found they cannot remove the risk:
reputation and a signer are what the cloud verdict leans on.

## The candidate, bpr-main-win, 2026-09-24

**Source and build.** `main` at `85e522d86f29230454c72b4d965fa2e06986e394`,
pushed through the station gate. It was cloned clean into the new task root
`C:/Users/mikem/bpr0974-candidate-85e522d8`, which holds `source/`, `b/` and
`installed/`; the clone was still clean after the install.

- Preset `windows-app-release`: Ninja, Release, `BUILD_TESTING=OFF`,
  `BPR_DISTRIBUTION_BUILD=ON`.
- MSVC 19.51.36257.0 (toolset 14.51.36231) and Qt 6.11.2, from the checkout's
  CPM cache. KSyntaxHighlighting 6.28.1 was built with `QRC_SYNTAX=OFF`.
- `cmake --install b --prefix installed` deployed the restricted runtime,
  notices, SBOM and separate source asset. The notice was VS 18's
  `Licenses/1033/Redist.txt`. The upstream source archives were copied from
  the BPR-0014 M9 task root; the install verified their digests and used no
  network.

**Bytes.**

- `installed/better_pull_review.exe`, SHA-256
  `6aa27c8de85604d38a04c8728c610d2afb1fbcbd541e489bdedcc3891d798ac4`, is
  byte-identical to `b/src/app/better_pull_review.exe`.
- `installed/better_pull_review.rcc`, SHA-256
  `ca3bc2b191085ef61a5225369565376e163b4efb80920b2b52e72e68bf048f91`, is
  3,543,222 bytes. The image carries this digest.
- The agent supervisor is `f951e65c…` and the MCP context helper `9fd2d9b8…`.
- `installed/sbom.spdx.json` is `3431ca1b…`.

The task root's own `scripts/verify_release_package.py` reports
`RELEASE_PACKAGE_OK` over `installed/`:

- 23 files, bound to the SBOM;
- 16 PE images, the first-party ones through their linker maps;
- the bundle as declared runtime data;
- the separate source asset `11babc5f…`.

The M9 audit is not trust or release approval.

**Static properties against BPR-1110's flagged image.** The flagged file is
quarantined, so its column is BPR-1110's: its own linker map and its
structurally identical M9 sibling `67f5e514`, measured again here with the
same reader. The candidate's column was read from its files, which were not
run.

| | Flagged image, `2d3bdbdf` | Candidate, `85e522d8` |
|---|---|---|
| Size | 12,931,584 bytes (the sibling) | 9,640,960 bytes |
| Whole-file entropy | 7.063 bits/byte | 6.466 |
| `.text` | 6,822,912 bytes, 6.342 | 7,026,176 bytes, 6.339 |
| `.rdata` | 5,107,712 bytes, 7.573 | 1,602,560 bytes, 5.937 |
| Qt resource blobs in the image | 3,572,480 bytes at 7.965: syntax 1,643,632, Dif 1,580,128, icon 294,944, theme 53,776 | 53,776 bytes at 7.994, theme only |
| `VERSIONINFO` | absent | GameCoreTech, BetterPullReview, 0.1.3, `better_pull_review.exe` |
| Debug directory | `coffgrp` only | CodeView (RSDS) naming `better_pull_review.pdb` only, `VC_FEATURE`, `coffgrp` |
| Resources | icon x8, icon group, manifest | the same, and `VERSION` (676 bytes) |
| Helpers | no `VERSIONINFO`, `coffgrp` only | `VERSIONINFO` each, and CodeView naming their bare PDB file |
| Beside the image | nothing | `better_pull_review.rcc`, 7.959 bits/byte: 10 strips (1,580,075 bytes, uncompressed), 9 icons (294,896), 459 syntax files and index (1,641,783, zlib) |
| Signed, subsystem, hardening | no; console; ASLR (high entropy), NX, CFG | unchanged |

`.text` grew because `85e522d8` carries two days of product changes since
`2d3bdbdf`, not because of these three changes. The image contains no build
path to a PDB. The bundle digest occurs once in the image, as does the refusal
line.

**A trial image, not the candidate.** Before pushing, the checkout's
`build/windows-app-release` built the same product tree (unpushed `020f9ed5`,
image `517f6b8a…`) to catch configure and link errors. It was never run. On
it, `scripts/verify_dif_asset_boundaries.py`'s `app-release` profile passes
the bundled layout: ten accepted strips, in the bundle and not in the image.
That verifier's `dev` and `core-only` profiles cannot pass on Windows for an
older reason: `dif_assets_contract` runs uncontained there, so its registration
has no network-isolation launcher. That is [[BPR-1125]].

**Defender before the first launch, read-only.** Read with
`Get-MpThreatDetection`, `Get-MpComputerStatus`, `Get-MpPreference` fields and
the operational log, at 2026-09-25T01:44:39Z and again at 02:05:13Z, after the
candidate was built, installed and read:

- The same three detections as before: the two 2026-09-22 BetterPullReview
  ones and the unrelated 2026-09-21 command-line verdict.
- The newest operational record is 6545 (01:35:16Z). No event 1116 or 1117
  appears in the last 24 hours.
- Client 4.18.26080.4, engine 1.1.26080.3, signatures 1.459.384.0.
- Real-time and tamper protection are on, with Advanced MAPS and
  `SubmitSamplesConsent=1`.

Building, installing and reading the candidate and the trial produced no
Defender event.

**Not yet done: the first launch.** It is a foreground run and waits for the
coordinator's clearance. It launches `installed/better_pull_review.exe` once,
with `--render-evidence` and no value, so the run gets a private temporary
profile, as the 2026-09-22 course did. It waits for the window, holds 8
seconds, posts `WM_CLOSE`, and waits for exit. It then reads, without changing
anything, `Get-MpThreatDetection` and every operational event after record
6545, printing the full text of events 1116, 1117, 2010 and their
neighbours, 20 seconds after exit. The script is
`build/logs/bpr-0974-main-win/first-launch.ps1` on the station. If Defender
flags the image, that is recorded here and nothing is retried, rebuilt,
restored or excluded.

**Where the criteria stand.** AC1, as the owner's disposition amends it:

- the three changes are in the image;
- its source, build and digests are recorded above;
- **pending:** that Defender does not detect that digest on its first launch.

AC2 through AC4 run on this image once AC1 holds.

### Checkpoint, 2026-09-25T02:19Z: ready for the first launch

- **The integration pass was stopped at 30 rows.** A headless pass over
  `build/dev` ran with every row offscreen, and with
  `application_windows_title_row` left out because it opens real windows. It
  was stopped at 30 of 419 rows, because at low priority it would have taken
  hours. 29 rows passed, among them `application_profile_*`, `application_epoch_*`,
  `qt_workbench_window_scale_200`, `live_mutation_isolation` and
  `architecture_boundary_contract`.
- **`application_composition_contract` timed out.** It was building its
  tests-disabled Release graph under that load, and it never linked an image.
  That row builds a tests-disabled image and runs it, so it is held back until
  the cleared launch has passed. Its build leftovers were removed.
- **Defender, read-only, after the build.** The log gained three routine
  events 2010 at 02:05:40Z and 02:06:15Z, during the gate push and the start of
  the pass. Each was a time-limited, version `0.0.0.0` cloud update, like 2,138
  of the 2,165 BPR-1110 counted. There was no event 1116 or 1117, and still
  three detections. The launch script reads every event after the newest
  record at launch time.
- **Every task-owned process has stopped.** That includes the builds' `vctip`.
  The candidate and the trial image have never been run.
- **Next:** the first launch, after the coordinator's clearance.

## First launch, 2026-09-25T02:21Z: not detected

The coordinator cleared the foreground run. Before it,
`bpr-main-helpers/foreground-check.ps1` showed Windows Terminal in front and no
WSLg (`msrdc`) window. The staged `first-launch.ps1` then ran once. An earlier
invocation at 02:20:46Z never reached PowerShell: the shell collapsed the
backslashes in the script path, PowerShell refused a path that did not exist,
and nothing was launched. The script was then invoked through a `.cmd` with the
same arguments.

**The run** (UTC):

| Time | What happened |
|---|---|
| before | 3 detections; newest operational record 6548 (02:06:15Z); signatures 1.459.384.0, engine 1.1.26080.3, real-time and tamper protection on |
| 02:21:07.70 | `installed\better_pull_review.exe --render-evidence` started, PID 21344 |
| 02:21:08.46 (+0.8 s) | Window `BetterPullReview - Default profile - BetterPullReview`, class `Qt6112QWindowIcon` |
| 02:21:16.48 | `WM_CLOSE` posted |
| 02:21:16.56 | Exited with code **0** |
| 02:21:36.58 (+20 s) | Still 3 detections; **no** operational event after record 6548 |
| 02:21:52 | Read again: still 3 detections, no new event |

- **The bundle registered.** The image reached QApplication and its window, and
  its standard error is empty; a refused bundle ends the process before
  QApplication with exit 1 and an `INSTALLED_RESOURCES_REFUSED` line. Standard
  output was `AUTOMATED_PROFILE_PRIVATE_OK` alone: the run used a private
  temporary profile, not the owner's.
- **Nothing was quarantined.** All three files were still present afterwards,
  with unchanged digests: `installed\better_pull_review.exe` `6aa27c8d…`,
  `b\src\app\better_pull_review.exe` (the build copy) and
  `installed\better_pull_review.rcc` `ca3bc2b1…`.
- **Nothing else was done.** No scan, exclusion, submission, restore or setting
  change. The process was gone after the run.

The result is the Defender verdict on this one digest at its first launch,
with signatures 1.459.384.0. BPR-1110 found the verdict is per file and
probabilistic, so it says nothing about the next build, and the 2026-09-22
finding stays recorded. It is not called a false positive.

**AC1 is met as the owner's disposition amended it.** The three changes are in
the image; its source (`85e522d8`), build and digests are recorded above; and
Defender did not detect that digest.

**The candidate goes to [[BPR-0653]]** for the integrator's attended capture;
the paths and digests are on that card.

### After the launch, 2026-09-25T02:32Z

**`application_composition_contract` passes.** Run alone, headless, on
`build/dev` at `85e522d8` plus its record commits: 7 of 7 cases in 565 s,
close to its 600 s limit. It configures and builds its own tests-disabled
Release graph, which now carries the external bundle. It runs that image's
`--self-test` twice and a failing `--render-evidence`, offscreen, and checks
its production composition and fixture-free bytes. That is a second, separate
first launch of a new tests-disabled image. Afterwards there were still 3
detections and no operational event after record 6548, and the candidate was
still present. The nested build's `vctip` and `mspdbsrv` were stopped.

**AC2 to AC4 need the foreground; none of them was run.** Each drives the
real input desktop:

- **AC2**, the native Settings course on this image (`installed\`, PID-bound,
  terminal exit 0):
  - the menu and Ctrl+comma;
  - modeless focus;
  - the singleton;
  - Escape;
  - closing and reopening a same-profile sibling;
  - Quit.

  It needs about 3 to 5 minutes of the desktop. Before it runs,
  [`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1)'s
  `Focus` must take the foreground the way
  [`windows_profile_activation_course.ps1`](../../../tests/scripts/windows_profile_activation_course.ps1)'s
  `Front` does. That adaptation can be prepared headless. The Quit defect this
  card named is fixed: [[BPR-1028]]'s `popup()` (`e08a0409`) replaced the strip
  menu's stack `exec()`.
- **AC3**, a UI Automation capture of the seven Settings pages from the same
  image. It needs about 2 minutes of the desktop. Extending
  `settings_inventory_comparison_tests.py` with the new capture is headless
  afterwards.
- **AC4**, private-profile isolation, unchanged live-station hashes and
  accounting for every process and lease. It rides with the AC2 and AC3 runs
  and needs no foreground of its own.

The card stays **In Progress**: AC1 is met, and AC2 to AC4 wait for a cleared
foreground window.

### Checkpoint, 2026-09-25T04:00Z: AC2 and AC3 prepared headless, waiting for the foreground

Run `bpr-main-win-20260923`, segment 11. No foreground was used: the
candidate was not launched. Its three files were only read.

**The course takes the foreground as [[BPR-0980]]'s activation course
does.** [`windows_settings_inventory.ps1`](../../../tests/scripts/windows_settings_inventory.ps1)
was reworked:

- **The launch.** It launches the image itself, once, as the child of a small
  launcher window of its own. The launcher is put in front and gets one F24
  press first, so the image starts with the foreground right.
- **Focus.** Each `Focus` then puts one of the image's windows in front by
  sharing the foreground thread's input state for the call (`Front`). The
  old `SetForegroundWindow` from a background process could not do that
  under this host's foreground lock (2147483647 ms).
- **No pointer.** Clicks are posted messages, menus go through UI
  Automation, and a key is sent only while one of the image's windows is in
  front.
- **Bound to the image.** The image is bound to its SHA-256 and its bundle's,
  and to its PID once the window is up. It runs with `--render-evidence` and
  no value, so a private temporary profile is used.
- **Windows found by title.** They are matched as `BetterPullReview — …
  profile` and `Settings — … profile`, which is what the product sets.
  Qt's appended display name no longer breaks the match.
- **Two modes.**
  - `course`: AC2 with AC3's seven pages, in one launch. Quit must exit 0.
  - `inventory`: AC3 alone, for when the course stops first. It ends with
    `WM_CLOSE`.
- **The record.** The JSON is written whatever happens: pages so far,
  checks, PIDs, exit code, standard output and error, failure, and leftover
  processes. `-DryRun` checks everything short of launching.

The Quit defect this card named is gone: [[BPR-1028]] (`e08a0409`).

**Checked headless.**

- The script parses (PowerShell parser, 2,911 tokens).
- Its dry run against `installed\` binds both digests, `6aa27c8d…` and
  `ca3bc2b1…`. It reads session 1, `JAEGAW\mikem`, not elevated, system DPI
  144 (150%), and the foreground lock timeout 2147483647. It launched
  nothing.
- A dry run with a wrong image digest is refused with exit 1, and writes no
  record.

**AC3's comparison is staged,** in the station's helper folder until the
capture exists, because a committed test cannot name a file that is not
there yet. It adds four tests to `settings_inventory_comparison_tests.py`:

- the capture binds `85e522d8`, both digests, the course's five checks, exit
  0, the private profile and no process left;
- page by page, it matches the Linux `b651554f` capture except for named
  deltas. Known in advance: [[BPR-1107]]'s `Draw white space` selector on
  Appearance. The updater, CLI and maintenance deltas are filled from the
  capture itself;
- it inventories the updater, CLI and seven maintenance rows;
- missing-control and changed-state negative controls on the Windows pages.

Run against a synthetic capture (the Linux pages plus the new selector), the
staged tests pass 3 of 3. Without the stated delta, the comparison fails.

**Read-only state at 03:54Z.**

- Still the same 3 detections; the newest operational record is 6552.
- Signatures 1.459.384.0; real-time and tamper protection are on.
- The candidate's files are unchanged: `6aa27c8d…`, `ca3bc2b1…`, and the
  build copy `6aa27c8d…`.

[[BPR-1125]] is Done, so the Dif boundary verifier's `dev` and `core-only`
profiles now pass on Windows too.

**Next: the foreground runs, after the coordinator's clearance.**

1. `check`.
2. `defender before` and `digests before`.
3. `ac2-ac3`, about 3 to 5 minutes.
4. 20 seconds later, `defender after-course <record>` and `digests after`.
5. `ac3-inventory`, only if the course stopped before the pages.

Each launch is its own Defender verdict. If one is flagged, the runs stop,
and the detection is recorded here the way the first was. AC4's accounting
(private profile, unchanged digests, no process left) comes from the same
records.

## The cleared foreground runs, 2026-09-25T04:21Z to 04:33Z (bpr-main-win)

The coordinator announced and cleared a 30-minute foreground block. Before
the first run, `foreground.cmd check` showed Windows Terminal in front and no
WSLg (`msrdc`) window. Each script verified the candidate's two digests itself
before launching. Every launch was `installed\better_pull_review.exe
--render-evidence`, so every run used a private temporary profile and printed
`AUTOMATED_PROFILE_PRIVATE_OK`, with empty standard error.

| UTC | Run | PID | Result |
| --- | --- | --- | --- |
| 04:21:10 | Defender before | | 3 detections (the two 2026-09-22 ones and the unrelated command-line verdict); newest operational record 6554; signatures 1.459.384.0; real-time and tamper protection on |
| 04:21:15-04:23:17 | Course, attempt 1 | 47964 | Stopped: "Initial signed-out workbench absent". The window was up, but the course's title pattern never matched. The script held a literal em dash, and Windows PowerShell 5.1 reads a file without a BOM in the ANSI code page. Script fixed: it is now ASCII, with the dash as `—` |
| 04:25:22-04:25:37 | Course, attempt 2 | 47628 | Two checks passed: `native-menu-ctrl-comma-modeless-singleton-profile-label` and `escape-and-shortcut-reopen`. Stopped at Profiles > New Window: "Expected one visible menu item Profiles, got 0" |
| 04:26:41-04:26:57 | Course, attempt 3, with leftover menus closed first | 17772 | The same two checks, and the same stop |
| 04:27:36-04:27:47 | AC3 inventory | 3640 | **Passed, exit 0**: View > Settings…, the seven pages, Escape, `WM_CLOSE` |
| 04:31:34-04:31:51 | Course with a diagnosis (a temporary edit to its failure branch, not committed) | 39444 | The same stop, now described (below) |
| 04:33:01 | Defender final | | Still 3 detections; **no operational event after record 6554** |

The BPR-0653 capture and the BPR-1123 real-platform check ran inside the same
block, at 04:28Z to 04:32:48Z; they are recorded on their own cards. The digests of `installed\better_pull_review.exe`
(`6aa27c8d…`), `installed\better_pull_review.rcc` (`ca3bc2b1…`) and the build
copy were read before, between and after the runs, and never changed. A
failed course stops its own image, so the three failed attempts and the
diagnosis end with exit -1 from that stop. No `better_pull_review.exe` was
left after any run. Across the candidate's eight launches, its first at
02:21Z and these seven, Defender recorded nothing.

### AC2: not met. The stop is classified, and it is the 2026-09-22 stop

The course reaches exactly where the 2026-09-22 course stopped ("stopped at a
missing Profiles menu"), now on the cleared image:

- **Passed, bound to the image and its PID:**
  - View > Settings… through the strip's Application menu opens one Settings
    window, labelled with the profile;
  - it is modeless;
  - Ctrl+comma from the workbench reuses and focuses that same window
    (singleton);
  - Escape closes it, and Ctrl+comma reopens it.
- **Stopped:** the second use of the Application menu. The diagnosis run
  listed the image's windows at the stop
  ([record](../../verification/BPR-0974-windows-native-settings/windows-course-stop-85e522d8.json)):
  - `BetterPullReview`, class `QMenu`, on screen at 972,423, 160x189 physical
    pixels. **The menu is open**;
  - that `QMenu` exposes **no menu item** to UI Automation, on screen or off;
  - invoking the Application menu control through UI Automation
    (`InvokePattern`) instead of a posted click shows the same thing:
    `AFTER_UIA_INVOKE visible=` is empty.

  The first opening of the same menu, on a fresh window, exposes its items:
  View > Settings… is found and invoked in every run, and so is the inventory's.
  So the strip's Application menu, opened a second time, is **inaccessible**:
  a screen reader or any UI Automation client would find it empty. It is a
  product accessibility defect (in the strip's menu, or in Qt 6.11.2's
  Windows UI Automation bridge for a `QMenu` shown again with `popup()`), not a
  missing Profiles menu. The earlier stop was this, unrecognised.
- **Not reached:** Profiles > New Window, the sibling's menu and shortcut,
  caption close, retiring the first window, and File > Quit's exit 0.

AC2 asks to "classify and correct any reproduced product failure, including
Profiles". It is classified. The correction is product work, not done in this
pass: find why the second popup's accessible children are missing, fix it, and
pin it in a Qt row. Only then can the full course, with its exit-0 Quit, run
again on a cleared image.

### AC3: met

[`windows-inventory-85e522d8.json`](../../verification/BPR-0974-windows-native-settings/windows-inventory-85e522d8.json)
holds all seven pages: 495 nodes, DPI 144 (150%). It is bound to `85e522d8`,
the image's and the bundle's digests, and its private profile.
[`settings_inventory_comparison_tests.py`](../../../tests/scripts/settings_inventory_comparison_tests.py)
compares it, 22 of 22 passing:

| Page | Against Linux `b651554f` | Against macOS `2d3bdbdf` |
| --- | --- | --- |
| General: the updater, 9 rows | identical: automatic check on and disabled, account combo disabled, six actions disabled, View Release enabled | identical |
| Accounts & hosts | identical, 3 | no "Use GitHub CLI". The CLI profile is macOS-only (RFC-BPR-0015): the one structural capability difference |
| Reviews & queue | identical, 35 | identical |
| Appearance | +"About white space", +"Draw white space" ([[BPR-1107]], `eb9a94a9`); −"Diff hunk header background", −"Diff hunk header text" ([[BPR-0986]], `8f0ec40b`) | the same four |
| Notifications, Agents & context | identical, 4 and 3 | identical |
| Storage & privacy: 11, the last seven the maintenance rows | identical | identical |

Every difference is a named product change since the Linux capture or the
CLI capability; each is stated once in `WINDOWS_DELTAS` and checked both
ways.

**Negative controls,** in the comparison itself:
- a Windows checkbox with its enabled state flipped, its check state flipped,
  or removed is caught;
- a capture missing the stated `Draw white space` combo is refused;
- a capture with the removed hunk-header row back is refused;
- a disabled "Prepare profile purge…" is refused;
- a missing "Pause notifications" is refused.

With `WINDOWS_DELTAS` emptied, the comparison fails at "Appearance: other
controls differ".

### AC4: met for these runs

- **Private profile.** Every launch was a private temporary profile.
- **Nothing changed.** The candidate's files kept their digests.
- **Nothing left behind.** No app, course or observer process remained. No
  station lease or scheduled task was used; this station runs in the
  session.
- **No forbidden effect.** No sign-in, provider mutation, updater action or
  maintenance was invoked.
- **No writer retained.** The courses' records are station logs under
  `build/logs/bpr-0974-main-win/`. The two committed records carry no
  editable value.

### Where the card stands

AC1, AC3 and AC4 are met. **AC2 is not**: it waits for the Application menu's
accessibility defect to be corrected, then for one more cleared course run of
a candidate carrying the fix. That needs a new tests-disabled image, whose
first launch is its own Defender verdict. The card stays **In Progress**.

## Checkpoint, 2026-09-25 (bpr-main-win, wind-down): the claim is kept

The owner asked the stations to wind down, and asked them to release the
claims they would not finish. This one stays In Progress.

Moving the card rewrites the link to it in
`docs/verification/BPR-0653-2026-09-25/index.md`. That file is the capture
index the owner reviewed. The review packet's `source-manifest.txt` pins it
by hash, and the documentation gate refuses the changed bytes. Neither file
may be rewritten to allow the move.

Where the card stands has not changed: AC1, AC3 and AC4 are met, and AC2 is
not.
AC2 waits on [[BPR-1136]]: the strip's Application menu exposes no items to
UI Automation when it opens a second time. That card is reserved and not yet
written.

- **Evidence.** The checkpoint and the logs are station records under
  `build/logs/bpr-0974-main-win/`. The owner's BPR-0653 image
  `C:\Users\mikem\bpr0974-candidate-85e522d8\` stays reserved and must not
  be touched.
- **Next, in order** (the station's intended queue):
  1. BPR-1128's Windows rows.
  2. [[BPR-1138]].
  3. [[BPR-1136]], after RFC-BPR-0052's milestone 1 lands.
  4. A new tests-disabled `windows-app-release` image in a new directory,
     with its own first-launch Defender check (foreground).
  5. This card's AC2 course on that image (foreground). Then record which
     image met each criterion.

## A new candidate carrying BPR-1136, bpr-main-win, 2026-10-01

Queue item 3, [[BPR-1136]], is done. The strip's Application menu now gives
every reopening its own accessible items. It is pinned in
`qt_workbench_strip_menu`, with a negative control. Item 4 follows: a new
tests-disabled image carrying that fix, in a new task root. The owner's
BPR-0653 image in `bpr0974-candidate-85e522d8\` was not touched.

**Source and build.**

- `main` at `7c8402fca46b638fb589866ad5dc23ba73807a22`, pushed through the
  station gate, cloned clean into `C:\Users\mikem\bpr0974-candidate-7c8402fc`.
- The same steps as `85e522d8`: preset `windows-app-release`, MSVC 19.51,
  Qt 6.11.2 from the CPM cache, and the same digest-verified upstream source
  archives.
- `cmake --install` into `installed\`. No `warning` line.

**Bytes.**

- `installed\better_pull_review.exe` is SHA-256
  `1947fa54a4d9886e3ae13e9a541ecaa5cccfb1bbdee2a901dd8e23dc5838e5f2`,
  byte-identical to `b\src\app\better_pull_review.exe`.
- `installed\better_pull_review.rcc` is SHA-256
  `fd94a0e6dfc9703870adf38b25076787552519939d133b8af647eb8620d7971c`.
- `installed\sbom.spdx.json` is `49203ca1…`.

`verify_release_package.py` reports `RELEASE_PACKAGE_OK` over `installed\`:
23 files, 16 PE images, 333 first-party compile units, and the bundle as
runtime data. It is not trust or release approval.

**Static properties, read from the file without running it.** The three
owner-approved changes are all in place:

- `VERSIONINFO` (GameCoreTech, BetterPullReview, 0.1.3);
- a CodeView entry naming only `better_pull_review.pdb`;
- resources in the separate `.rcc`.

| | Cleared `85e522d8` | This candidate |
|---|---|---|
| Size | 9,640,960 bytes | 13,893,632 bytes |
| Whole-file entropy | 6.466 bits/byte | 6.477 |
| `.text` | 6.339 | 6.318 |
| `.rdata` | 5.937 | 5.912 |

The image is larger because it carries a week of product code (Review
Cycles), not because of how it was built.

**Defender, read-only.** Read at 15:40:08Z before the build and at
15:45:16Z after it:

- the same three detections;
- the newest operational record is still 7014;
- no event 1116 or 1117;
- client 4.18.26080.4, signatures 1.459.497.0, real-time and tamper
  protection on.

Building, installing and reading the image produced no Defender event.

**Not yet done: the first launch and the AC2 course.** Both are foreground
runs on the station's desktop. Each has been run before only inside a
foreground block the coordinator announced and cleared. They wait for that
clearance. Nothing has launched this image.

## The 7c8402fc candidate's first launch and AC2 course, bpr-main-win, 2026-10-05

The owner cleared the JaegAW desktop for these foreground runs, and approved
them on the station, on 2026-10-05. Each run was driven by a station
dispatcher bound to this image, and its records are under
`build/logs/bpr-0974-main-win/7c8402fc/`. The image, its bundle and its build
copy were read before and after every run, and never changed: `1947fa54…`,
`fd94a0e6…` and `1947fa54…`. The 85e522d8 image was not touched.

| UTC (2026-10-06) | Run | Result |
| --- | --- | --- |
| 02:30:09 | Foreground check | Claude's window in front, no WSLg (`msrdc`) window |
| 02:30:10 | Defender before | The same 3 detections; newest operational record 7317; signatures 1.459.565.0, engine 1.1.26080.3; real-time and tamper protection on |
| 02:30:16-02:30:45 | **First launch**, PID 12880 | Window `BetterPullReview - Default profile` (class `Qt6112QWindowIcon`) at +0.9 s; `WM_CLOSE`; exit **0**; 20 s later still 3 detections and **no operational event after 7317**; all three files present, digests unchanged |
| 02:30:52-02:31:13 | **AC2 course with AC3's pages**, application PID 4140 | **Passed, exit 0**, `WINDOWS_SETTINGS_INVENTORY_OK` |
| after | Defender after the course | No operational event after 7317; still 3 detections |

**AC2 is met on 7c8402fc.** The record is
[`windows-course-7c8402fc.json`](../../verification/BPR-0974-windows-native-settings/windows-course-7c8402fc.json).
In one launch, bound to the image's and bundle's digests and its PID, it
passed all five checks:

- the native menu, Ctrl+comma, modeless, singleton and profile label check;
- Escape, then reopening with the shortcut;
- the same-profile sibling: Profiles > New Window, its menu and shortcut,
  and retirement;
- the seven categories, each with its page heading;
- File > Quit, with exit **0**.

Before [[BPR-1136]], the second use of the strip's Application menu exposed
no item. The course now gets past it. It used a private temporary profile
(`AUTOMATED_PROFILE_PRIVATE_OK`) and left no `better_pull_review.exe`. Its
standard error held one redacted `QT_DIAGNOSTIC:warning content=omitted`
line. `settings_inventory_comparison_tests.py` now binds the record to the
image, the five checks, exit 0, the private profile and no remaining process.
Its two negative controls fail as they should: a stopped course, and another
image's digest. The suite passes 23 of 23.

**Which image met each criterion:**

- AC1: `85e522d8`, by the owner's amended disposition. This image's first
  launch also drew no detection.
- AC2: `7c8402fc`.
- AC3: `85e522d8`, whose Linux comparison is unchanged. On this course's
  capture, five pages match the 85e522d8 inventory control for control.
  Agents & context and Storage & privacy changed with later product work,
  the writing-hook controls and the diagnostics controls. This card does not
  judge those.
- AC4: both runs used a private profile, kept every digest unchanged, left no
  process and changed nothing in Defender.

**The card stays In Progress for one reason.** Moving it to `60-done/`
rewrites its link in `docs/verification/BPR-0653-2026-09-25/index.md`, which
the review packet pins by hash (checkpoint of 2026-09-25 above). How to close
it is the integrator's decision.
