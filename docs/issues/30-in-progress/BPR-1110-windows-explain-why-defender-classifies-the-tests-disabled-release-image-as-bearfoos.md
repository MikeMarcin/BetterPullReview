---
schema: "implement-rfc.card.v1"
id: "BPR-1110"
title: "Windows: explain why Defender classifies the tests-disabled Release image as Trojan:Win32/Bearfoos.A!ml"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"origin":"owner note on attended candidate BPR-0974-defender-disposition-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 3482f766-1945-42d2-b175-b2e89434c4e6, 2026-09-24","related_cards":["BPR-0974","BPR-0960","BPR-0962","BPR-0653"],"threat_id":"2147731250","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
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
