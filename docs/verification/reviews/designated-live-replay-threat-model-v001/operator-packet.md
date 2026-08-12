# Designated live replay threat-model operator review

Status: **pending operator decision; not live-publication or RFC approval**

This packet presents the bounded product/security decision required by
BPR-0261 after capability-critical review proved that current-user-mutable
filesystem state cannot provide deletion- or rollback-resistant monotonic
replay authority. It does not materialize an approval, authorize a credential,
run GitHub, publish a comment, approve release packaging, or close an RFC.

## Decision requested

Review and either accept or reject all of these facts as one decision:

1. The exact seven facts in `decision-subject.txt` define the supported replay
   threat model and reset contract for the macOS designated-live verification
   lane.
2. Same-user or administrator deletion, rollback, restoration, or intentional
   mutation of replay state is explicitly outside the one-use guarantee.
3. Within that boundary, implementation must separate namespace initialization
   from attended launch, bind a fresh namespace identity into every approval,
   retain and revalidate descriptors and marker identity, and fail closed on
   every detected path, concurrency, integrity, or durability failure.
4. Rejection keeps BPR-0261 in Backlog until a monotonic authority outside
   current-user-mutable storage is selected.

## Evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `decision-subject.txt` | `sha256:a1e954aef213ea36e920b6635a8d3542cf783a7a02200ef3730b17c7af71c43f` | 1731 |
| `src/app/src/github_application.cpp` | `sha256:1231342889316149a194766b6463c1efc3e42cfb145009c9b7b8e8d83fe0c991` | 375966 |
| `src/application/src/designated_review_authority_bootstrap.cpp` | `sha256:fabb6db81a93bbdb04a48a5a78efed10965b37ac6fd31b7e9ec48a56a3ffbbef` | 24209 |

The rejected implementation was independently falsified in three ways: the
selected application path recreated an absent replay directory on a later
launch; a same-user watcher could unlink the spent marker after exclusive
creation; and injected `fsync` failure left a live marker without proving that
crash recovery retained it. The source was reverted and BPR-0261 was returned
to Backlog. No live lane, network request, credential, approval, or GitHub
mutation was used.

Changing any bound artifact byte reopens this decision. Approval selects only
the narrowed local replay design in the exact subject; it does not retroactively
approve the rejected implementation.

## Operator response

To approve, reply with this exact statement:

> I approve the macOS designated-live replay threat model and reset contract
> bound by decision subject
> sha256:a1e954aef213ea36e920b6635a8d3542cf783a7a02200ef3730b17c7af71c43f,
> including all seven facts and the explicit exclusion of intentional
> same-user or administrator deletion, rollback, restoration, or mutation of
> replay state, for implementation and hermetic verification only.

To reject, identify the fact or evidence that must change. Rejection leaves
BPR-0261 in Backlog and authorizes no implementation or publication.

This response applies only to BPR-0261's macOS verification-lane replay
authority. BPR-0262 supervisor containment, the invalidated BPR-0256 live
publication packet, distribution policy, and every live GitHub decision remain
separate attended gates.
