# RFC-BPR-0068 verification

Mandate `merge-gates-r1`; plan `merge-gates-and-review-waiting-v1`, milestone
`inspect-checks`. RFC_BASE_SHA: `9857d596769468c0585c475467175e3998a33530`.
Frontend-design was loaded from the official Claude marketplace before Qt work.

## Observation slice

Provider candidate `34f62d7e3` preserves exact check-run identity, producer,
output title/summary, detail destination and evaluated head. Commit statuses use
one bounded exhaustive REST read of history, retaining the first/newest value
per context and preserving a distinct source/identity from check runs. Check
runs use the documented endpoint default `filter=latest`. Missing, malformed,
wrong-head and limited optional collections are unavailable, and valid PR
content survives. Cancellation remains terminal. Global record/text validation
still refuses invalid mandatory PR content after optional details are removed.

`github_paged_read`, `github_read_contract`, `application_pull_request_overview`,
`application_github_window_read_host` and `qt_pull_request_overview_page` passed
in the focused run (5/5). `architecture_boundaries` and `gh_cli_transport`
passed (2/2). `python3 scripts/verify_documentation.py --root .` and its 67
regression tests passed. Initial failures were stale operation/response counts
(14→15 compiled operations, 11→12 observed snapshot responses); the rejection
assertions retain their exact duplicate/authority checks. A composed overlay
initially used the contract placeholder head rather than the served Git head;
its corrected binding uses the committed nested-folders fixture head.

The native/CLI request compiler admits the exact new read route; fixture
contract tests qualify GitHub.com and GHES 3.18 normalization. This is not live
private-organization or native GHES parity. No network mutation was performed.

## Public composition and remaining presentation work

The real fixture provider → host → Overview course passes 16 passed, 1 failed,
7 skipped; Branch Freshness and its explanation; admitted recording-browser
handoff; unsafe/missing destination refusal; keyboard disclosure; and retained
selection/collapse after ordinary Refresh. It performs zero provider writes.
Rendered review caught fixed-summary clipping caused by a long commit label
and elided long result names. Presentation corrections and final AC05 evidence
are still pending at this observation-slice checkpoint. Independent critical
review of the final pack remains pending for the coordinator’s final review.

Primary sources: [check runs](https://docs.github.com/en/rest/checks/runs#list-check-runs-for-a-git-reference)
and [commit statuses](https://docs.github.com/en/rest/commits/statuses#list-commit-statuses-for-a-reference).
