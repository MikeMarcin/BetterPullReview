---
schema: "implement-rfc.card.v1"
id: "BPR-1110"
title: "Windows: explain why Defender classifies the tests-disabled Release image as Trojan:Win32/Bearfoos.A!ml"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"origin":"owner note on attended candidate BPR-0974-defender-disposition-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 3482f766-1945-42d2-b175-b2e89434c4e6, 2026-09-24","related_cards":["BPR-0974","BPR-0960","BPR-0962","BPR-0653"],"threat_id":"2147731250","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
acceptance: [{"criterion":"AC1 A short explanation, written for the owner, of why this image is classified as Bearfoos.A!ml; each claim tied to its evidence, inference marked, and what cannot be established read-only named","outcome":"pass","revision":"94a09016","evidence":{"kind":"observation","detail":"Card section 'Finding, 2026-09-24 (bpr-main-win)': a four-point owner summary, then facts from Get-MpThreatDetection, Get-MpThreat and the Defender operational log (event 2010 cloud-delivered dynamic signature 1.459.324.1 compiled 04:07:41 and 0.3 s later 1116 FastPath/System/process Unknown on the installed image; 1116 Real-Time Protection/powershell.exe on the build copy at 04:09:45; the only versioned dynamic signature since 2026-07-19), the task-root timeline (linked 04:04:23Z, launched 04:07:37Z, detected 04:07:40.56Z), the static profile from the flagged image's own linker map and the clean sibling's file (unsigned, no VERSIONINFO, no PDB record, console subsystem, 3,572,480 bytes of Qt resource blobs at 7.965 bits/byte, credential/ACL/process/network imports), a ranked inference section, a list of what cannot be established read-only, sources marked as not re-fetched, and seven reversible options for BPR-0974.","path":"docs/issues/60-done/BPR-1110-windows-explain-why-defender-classifies-the-tests-disabled-release-image-as-bearfoos.md#finding-2026-09-24-bpr-main-win"}},{"criterion":"AC2 The difference between the flagged image and the unflagged tests-enabled builds is stated from build facts","outcome":"pass","revision":"94a09016","evidence":{"kind":"command","detail":"Table 'The flagged image against the unflagged tests-enabled builds': windows-app-release Release /O2, BUILD_TESTING=OFF, BPR_DISTRIBUTION_BUILD=ON, release Qt/MSVC runtime, ~12.9 MB, coffgrp-only debug directory, file entropy 7.06 / .rdata 7.57, against build/dev Debug, BUILD_TESTING=ON, distribution OFF, debug runtime (Qt6*d, MSVCP140D, ucrtbased), 32.1 MB, CodeView PDB record, 6.25 / 6.42; same product imports, both unsigned, no VERSIONINFO, console. Stronger still: the unflagged tests-disabled M9 image 67f5e514 has the same 2,889 imports from 22 DLLs and the same resources, section sizes within 16 and 8 bytes (map_compare.py over both linker maps), and src/ differs only by 2d3bdbdf's 6-line shortcut change (git diff 67f5e514 2d3bdbdf -- src). No new image was built or scanned.","path":"docs/issues/60-done/BPR-1110-windows-explain-why-defender-classifies-the-tests-disabled-release-image-as-bearfoos.md#the-flagged-image-against-the-unflagged-tests-enabled-builds-ac2-facts"}},{"criterion":"AC3 Nothing is restored, excluded, rebuilt, renamed, executed, submitted or disclosed; no security setting changes; the station's live state is unchanged, and the station records the commands it ran","outcome":"pass","revision":"94a09016","evidence":{"kind":"command","detail":"Only read-only queries (Get-MpThreatDetection, Get-MpThreat, Get-MpPreference and Get-MpComputerStatus fields, Get-WinEvent on the Defender operational log), file listings and times, Python over linker maps and clean images, and dumpbin /headers /dependents /loadconfig /imports over the clean M9 image, the flagged build's two helpers and the dev Debug image. No image launched, no MpCmdRun, no scan, no quarantine action, exclusion, rename, rebuild, submission or setting change. Get-MpThreatDetection afterwards lists the same three detections and no new one; real-time and tamper protection still on. Commands listed under 'Station record (AC3)'; logs and scripts under build/logs/bpr-1110-main-win/ on the station.","path":"docs/issues/60-done/BPR-1110-windows-explain-why-defender-classifies-the-tests-disabled-release-image-as-bearfoos.md#station-record-ac3"}}]
---

