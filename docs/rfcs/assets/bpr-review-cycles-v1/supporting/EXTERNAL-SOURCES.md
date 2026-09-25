# External technical references

Primary documentation consulted 2026-09-25. These describe underlying capabilities;
BPR's proposed policy is in the RFCs/specs. Revalidate actual admitted Git, API and
harness versions at implementation. No external page overrides the project's host,
credential, execution or release policy.

| Reference | Relevant fact / application |
|---|---|
| [Git hash-object](https://git-scm.com/docs/git-hash-object) | `--no-filters` disables input conversions. Captured raw source bytes must not be confused with Git's potentially transformed committed representation. |
| [Git commit-tree](https://git-scm.com/docs/git-commit-tree) | Creates a commit from a tree and specified parents/metadata. A private one-parent candidate is possible without resetting the user's branch. Signing and author metadata remain explicit capabilities. |
| [Git write-tree](https://git-scm.com/docs/git-write-tree) and [Git environment](https://git-scm.com/docs/git) | Git has index/object plumbing and an independently selected index. BPR still needs controlled environment, object residency and no-hook/filter execution guarantees. |
| [GitHub REST blobs](https://docs.github.com/en/rest/git/blobs), [trees](https://docs.github.com/en/rest/git/trees), [commits](https://docs.github.com/en/rest/git/commits) | Git objects can be created through bounded provider operations. Verify returned object identities rather than equating HTTP success with the intended candidate. |
| [GitHub REST references](https://docs.github.com/en/rest/git/refs) | A new branch reference is a separate operation. This pack does not authorize overwriting an existing ref or force-pushing. |
| [GitHub REST pull requests](https://docs.github.com/en/rest/pulls/pulls) | PR creation specifies base/head and is separate from uploading objects or creating a branch. Permissions, server contracts and failure recovery need independent handling. |
| [Agent Skills specification](https://agentskills.io/specification) | A skill consists of a named SKILL.md with metadata/instructions and optional progressive references. It does not itself create CLI permissions, runtime reachability or uniform support in every harness. |

API documentation may show a newer version than BPR currently admits. This pack does
not silently upgrade that version or promise every GHES release supports the new
publication operations. There is no claim that raw-byte Git storage, JSON schemas,
content hashes or a skill prompt alone form a security sandbox.
