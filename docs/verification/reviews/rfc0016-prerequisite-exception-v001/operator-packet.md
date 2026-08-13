# RFC-BPR-0016 prerequisite exception operator review

Status: **pending operator decision; not GitHub registration, traffic, credential, or RFC closeout approval**

This packet asks whether RFC-BPR-0016 may be implemented and verified on the
available macOS host before RFC-BPR-0002's direct Windows x64 closeout. It does
not authorize GitHub registration, network traffic, credential use, or RFC
closeout.

## Decision requested

Accept or reject the seven facts in `decision-subject.txt` as one bounded
decision. Approval preserves BPR-0035/BPR-0036 and RFC-BPR-0002's `Proposed`
status while allowing only later RFC-0016 implementation and hermetic evidence
after the separate application-registration gates are satisfied.

## Decision-time context

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `decision-subject.txt` | `sha256:64a258848ab1ca1277da2757584d82870ed98618590afbb6cfa479b3d75d4508` | 1954 |
| `docs/rfcs/future/RFC-BPR-0016-github-device-authorization-flow.md` | `sha256:24c24335b40767aa4412e6244752576e6452e12312a6e41f084dcddee427c681` | 15685 |
| `docs/rfcs/RFC-BPR-0002-github-host-auth-capability-layer.md` | `sha256:5007fa1ac89809dc004985d1bd4ed9362883437e7802306ade15ebffc8c3a310` | 8192 |
| `docs/issues/backlog/BPR-0036-windows-x64-target-native-verification.md` | `sha256:9752ff5470e1cd7fa574a812ff73c33bc15c85f0196b8914819a899d292bed15` | 1664 |

Changing any byte of the decision subject reopens the gate. Implementation may
not begin until an accepted response, the required RFC/dependency edit, and the
separate application-registration/wire-contract gate are all committed.

## Operator response

To approve, reply with this exact statement:

> I approve the RFC-BPR-0016 available-target implementation exception bound
> by decision subject
> sha256:64a258848ab1ca1277da2757584d82870ed98618590afbb6cfa479b3d75d4508,
> including all seven facts. This is
> for implementation and hermetic verification only; it does not waive
> BPR-0035 or BPR-0036, change RFC-BPR-0002 status, authorize GitHub
> registration or traffic, or authorize RFC-BPR-0016 closeout.

To reject, identify the fact that must change. Rejection preserves planning
only and leaves BPR-0276 dependent on BPR-0035.

This response applies only to the RFC-BPR-0002 implementation prerequisite.
The application registration, real-service capture, live-repository,
distribution, and RFC closeout gates remain separate.