# BPR-1110: Windows: explain why Defender classifies the tests-disabled Release image as Trojan:Win32/Bearfoos.A!ml

## Why

Defender quarantined the MSVC 19.51 / Qt 6.11.2 x64 Release image (tests off,
distribution on, source `2d3bdbdf`) at both of its paths on 2026-09-22 as
`Trojan:Win32/Bearfoos.A!ml`, threat ID `2147731250`
([[BPR-0974]] records the paths, times and the read-only recheck). Asked which
disposition to authorize, the owner rejected all three on 2026-09-24 in
attended session `cbd48b91000b00f1e72e1ad62c7c384c` (candidate
`BPR-0974-defender-disposition-v001`, decision `3482f766`, 19:00:23Z), with
this note, verbatim:

> Explain why this is being triggered.

This card is that explanation. BPR-0974 depends on it and stays Blocked until
the owner can decide with it in hand.

## What is already known

- No tests-enabled Debug `better_pull_review.exe` built and run on the same
  host during 2026-09-23 was detected. Only the tests-disabled,
  distribution-on Release image was.
- The `!ml` suffix denotes a machine-learning classification, not a signature
  for a known sample. `IsActive=false` and `DidThreatExecute=false` were
  reported.
- Real-time and tamper protection are on; signatures 1.459.343.0 at the
  2026-09-23 recheck.

## Scope (read-only)

Find out, and say plainly, what about that image leads Defender to this
verdict. Sources, all read without changing anything:

- `Get-MpThreat -ThreatID 2147731250`, `Get-MpThreatDetection` and the
  `Microsoft-Windows-Windows Defender/Operational` events for the two
  detections (1116/1117 and neighbours): detection source (real-time, on-access
  scan, cloud or ML), detection origin, the process that touched the file, the
  engine and signature versions, and any extra fields the events carry.
- Microsoft's public description of the Bearfoos family and of `!ml` verdicts,
  cited.
- The image's static properties as built, from the retained build tree,
  logs, SPDX and task root, without restoring the quarantined file:
  signing state, PE characteristics and hardening flags, imports that
  heuristics commonly weigh (process creation, job objects, token and ACL
  manipulation, the AppContainer and network-sandbox launcher, self-update
  and file replacement), packing or embedded resources, and how the
  distribution-on build differs from the tests-enabled Debug builds that were
  not flagged.
- Whether the same verdict has been seen for other unsigned Qt or MSVC
  applications, cited, as context and not as a conclusion.

## Acceptance

- AC1 A short explanation, written for the owner, of why this image is
  classified as `Bearfoos.A!ml`. Each claim is tied to its evidence (event
  field, command output, build fact or cited source). What is inferred is
  marked as inference, and what cannot be established read-only is named.
- AC2 The difference between the flagged image and the unflagged tests-enabled
  builds is stated from build facts. If it cannot be pinned without building
  or scanning a new image, the card says so and proposes that step as the
  owner's question for BPR-0974, rather than taking it.
- AC3 Nothing is restored, excluded, rebuilt, renamed, executed, submitted or
  disclosed. No Defender, SmartScreen or other security setting changes. The
  station's live state is unchanged, and the station records the commands it
  ran.

## Boundaries

Read-only on the Windows station. No quarantine restore, exclusion, security
setting change, rebuild around the detection, execution of any image, vendor
submission or other external disclosure. Any of those needs the owner's own
authorization through BPR-0974. No credential, token or private repository
content goes into the record.

## Finding, 2026-09-24 (bpr-main-win)

Run `bpr-main-win-20260923`, main at `94a09016`. Everything below was read
without changing anything; the commands are listed under "Station record".
**Fact** marks what a record or command output shows. **Inference** marks
what follows from the facts but that no record shows directly.

### For the owner, in short

1. **Defender recognised nothing in the program.** The image had sat on disk
   for about three minutes without being detected. Within about three seconds
   of its first launch, Microsoft's cloud sent Defender a new signature. That
   signature was made for this one file, and it was compiled that same second.
   Defender quarantined both copies of the file with it. The `!ml` in the name
   marks a machine-learning verdict. `FastPath` in the event marks a
   cloud-delivered dynamic signature. So this is Microsoft's model classifying
   a file it had never seen, not a match against known malware.
