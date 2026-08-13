# RFC-BPR-0016 GitHub App registration operator review

Status: **pending operator decision; not repository mutation, release, GHES, prerequisite-exception, or RFC closeout approval**

This packet selects the application kind and asks for bounded authority to
register it and capture the real sanitized contracts required by RFC-BPR-0016.
It is independent of the RFC-BPR-0002 prerequisite exception.

## Recommended decision

Approve the ten facts in `decision-subject.txt` as one decision: a public
GitHub App owned by `MikeMarcin`, installable by any account, with only
Metadata read, Contents read, Checks read, and Pull requests read/write. The
choice follows GitHub's fine-grained GitHub App model instead of OAuth App's
broad private-repository `repo` scope.

GitHub documents that device flow uses the public client identifier without a
client secret, that GitHub App user tokens can use refresh tokens, and that app
permissions are intersected with installation and user authority:

- [GitHub App device authorization](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-a-user-access-token-for-a-github-app)
- [GitHub App user-token refresh](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/refreshing-user-access-tokens)
- [Choosing GitHub App permissions](https://docs.github.com/en/enterprise-cloud@latest/apps/creating-github-apps/registering-a-github-app/choosing-permissions-for-a-github-app)
- [OAuth App scopes](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/scopes-for-oauth-apps?apiVersion=2022-11-28)

The exact GraphQL query and all REST routes still require a positive/negative
real app-token proof. Any wider permission requirement invalidates this packet
instead of being added opportunistically.

## Decision-time context

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `decision-subject.txt` | `sha256:719c921d814d13ee66ba2546f7a50286d89200f28cabf4bd5994dda4aadd02b9` | 3426 |
| `docs/rfcs/future/RFC-BPR-0016-github-device-authorization-flow.md` | `sha256:24c24335b40767aa4412e6244752576e6452e12312a6e41f084dcddee427c681` | 15685 |
| `src/adapters/github/src/github_read_contract.cpp` | `sha256:b7c2a5fdf257359faaab88b6df3ac9c28a15958d783a85542afa89339a2116fc` | 61856 |
| `src/adapters/github/src/github_mutation_contract.cpp` | `sha256:476f2c8711d3938255b564eaa57b532794bf43e56ae254f382e73110436c58af` | 46942 |

Changing any decision-subject byte reopens the gate. The code-source hashes
identify the operation inventory this permission decision was made against;
new GitHub operations require a permission-impact review.

## Operator response

To approve, reply with this exact statement:

> I approve the RFC-BPR-0016 GitHub App registration and bounded real-service
> contract capture bound by decision subject
> sha256:719c921d814d13ee66ba2546f7a50286d89200f28cabf4bd5994dda4aadd02b9,
> including all ten facts, with the GitHub user MikeMarcin as owner and the exact stated
> repository permissions. This authorizes no repository mutation, release,
> distribution, GHES registration, or secret-bearing evidence.

To reject, identify the ownership, application-kind, permission, installation,
token-lifetime, or capture fact that must change. Rejection keeps device flow
unavailable and manual token enrollment unchanged.

This response applies only to the exact GitHub.com application registration
and bounded sanitized capture. The RFC-BPR-0002 prerequisite, live-repository,
release, distribution, GHES, and RFC closeout gates remain separate.
