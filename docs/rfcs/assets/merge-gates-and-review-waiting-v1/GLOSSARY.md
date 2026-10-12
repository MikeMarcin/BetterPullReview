# Pack glossary

Read with the existing [queue vocabulary](../../../architecture/review-queue.md#typed-vocabulary)
and [domain model](../../../architecture/domain-and-state-model.md). These
terms clarify this pack; they do not rename the project's wider domain.

| Term | Meaning here | Avoid confusing it with |
|---|---|---|
| Check result | A check run or commit-status context for an identified commit, with an observed state/outcome | A complete merge verdict |
| Required check | A result or expected context that accepted policy evidence requires for the PR | Every failed check |
| Merge requirement | An applicable condition for merging, with known satisfied/unmet/unknown evaluation | A BPR merge action or permission grant |
| Review request | A current request directed to a user or team | A pending unpublished review |
| Submitted review | An actor's published comment/approve/request-changes disposition | Proof that every applicable approval rule is satisfied |
| Code-owner request | A review request GitHub marks as created for a code owner | An inference from team name, CODEOWNERS parsing or team membership |
| Required team review | A team-specific approval obligation evidenced by applicable policy | Every team listed as a possible code owner or ordinary requested reviewer |
| Waiting on others | The existing Home section for authored PRs awaiting review work without higher-priority existing activity | Quiet/current, or a claim that checks have passed |
| Approval minimum | A configured number of required qualifying approvals | The number still missing, requested users, or requested teams |
| Last known | An accepted observation retained with explicit freshness limits | A fresh assertion about GitHub |

The same PR can have passed checks, an unmet review requirement and outstanding
requests together. Likewise, an approving review and a new request to the same
person can coexist. Model and present these as independent facts.