2. **Nothing the program does changed.** An earlier tests-disabled Release
   image is still on this host, unflagged: BPR-0014 M9's `67f5e514`, built on
   the same host 4½ hours before with the same compiler, Qt 6.11.2 and preset.
   Its linker map and the flagged image's map list the same 2,889 imported
   functions from the same 22 DLLs, and the same resources. The section sizes
   differ by 16 bytes of code and 8 bytes of read-only data. Between the two
   sources, `src/` differs only by the 6-line Ctrl+comma shortcut change in
   `2d3bdbdf`.
3. **Why a model might call such a file a trojan (inference).**
   - It is unsigned, and it has no version information and no debug path.
   - It has never been seen before, so it has no reputation.
   - It carries 3.6 MB of near-random data, about 28% of the file. These are
     the compressed Qt resources: the syntax definitions, the Dif sprite
     sheets and the icon.
   - Its import list combines Windows Credential Manager (`CredReadW`,
     `CredWriteW`), token and ACL editing, starting a child process suspended
     and resuming it, and a network client. The product needs each of these:
     token storage, the owner-private profile, and launching Git. Together,
     though, they are the profile of a credential-stealing trojan.
4. **It is a per-file, probabilistic verdict.** The same features in the M8
   and M9 images drew no verdict. As far as this host records, those images
   were never launched. A new build is a new, unknown file, and it may or may
   not be flagged when it is first run. Only an Authenticode signature, with
   the reputation that builds on it, changes that reliably. Options are at
   the end.

### The detection records (facts)

`Get-MpThreatDetection`, `Get-MpThreat` and the `Microsoft-Windows-Windows
Defender/Operational` log; the log is retained back to 2026-07-18.

| Time (UTC, 2026-09-22) | Event | What it says |
|---|---|---|
| 04:07:40.243 | 2010 | Defender "used cloud protection to get additional security intelligence". This one is dynamic signature version `1.459.324.1`, compiled 04:07:41 by the cloud's clock. It lasts until the local signature version changes (`Persistence Limit Type: VDM version`). |
| 04:07:40.557 | 1116 | `installed\better_pull_review.exe`, `Trojan:Win32/Bearfoos.A!ml`, ID 2147731250, Severe, Trojan. `Detection Type: FastPath`, `Detection Source: System`, `Detection Origin: Local machine`, process `Unknown`, user SYSTEM. Signatures AV 1.459.324.0, engine 1.1.26080.3. |
| 04:07:43.243 | 1116 | The same file again, with process `svchost.exe`. |
| 04:07:54.186 | 1117 | Quarantined, action successful. The `installed` directory was last written at 04:07:54.18. |
| 04:09:45.407 | 1116 | `b\src\app\better_pull_review.exe`, the build output that `installed` was copied from. `FastPath`, `Detection Source: Real-Time Protection`, process `powershell.exe`, user `JaegAW\mikem`. So the file was caught when a PowerShell session opened it. |
| 04:09:57.751 | 1117 | Quarantined. `b\src\app` was last written at 04:09:57.75. |

- Of the 2,165 events 2010 in the retained log, 2,138 carry a time-limited
  signature with version `0.0.0.0`. The one at 04:07:40.243 is the only one
  since 2026-07-19 whose signature carries a version and lasts until the next
  signature update. No other 2010 event falls between 03:45Z and 12:02Z.
- `Get-MpThreatDetection` records `DetectionSourceTypeID` 2 (System) and 3
  (Real-time) for the two paths, `CurrentThreatExecutionStatusID` 1 and
  `ThreatStatusID` 3 (quarantined) for both. `Get-MpThreat` reports
  `DidThreatExecute=False` and `IsActive=False`.
- The retained log holds six events 1116 in all. The other three are one
  unrelated `!ml` verdict on 2026-09-21: `Trojan:Win32/Commando.A!ml`,
  against a PowerShell download-and-run command line, not a file, and not
  BetterPullReview. It went through the same `FastPath` path. No event names
  any other BetterPullReview image.
- Settings, read only (`Get-MpPreference`, `Get-MpComputerStatus`):
  - cloud protection is Advanced MAPS (`MAPSReporting=2`);
  - sample submission is "send safe samples automatically"
    (`SubmitSamplesConsent=1`);
  - block at first sight is on, at the default cloud block level and
    timeout;
  - real-time, behaviour and IOAV monitoring are on, and tamper protection
    is on;
  - signatures are now 1.459.374.0.
