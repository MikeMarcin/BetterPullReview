# RFC-BPR-0016 distributable GitHub App registration operator review

Status: **pending operator decision; not repository mutation, release, distribution, GHES, platform capability-flip, or RFC closeout approval**

This packet carries the bounded authority the owner's plain **proceed**
direction of 2026-08-27 called for: registering the separate distributable
GitHub.com application identity for BetterPullReview and running one
designated read-only course that proves the new client works with the
approved permissions. It changes nothing about the private verification app,
authorizes no mutation anywhere, and does not enable any platform — the
macOS capability flip and its clean rerun remain later steps of
[[BPR-0318]] under their own recorded provenance.

## Recommended decision

Approve the nine facts in `decision-subject.txt` as one decision: a second
GitHub App owned by `MikeMarcin`, installable by any account, with device
flow and expiring user tokens enabled, exactly Metadata read, Contents read,
Checks read, and Pull requests read/write, no client secret in any shipped
artifact, installed for the course only on designated `MikeMarcin/BPR-Test`
(database ID `1328277631`).

The shape mirrors the accepted verification registration — the digest-bound
[`official-contract-record.md`](../rfc0016-github-app-registration-v001/official-contract-record.md)
from that decision still records the official GitHub App model documentation
this recommendation relies on — with the one deliberate difference the
distributable identity exists for: the any-account installation policy the
verification app's later private/owner-only amendment narrowed away. The
seeded verification-shape observation in this directory is the expected
template; after registration the sanitized production observation replaces
it and must differ from the verification identity in client identifier and
installability, and in nothing else material.

The read-only course authorizes real device-flow token success for the new
client plus read-only probes of the existing operation catalog, only against
the designated repository and pull request 1 where required. Pull requests
write is inspected as a configured setting only; review-comment publication
proof remains solely under the separate identity-bound designated-live
safe-target authority. Any wider permission requirement invalidates this
packet instead of being added opportunistically.

## Decision-time context

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `decision-subject.txt` | `sha256:18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b` | 4291 |
| `../rfc0016-github-app-registration-v001/official-contract-record.md` | `sha256:0ea5fb0edfbb6debbbe7261b595b8bf6180928120eb98142dce1627dc46a7fc8` | 2771 |
| `registration-observation.json` (seeded verification-shape template) | `sha256:f45b304320dd8acc4561fe122a9110929d0a9080e4973e2a9f5248c4519a2a17` | 1219 |
| `docs/rfcs/future/RFC-BPR-0016-github-device-authorization-flow.md` | `sha256:5e2c6c2fe19de059f361e0bcac4762abd78e3236412a3dd87021dc58f82b6a18` | 17849 |
| `src/adapters/github/src/github_read_contract.cpp` | `sha256:ad883f1beab36718555e9135c519670925ccb4368d715568bbedc217d7614680` | 62164 |
| `src/adapters/github/src/github_mutation_contract.cpp` | `sha256:0cff034ec478c1c3ed18afeacd2461b3b8c235bc2e38e6195d2db757e28c7b46` | 47120 |

Changing any decision-subject byte reopens the gate. The code-source hashes
identify the operation inventory this permission decision was made against;
new GitHub operations require a permission-impact review.

## Operator response

To approve, reply with this exact statement:

> I approve the RFC-BPR-0016 distributable GitHub App registration and
> bounded designated read-only course bound by decision subject
> sha256:18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b,
> including all nine facts, with the GitHub user MikeMarcin as owner, the
> any-account installation policy, and the exact stated repository
> permissions. This authorizes no repository mutation, release,
> distribution, GHES change, platform capability flip, or secret-bearing
> evidence.

To reject, identify the ownership, installation-policy, permission,
token-lifetime, course, or evidence fact that must change. Rejection keeps
every production platform visibly `device_flow_unavailable` with zero
requests and RFC-BPR-0016 open.

This response applies only to the exact distributable GitHub.com
registration and the one designated read-only course. Repository mutation,
release, distribution, GHES, the macOS capability flip, and RFC closeout
remain separate gates.
