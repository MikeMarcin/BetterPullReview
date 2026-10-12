# Design record

## Scope and provenance

Pack `merge-gates-and-review-waiting-v1`, draft v1, 2026-10-11. Authored from
the owner's request in this chat; no supplied RFC was overwritten. The source
report describes matching totals of 16 passed, 1 failed and 7 skipped, a
Branch Freshness failure, named code-owner review requirements, absent
requested reviewers and an authored PR appearing as Following/Quiet.

The original chat remains the source of the report. It supplies no PR URL,
repository identity, exact host version or live response; none was inferred
or read from personal application data. Examples in the RFCs are sanitized
acceptance scenarios, not captured private repository data. Code grounding was
inspected on main at `2beb98cc9a58eeeeeebb8bdd08cc0af4a49c4e20`; this is a
source-inspection locator, not an implementation baseline or evidence that a
fix ran. [The amendment](PROPOSED_AMENDMENTS.md) records exact live-document
content identities.

Used author-rfc from the user-supplied MASK package
`e9e655e696b389b35d1220bcfc5a226e5946be36676c7dc1826bea8419a57f28` and the
discovered implement-rfc offline kit from package
`0adaa2df297b6c87de7035056b495075f362d6a0b3192e21f02e9c6ef2abbbab`.
Loaded frontend-design from the installed Claude plugin marketplace before
proposing UX behavior. Its native-theme application is described in each RFC;
no mockup or rendered application was presented as verified design.

## Decisions