- The log holds no event 2050 or 2051. So whether a sample was uploaded
  cannot be told from here.

### The timeline around it (facts from the task root)

The task root is `C:/Users/mikem/bpr0018-native-280167ac`, still inert, and
only its file times and text files were read. Its timeline, in UTC on
2026-09-22:

- 03:58:48 `2d3bdbdf` was committed (the Ctrl+comma fix).
- 03:59:40 the task's source directory was created.
- 04:02:13 the build was configured: `windows-app-release`, which is Release,
  `BUILD_TESTING=OFF` and `BPR_DISTRIBUTION_BUILD=ON`, with Qt 6.11.2.
- 04:04:23 the image was linked (map timestamp `6ab1fe47`).
- 04:04:37 it was installed, with an SBOM giving SHA-256 `341e25b2…`.
- 04:07:37 the next course run started and launched the installed image
  with `--render-evidence`.
- 04:07:40.24 the dynamic signature arrived, and at 04:07:40.56 the detection
  followed.
- 04:07:41.47 the course wrote "Expected one visible menu item Profiles, got
  0".
- 04:08:28 the following run found no file.

On BPR-0974's unclassified missing-Profiles stop: the detection came 0.9 s
before that failure was written, and the quarantine came 13 s after it.
These records do not show whether the process was stopped at detection, so
the stop stays unclassified.

Earlier, from 03:53 to 03:55, three runs of the same course failed on the
Ctrl+comma defect that `2d3bdbdf` then fixed. So they ran some image without
the fix, and none of them was detected. This host does not record which
image they ran. The last-access times of the M8 and M9 images fall in that
window (03:52:27–03:55:32; Windows updates last-access at most hourly). The
Mac-side `build/rfc0018-windows-*.log` would say which image it was.

### Flagged and not flagged (facts)

| Image | Build | Detected | Recorded as launched on this host |
|---|---|---|---|
| `2d3bdbdf`, `windows-app-release`, Qt 6.11.2, two copies | Release, tests off, distribution on | **yes**, both copies | yes, 04:07:37 |
| `67f5e514` (BPR-0014 M9), Qt 6.11.2: `b`, the CPack staging, the MSI extraction | the same preset | no | no: packaged, extracted and audited statically |
| `6ad71635` and `9409ff61` (BPR-0014 M8), Qt 6.10.3: build, staging, and an MSI install/uninstall | the same preset | no | no |
| `bpr-windows` `windows-app-release` (2026-09-17), RFC-0016/0017 release images (2026-09-02/03) | Release | no | not established |
| `build/dev` and `build/windows-dev` `better_pull_review.exe` | Debug, tests on, distribution off | no | yes, by ctest, many times |

The retained log has no detection for any of the images marked "no".

### The flagged image against its unflagged sibling (facts)

The flagged file is gone, so its own linker map
(`b/src/app/better_pull_review.map`) stands in for it. The M9 file and the
flagged build's two unflagged helpers were read in place with `dumpbin` and
a Python PE reader, and none of them was run.

- **Same program.** Maps compared symbol by symbol for imports:
  - the same 2,889 functions from the same 22 DLLs, with no difference in
    any DLL;
  - the same resources;
  - code 6,822,913 against 6,822,897 bytes, read-only data +8 bytes, and the
    other sections identical.
- **Not signed.** There is no Authenticode security directory. The bundled
  Qt DLLs are signed.
- **No `VERSIONINFO`.** `packaging/icons/windows/better_pull_review.rc`
  declares only the icon, so the image has no company, product, description
  or file version. The Qt DLLs have them.
- **No debug path.** The debug directory holds only a `coffgrp` entry, with no
  CodeView/PDB record. The Debug image has one.
- **Console subsystem.** The image is subsystem 3 (Windows CUI), although it
  is a GUI application. It has ASLR (high entropy), NX and Control Flow
  Guard, no overlay, and no self-extraction layout.
- **Resources:** eight icon images (379,168 bytes), one icon group and the
  linker's default manifest (570 bytes). Nothing else.
- **Entropy.** Whole file 7.063 bits/byte; `.text` 6.34; `.rdata` 7.57.
  - Four Qt resource blobs in `.rdata` total 3,572,480 bytes at 7.965
    bits/byte, so they are effectively incompressible:
    - KSyntaxHighlighting syntax data, 1,643,632 bytes;
    - the Dif sprite PNGs (`bpr_dif`, `--no-compress`, but PNG is already
      compressed), 1,580,128 bytes;
    - the app icon PNGs, 294,944 bytes;
    - theme data, 53,776 bytes.
  - The rest of `.rdata` is 5.82 bits/byte.
