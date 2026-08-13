# Designated live macOS containment operator review

Status: **pending operator decision; not live-publication, packaging, or RFC approval**

This packet presents the bounded architecture decision required by BPR-0262.
It does not materialize an approval, authorize a credential, execute a live
lane, publish a comment, select distribution signing/notarization policy, or
close RFC-BPR-0014 or RFC-BPR-0008.

## Decision requested

Review and either accept or reject all of these facts as one decision:

1. The exact seven facts in `decision-subject.txt` define the verification-only
   macOS containment and authenticated launch topology.
2. A read-only mounted image closes nested-resource replacement while attached;
   it is paired with retained descriptors, mount and inventory checks, and an
   in-image stage-one supervisor rather than treated as origin proof by itself.
3. Supervisor provenance uses only an initially empty Mach `SERVICE_SLOT`,
   immediate restoration of the parent port snapshot, kernel audit trailers,
   and bidirectional Security.framework code-identity validation before Qt.
   Every unavailable or mismatched condition fails closed with no generic
   inherited-pipe, argv, environment, PID, nonce, or persistent-state fallback.
4. This selects an implementation/testing topology, not a release artifact or
   operator workflow; RFC-BPR-0014 remains the authority for distribution.

## Evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `decision-subject.txt` | `sha256:cf71a398143725314c461dcd9c06cea2605e00847e4aee90acf9699c5b7fb6f9` | 1858 |
| `src/app/src/designated_live_macos_supervisor.cpp` | `sha256:c6e9ad0e6673a66994cd7ffd637344c40e603ee61f6600fba4dfdf26a7015277` | 22913 |
| `src/app/main.cpp` | `sha256:449d34b8186d039684f678f51032dae0bd439238e4be1e0f0c49282645fb1b34` | 224798 |
| `scripts/designated_live_macos_capsule.py` | `sha256:7e4d77d0c93a3aa393de60b4a893f0a220986b6e51cbf79b05c0f31657ae32ea` | 25358 |

Read-only host probes established that a UDZO/APFS image attached with
`-readonly` rejects mounted-tree writes, and a native spawn probe established
that registered ports survive suspended spawn and `exec`. The same probes also
established two limits captured in the subject: `TASK_RESOURCE_NOTIFY_PORT` is
not usable for this handoff, and DiskImages does not accept the tested
`/dev/fd/N` image path, so the design does not claim an FD-atomic association
with the pre-attach pathname. A regression must still prove that
`SERVICE_SLOT` is initially unused for the supported Qt launch, is restored on
every spawn result, and causes fail-closed behavior when occupied.

Changing any bound artifact byte reopens this decision. No live lane, network
request, credential, approval, or GitHub mutation was used to prepare it.
The pending packet was rebound after later diagnostic and notification-course
changes in `main.cpp`; the decision subject and every dedicated containment
artifact remained byte-identical, and no verdict had been recorded.

## Operator response

To approve, reply with this exact statement:

> I approve the verification-only macOS read-only-image containment and
> kernel-audited Mach SERVICE_SLOT launch topology bound by decision subject
> sha256:cf71a398143725314c461dcd9c06cea2605e00847e4aee90acf9699c5b7fb6f9,
> including all seven facts and every fail-closed condition, for implementation
> and hermetic verification only.

To reject, identify the fact or evidence that must change. Rejection leaves
BPR-0262 in Backlog and authorizes no implementation, package, or publication.

This response applies only to BPR-0262's macOS verification topology. BPR-0261
replay authority, RFC-BPR-0014 distribution policy, the invalidated BPR-0256
live publication packet, and every live GitHub decision remain separate
attended gates.
