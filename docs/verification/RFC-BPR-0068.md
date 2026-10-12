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

## Integrated acceptance

The real fixture provider → host → Overview course passes 16 passed, 1 failed,
7 skipped; Branch Freshness and its explanation; admitted recording-browser
handoff; unsafe/missing destination refusal; keyboard disclosure; and retained
selection/collapse after ordinary Refresh. It performs zero provider writes.
Rendered review caught fixed-summary clipping caused by a long commit label
and elided long result names. The corrected layout abbreviates the displayed
head (the normalized observation retains its exact OID), constrains the details
to the summary viewport and wraps result names with native selection/focus.
The final public course passed at 1×, 1.25× and 2× scale, each rendering System,
Light, Dark and the three high-contrast appearances, including 900×600 layout
and 35% enlarged application text. Direct render inspection confirmed complete
16/1/7 totals, wrapped long names and readable Branch Freshness text. The new
width assertion fails if the summary widget exceeds its viewport. Independent critical
review of the final pack remains pending for the coordinator’s final review.

Primary sources: [check runs](https://docs.github.com/en/rest/checks/runs#list-check-runs-for-a-git-reference)
and [commit statuses](https://docs.github.com/en/rest/commits/statuses#list-commit-statuses-for-a-reference).


## Criterion ledger

Candidate: `e126f2c445a43437d23510e3a8e1a2563e5c8a6c`. Product files are committed; later commits update
only tracker/evidence state. Source inputs for the passing evidence are unchanged.

| Criterion | Direct evidence | Result |
|---|---|---|
| AC01 | `application_overview_summary_1`, `_1_25`, `_2`: ordinary Home open reaches Overview; 24 current rows, matching 16/1/7 totals, automatic failure expansion, Branch Freshness and its supplied explanation | pass |
| AC02 | `github_paged_read` distinguishes same-name check run/status, current success from earlier failed status, producer and exact commit. Check-run endpoint’s documented latest filter retains current attempts; known head and unobserved test-merge/requirements are labelled, with partial totals on unavailable collections | pass |
| AC03 | Public Open check details reaches the recording browser once at the supplied exact destination. Missing and JavaScript destinations remain readable and disabled; provider HTML is inert literal text. `application_github_window_read_host` refuses substituted addresses and stale publication identities | pass |
| AC04 | Wrong-head/malformed optional reads retain PR files with unavailable check state; bounded exhaustive paging and hostile continuation/cancellation/authority tests pass. Window-host refresh/authority replacement refuses late results and old link contexts; content and optional details do not share false empty state | pass |
| AC05 | Three public scale courses exercise Space disclosure, accessible names, selection/caret/collapse preservation on Refresh, enlarged/narrow controls and six accepted appearances. Corrected full-window rasters were inspected offscreen; file navigation, split/reader state and Start/Continue review remain functional, zero model resets and zero provider writes | pass |

Final verification: five focused component/provider tests pass; three composed
scale courses pass; core dependency and architecture boundaries pass; CLI
transport contracts pass; clang-format 22 checks all 19 changed C++ files;
documentation verification and its 67 tests pass. Runtime evidence is local at
`/tmp/bpr-merge-gates-run/`, with generated scale rasters under
`build/dev/overview-renders/`. Inspection artifacts are not committed.

These offscreen checks establish deterministic native-widget layout and app
appearance inheritance, not installed-system appearance/platform qualification.
Test-merge selection and requiredness remain explicitly unknown in this first
milestone; the following milestone owns accepted merge-requirement evidence.
No live GitHub/GHES request, rerun, repository execution or collaboration write
was needed. The final independent critical review requested by the user remains
a pack obligation; this first milestone does not certify that review.

Documentation impact: the provider and Overview successor amendments are
reconciled; workbench specification describes the new read-only controls and
limits. The following milestones still own their Home/queue prose amendments.
The adopted plan, immutable packet and criteria retain their scope. Historical
RFC0028 native acceptance remains independent. Existing Home draft acceptance
landed externally as `1cc5c3e71` / `72db6901c` and is preserved.
