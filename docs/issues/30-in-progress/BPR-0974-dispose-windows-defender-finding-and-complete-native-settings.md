---
schema: "implement-rfc.card.v1"
id: "BPR-0974"
title: "Dispose Windows Defender finding and complete native Settings qualification"
status: "In Progress"
rfcs: []
depends_on: ["BPR-0730","BPR-1110"]
priority: "P1"
platforms: ["win"]
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Windows full native menu/shortcut/window-lifetime course","AC-11 current Windows inventory refresh"],"related_cards":["BPR-0960","BPR-0962","BPR-1110"],"owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0974-defender-disposition-v001","decision":"3482f766-1945-42d2-b175-b2e89434c4e6","outcome":"reject","note":"Explain why this is being triggered."},"owner_disposition":{"date":"2026-09-25","session":"7083474240820608cdf9ff551ddd298f","candidate":"BPR-0974-defender-disposition-v002","decision":"5c30287a-e458-4c2c-89fd-d952174016bb","selected":"Lower the static signals: add a VERSIONINFO resource, keep a PDB reference in Release (/PDBALTPATH:%_PDB%), and move the 3.6 MB of compressed resource blobs out of the executable","not_now":"Authenticode signing"}}
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
has no network-isolation launcher. A card for that is requested.

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