| Decision | Authoritative requirement/acceptance | Status and actual answer | Rationale and consequences | Affected artifacts |
|---|---|---|---|---|
| D1: Check details belong inside Overview | [Readable results](RFC-DRAFT-0007-inspect-commit-checks.md#readable-results-inside-overview), AC01–AC05 | Requested outcome: the owner cannot inspect the failure. Native disclosure treatment is proposed. | Identify the failing job and read its supplied explanation before any external handoff. | First RFC, first three cards, `inspect-checks` |
| D2: Named code-owner requests are required | [Pending-review roster](RFC-DRAFT-0008-reviewers-and-merge-requirements.md#a-complete-pending-review-roster), AC01–AC02 | Owner reiterated the names requirement; the API question was answered by investigation, as recorded below. No fallback option was accepted. | Retain `asCodeOwner` plus the user/team identity; a generic warning cannot replace accessible names. | Second RFC and its cards, `see-review-blockers` |
| D3: Server assessment, policy, approvals and requests are separate facts | [Merge requirements](RFC-DRAFT-0008-reviewers-and-merge-requirements.md#explain-merge-requirements-in-overview), AC03–AC05 | Proposed behavior within the requested visibility fix and existing GitHub authority contract. | Prevent guessed missing approvals, per-team obligations and auto-merge promises. | Second RFC; glossary; provider amendment |
| D4: Waiting placement survives the new My pull requests section | [Waiting includes required reviews](RFC-DRAFT-0009-preserve-home-waiting.md#waiting-includes-required-reviews), AC01–AC03 | Owner requested a PR like this under Waiting on others. Existing RFC0066 already records their answer: keep waiting PRs there. | Preserve unread-activity precedence; expose the simultaneous failed check without removing the waiting classification. | Third RFC and its cards, `keep-home-waiting` |
| D5: Open/discovery observations must preserve unrelated facts | [Observed facts](RFC-DRAFT-0009-preserve-home-waiting.md#preserve-and-combine-observed-facts), AC01–AC04 | Proposed fix to the inspected wholesale replacement and missing-authorship paths. | Unobserved is not an empty request roster; Follow-only authored PRs need accurate role facts too. | Third RFC, Home host/core work |
| D6: Visibility grants no collaboration or merge writes | [Native and read-only scope](RFC-DRAFT-0008-reviewers-and-merge-requirements.md#native-and-read-only-scope), AC06 | Existing product boundary retained; no new write was requested. | No automated pings, reviewer requests, branch updates, merges or rules changes. | All RFCs and entrypoint |

### Owner clarification and API finding

The author asked whether to show partial details or require exact owner-group
parity if supported APIs could not identify the groups. The owner replied:

> The website clearly says waiting on code owner review from: &lt;team A&gt;, &lt;team B&gt;, &lt;team C&gt;. Can we not get that info?

This was a request to investigate, not a choice of the suggested fallback.
The [documented ReviewRequest fields](https://docs.github.com/en/graphql/reference/pulls#reviewrequest)
include `asCodeOwner` and a requested actor that can be a team. That answers
the presumed missing-data concern for pending code-owner requests. The draft
requires that supported path and its named-team acceptance, retaining explicit
unavailability only for actual access/host/completeness failures. The earlier
fallback question is retired, not recorded as approved. No live read of the
owner's reported PR established exact website/API equivalence.

## Open frontier and explicit deferrals

No unanswered product choice currently prevents the authored scope from being
implemented. The owner's requested named teams remain in scope. Additional
provider mapping, exact limits for any newly introduced collections, type
design and incremental UI mechanics are implementation work under the defined
budgets and observable behavior, not requests for owner confirmation.

Full CI logs/annotations, job reruns, reminders, reviewer assignment, merges,
auto-merge changes and a local CODEOWNERS or rules evaluator are explicitly
outside scope. Absence of the latter does not waive showing provider-marked
code-owner requests. Host-specific schema availability, especially the
repository's GHES 3.18 floor, must be qualified during implementation rather
than inferred from current GitHub.com documentation. Unsupported fields retain
the RFC's explicit unavailable behavior and do not hide accessible PR data.

The portable plan defines all requirements; there is no undecomposed future
scope. If implementation discovers that the actual named-team scenario cannot
be met on an otherwise supported path, retain the failing criterion and bring
back that concrete evidence before reducing scope.

## Checks and confirmation

1. **Structural validity:** The discovered companion's offline validator
   passed on the final JSON (SHA-256
   `a3c1643b5da539453a2077e662cdf15ed1e53ec9121592e6252d4ccd48435527`)
   with exit 0 and “Structurally valid; references and semantic coverage
   unchecked.” Command:
   `python3 /Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py /Users/jaeger/Documents/GitHub/BetterPullReview/docs/rfcs/drafts/merge-gates-and-review-waiting/milestone-plan.json`.
   Running the same command with `--render` generated MILESTONES.md; a byte
   comparison against fresh renderer output passed. A local draft audit
   checked all three RFCs, their 16 unique criteria assigned exactly once,
   nine canonical Backlog frontmatters, criterion/card coverage, one complete
   closeout per RFC, dependency targets, local links, whitespace and exact
   schema-copy identity. Python's `jsonschema` package was unavailable, so no
   independent JSON Schema engine was run on the cards. Live consumer
   selector binding and implementation acceptance remain unchecked. The
   repository command `python3 scripts/verify_documentation.py --root .`
   returned `DOCUMENTATION_OK` with exit 0. These are draft-artifact checks,
   not product execution evidence.
2. **Design readiness:** All three milestones have defined observable behavior
   and no outstanding material product question. Current API documentation
   supports the named-request design; it does not prove runtime compatibility,
   policy completeness on every host, or the owner's particular PR.
3. **Confirmation:** The owner requested this fixes pack and reiterated named
   code-owner teams on 2026-10-11. RFC0066 supplies the already settled waiting
   placement. The complete v1 text and rendered implementation have not been
   approved by the owner, and no blanket authoring approval is required. No
   interview, independent review or implementation acceptance is claimed.
4. **Execution eligibility:** Not evaluated by drafting. Authorized
   implement-rfc intake must reconcile live mappings, dependencies, amendments,
   mandate and evidence. Provisional IDs, Backlog cards and a valid plan do not
   establish Ready status; no live counter or issue claim was changed.

## Continuation

Enter through [IMPLEMENTATION_PROMPT.md](IMPLEMENTATION_PROMPT.md), adopt the
proposed amendments through the normal workflow and preserve the canonical
milestone grouping. Start with `inspect-checks`, then `see-review-blockers`,
then `keep-home-waiting`. Verify actual capability and public paths before
recording acceptance; do not repeat the retired fallback question merely
because the original implementation lacks the documented fields.

During authoring, the separate draft-chip work was adopted as RFC-BPR-0067
and BPR-1389 became In Progress. Its uncommitted source changes were left
intact. The inspected diff still leaves the waiting-count replacement and
role-none mechanisms described in this pack; the entrypoint requires normal
reconciliation of the shared metadata paths before implementation.