- **Imports that classifiers weigh.** From the map, all direct imports:
  - `ADVAPI32`, credentials: `CredReadW`, `CredWriteW`, `CredDeleteW`,
    `CredFree`.
  - `ADVAPI32`, tokens and ACLs: `OpenProcessToken`, `GetTokenInformation`,
    `CreateWellKnownSid`, `InitializeAcl`, `AddAccessAllowedAceEx`,
    `SetSecurityInfo`, `SetSecurityDescriptorDacl` and
    `SetSecurityDescriptorOwner`.
  - `KERNEL32`, process launching: `CreateProcessW`, `ResumeThread`,
    `InitializeProcThreadAttributeList`, `UpdateProcThreadAttribute`,
    `OpenProcess`, `TerminateProcess`, and the job object calls.
  - `KERNEL32`, file replacement: `CreateHardLinkW`, `MoveFileExW`,
    `SetFileInformationByHandle`.
  - `KERNEL32`, other: `GetProcAddress`, `SetUnhandledExceptionFilter`.
  - `bcrypt`: `BCryptGenRandom`.
  - `Qt6Network`: `QNetworkAccessManager` `get`/`post`, TLS with a
    client-key setter, and proxy credentials.
  - Not imported by the main image: WinHTTP, `CryptProtectData`, and the
    AppContainer APIs (`userenv`). Those belong to Qt, its plug-ins or the
    helpers.
- **The two unflagged helpers from the same build** are also unsigned, have
  no version information and are console images:
  - `better_pull_review_agent_supervisor.exe` (35,840 bytes) imports
    `CreateProcessW`, `ResumeThread`, the job objects and the token calls;
  - `better_pull_review_mcp_context_helper.exe` (119,808 bytes) is the
    other.
  - Neither has high-entropy data: 5.68 and 6.28 bits/byte whole-file.

### The flagged image against the unflagged tests-enabled builds (AC2, facts)

| | Flagged `windows-app-release` | `build/dev` (unflagged) |
|---|---|---|
| Configuration | Release, `/O2 /Ob2 /DNDEBUG`, `/INCREMENTAL:NO`, `BUILD_TESTING=OFF`, `BPR_DISTRIBUTION_BUILD=ON` (compile definition and restricted installed runtime) | Debug, `BUILD_TESTING=ON`, `BPR_DISTRIBUTION_BUILD=OFF` |
| Runtime | `Qt6*.dll`, `MSVCP140`, `VCRUNTIME140`, the UCRT | `Qt6*d.dll`, `MSVCP140D`, `VCRUNTIME140D`, `ucrtbased` |
| Size | about 12.9 MB | 32.1 MB |
| Debug directory | `coffgrp` only | CodeView (PDB path), `feat`, `coffgrp` |
| Entropy | file 7.06, `.rdata` 7.57 | file 6.25, `.rdata` 6.42 |
| Product imports | credential, ACL and process calls, as above | the same product code, so the same calls, plus the debug runtime |
| Signed, `VERSIONINFO`, subsystem | no, no, console | no, no, console |

The compressed resource blobs are the same 3.6 MB in both images. In the
Debug image they are about 11% of the file, and in the Release image about
28%.

### What plausibly drives the verdict (inference)

Microsoft does not publish which features its model used for a verdict.
Ranked by how strongly the facts above support them:

1. **No reputation and no signer.** The file was brand new, unsigned and
   had no publisher metadata. Cloud verdicts lean heavily on prevalence and
   signer. Everything else in the table was the same for the unflagged M9
   sibling, except that the M9 image was, as far as recorded, never launched
   and so never classified.
2. **A large, near-random block inside the image.** A packed or encrypted
   payload looks the same to a static model. The Debug images dilute it
   (6.25 against 7.06 bits/byte) and they were never flagged. They also
   differ in many other ways, so this is not isolated.
3. **The import combination.** Credential Manager, DACL and owner rewriting,
   a child started suspended and then resumed, and an HTTP client. Each has
   a product reason. Together they match what credential stealers and
   loaders import.
4. **Minor:** no PDB path, and a console subsystem for a windowed program.

Other explanations are not supported by the facts:

