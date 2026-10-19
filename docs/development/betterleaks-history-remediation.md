# Review-session credential investigation and history cleanup

## Disposition on 2026-10-19

The remaining findings identify **one retired loopback review-session token**,
copied into two historical handoffs. The access risk is resolved by the
session's retirement. No evidence found in this investigation requires a
GitHub credential rotation or a GitHub Support purge request. This conclusion
concerns this credential; it is not a general approval to publish the repository.

The original history deliberately still fails the local scan. A retired real
credential is not a synthetic fixture, so it has no allowlist exception. A
separate history-cleanup rehearsal passes the scanner. Applying that rewrite
would require coordinated repository maintenance and explicit push
authorization; it was not applied to the shared repository or GitHub.

## Evidence

The affected locations are:

| File | Historical blob | First file change containing the URL |
|---|---|---|
| `docs/issues/_checkpoints/BPR-1575/handoff.json` | `21d652d139d244a26f44815bcb1a5e65a8f35886` | `26db2cfff133f231863e3d5fc51541a6f8736cf1` |
| `docs/issues/_checkpoints/BPR-1576/handoff.json` | `6dc63cddba6ea974012450718578537a7819b203` | `6c4e60cf3dafcc20f1eac903645b394964955c18` |

The initial history scan reported 18 diff occurrences. An additional exact-value
search read all 117,528 then-reachable Git objects, including commit and tag
messages and objects reached by non-commit refs. It found the value only in
these two blobs. The value was handled in memory and never printed or stored
in a report, command argument or replacement file.

Both current handoffs record session `232e2c2eee171af499adac505a0e561e` as stopped.
The referenced external session state was inspected directly: its ID matched,
its sequence was 10, and its host was null. The recorded server PID no longer
existed. The locally cached attended-review implementation creates a fresh
random token in each `serve` invocation, binds only to loopback, and clears
the host after shutdown. No request was made with the old credential, and no
process or session was modified during this investigation.

Read-only GitHub metadata showed the repository was private, with zero forks
and zero pull requests, 15 branches and 14 release tags. Local remote-tracking
tips agreed with the inspected remote branch tips. The token was absent from
every release tag's current tree. Release asset contents, external clones,
server caches, GitHub uploads and other accounts were not audited by this
exact-value search.

## Prevention and local verification

The managed pre-commit hook remains enabled in the integration checkout.
The local policy now has a dedicated `bpr-review-session-url` rule, and the
agent guide prohibits authenticated URLs in tracked handoffs. Record session
IDs, external state directories and lifecycle status instead.

All nine explicit local integration courses passed with BetterLeaks 1.9.0 on
macOS arm64 under OS-enforced network denial. The additional course checks a
low-entropy review token on IPv4, localhost and IPv6 loopback, both inline
allow comments, actual commit rejection, historical detection and clean
session metadata. The existing courses still check partial staging,
`commit -a`, dependency/hook conflicts and all 35 narrow exception groups.
Current tracked files pass. Nothing was added to CI or normal test discovery.

## Optional history-cleanup candidate

The ignored, owner-private directory
`.cache/betterleaks/remediation-20261019/` contains the local rehearsal:

- `candidate.git`: independent bare repository, with no remote or alternates.
- `source-refs.txt` and `candidate-refs.txt`: captured and rewritten ref maps.
- `rewrite-impact.json`, `verification.json` and `scanner-result.json`:
  metadata-only evidence.
- `candidate.git/filter-repo/`: commit map and first-changed-commit evidence.
- `remove-retired-url.callback`: the exact two-blob transformation.
- `tool.json` and `git-filter-repo`: the locally installed cleanup tool.

The tool is git-filter-repo **2.47.0**, obtained from its upstream-linked PyPI
distribution. The wheel's published SHA-256 was verified before reading its
single Python module; no global package was installed:

```text
wheel:  2cd04929b9024e83e65db571cbe36aec65ead0cb5f9ec5abe42158654af5ad83
module: 67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94
```

The callback replaces the authenticated URL with
`retired-session-url-removed` only in the two exact original blobs above. It
requires exactly one replacement per matching blob. No file, rule or history
range is excluded from scanning. The rewrite used `--sensitive-data-removal`,
`--no-fetch`, preserved commit messages/encoding, and disabled pruning of
empty and degenerate commits. It ran with networking denied.

The rehearsal preserved all **258 captured refs**, including local tracking
refs and tooling snapshots. git-filter-repo's normalization of tracking refs
was reconciled against that inventory; an older divergent tracking tip was
restored unchanged. These local tooling refs are not a proposed publication
set. Never mirror-push this rehearsal to GitHub.

The rewrite changes **623 commit IDs and 25 refs**, including the `v0.3.0`
tag. The first changed commit is
`6c4e60cf3dafcc20f1eac903645b394964955c18`. The tag's source tree is unchanged,
but its identity and commit ancestry change. Published provenance and
references to old commit IDs need an explicit migration plan if a rewrite is
chosen; this rehearsal did not edit release assets or their signatures.

Verification checks the rewritten commits' messages, authors, timestamps and
parent topology, restricts content changes to the two exact URL replacements,
compares every release tag tree, checks object integrity, searches every
stored candidate object for the retired value, and runs the pinned scanner
across candidate history. The candidate has no remaining match for the
retired value and its history scan exits 0.

This is a frozen rehearsal from the ref inventory, including integration
commit `abc56fc96a986c03f417a83bdd6a867e1e7fe150`. Other work continues in the
shared repository. The candidate does not incorporate later work, including
this remediation follow-up. Regenerate and reverify after a coordinated
freeze before considering any remote update. There were 41 local worktrees
at investigation time; none was reset or rewritten.

## GitHub Support and publication

[GitHub's sensitive-data removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)
says revocation can be sufficient. Support only assists with sensitive data
whose risk cannot be mitigated by rotating credentials. This retired local
session token does not establish such a need, so no ticket was filed.

If an additional, non-revocable exposure is discovered, resolve it before
publication. After an approved rewrite has cleaned normal remote refs,
Support can address retained PR references, cached views and server garbage
collection. A request should include `MikeMarcin/BetterPullReview`, the
reason rotation cannot mitigate the risk, the newly verified number of
affected PRs, the final first-changed commits and any orphaned LFS report.
Never include the credential itself. The current rehearsal has zero known
affected PRs; the tool reported LFS was not in use. Recheck these facts at
execution time.

Changing repository visibility, pushing rewritten refs, replacing release
assets and cleaning other clones remain separate actions. Prefer retaining
the existing history for this already retired credential unless a publication
policy requires byte removal. A local clean scan alone does not establish
that GitHub caches or external copies have been purged.