- A behaviour detection: the source is System or Real-Time Protection, not
  Behavior Monitoring, and the first event names no process.
- A packer or self-extractor: there is no overlay, and the resources hold
  only the icon and the manifest.
- Something the 6-line change added: the imports and layout are unchanged.

### What cannot be established read-only

- The model's actual reasons for this file, and why the sibling with the
  same features was not classified.
- Whether a sample was uploaded (no event 2050 or 2051 is logged).
- Whether a new build would be flagged when first run. Only building and
  running or scanning one would tell, and the owner has not authorised that.
- Which image the 03:53–03:55 runs launched.
- Whether the remediation stopped the process before the missing-Profiles
  failure.

### Sources

Cited from the station's knowledge. This session had no web access, so none
was re-fetched:

- Microsoft's Defender Antivirus event reference: event 2010,
  `MALWAREPROTECTION_SIGNATURE_FASTPATH_UPDATED`, the Dynamic Signature
  Service. See the "event IDs" section of
  <https://learn.microsoft.com/en-us/defender-endpoint/troubleshoot-microsoft-defender-antivirus>.
- The naming scheme `Type:Platform/Family.Variant!Suffix`:
  <https://learn.microsoft.com/en-us/defender-xdr/malware-naming>.
- The threat page Defender itself links for this name:
  `https://go.microsoft.com/fwlink/?linkid=37020&name=Trojan:Win32/Bearfoos.A!ml&threatid=2147731250`.
- `Bearfoos.A!ml` is widely reported against unsigned, freshly built
  executables: PyInstaller, Go and similar toolchains, and unsigned Qt/MSVC
  applications. That is context only. No specific report is cited here,
  because none could be checked from this station.

### Options for the owner (for BPR-0974's question; all reversible)

1. **Authenticode-sign the Windows images.** Use an OV or EV certificate, or
   a managed signing service. This is the strongest lever for both the
   cloud verdict and SmartScreen. Users of an unsigned release face the same
   first-run cloud check. It is release work owned by RFC-0038
   ([[BPR-0960]]), and it needs the owner's certificate.
2. **Add a `VERSIONINFO` resource** to the app and its helpers: company,
   product, description, file version, original file name. This is a small
   product change and a weak signal on its own.
3. **Keep a PDB reference in Release builds** (`/DEBUG` with
   `/PDBALTPATH:%_PDB%`, so no local path ships). Cheap, and weak.
4. **Move the 3.6 MB of compressed resources out of the executable**, into
   an external `.rcc` or resource DLL registered at start. This is moderate
   product work, and it removes the high-entropy block.
5. **Link the distribution image as a GUI-subsystem program**
   (`WIN32_EXECUTABLE`). This changes console behaviour, so it is a product
   decision. The signal is weak.
6. **Submit the quarantined sample to Microsoft as a false positive.** This
   is BPR-0974's option 2, and an external disclosure of an unreleased
   binary. It clears that hash and may retrain the model. It does not cover
   future builds.
7. **Build a fresh tests-disabled image and scan it on demand.** This is
   BPR-0974's option 1. One clean or flagged result says little about the
   next build, because the verdict is per file.

Options 2–5 lower the risk without removing it. Only option 1 addresses
reputation.

### Station record (AC3)

Commands, all read-only:

- `Get-MpThreatDetection`, `Get-MpThreat`,
  `Get-MpPreference` (selected fields) and `Get-MpComputerStatus` (selected
  fields).
- `Get-WinEvent` on the Defender operational log:
  - events 1006–1008, 1015 and 1116–1119;
  - every event from 2026-09-21T20:00Z to 2026-09-22T05:00Z;
  - events 2050, 2051 and 2010.
- File listings and times of the task root and of every
  `better_pull_review*.exe` under the BPR roots. `Get-Item` time queries
  open no file.
- Python over the two linker maps, and a Python PE reader over the clean
  images: sections, entropy, resources and data directories.
- `dumpbin /headers /dependents /loadconfig /imports` over the M9 image, the
  two helpers and the dev Debug image.

What was not done:

- No image was launched.
- No `MpCmdRun` was used, and no scan was started.
- Nothing was restored, excluded, rebuilt, renamed, submitted or
  disclosed.
- No setting changed.

`Get-MpThreatDetection` after the work lists the same three detections and no
new one. The station's logs and scripts are under
`build/logs/bpr-1110-main-win/` and are not committed.
