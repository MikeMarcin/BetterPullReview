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

Feature candidate: `e126f2c445a43437d23510e3a8e1a2563e5c8a6c`. Final parser/host regression candidate: `ad350a7fc8478509f2aa39d096d5a35e2b5eaae1`. The final 3/3 recheck covers the portable initialization and status-limit regressions; the GUI inputs for the three passing scale courses are unchanged. Later commits update tracker/evidence state only.

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

Closeout also runs a direct negative regression on the new commit-status collection: a tightened raw-record cap and a hostile continuation each retain known check runs/PR content with status availability unavailable. The self-review uses named initialized check records in the extended parsers and new host fixture, preserving values while following the repository’s GCC portability rule.

## Independent critical-review correction

The independent `rfc-reviewer-critical` (`gpt-6-astra`, `xhigh`) reviewed
`15365e9df15ff51b366f9735c887dccdd80cae9e` against the immutable RFC base and
reproduced F1: optional status paging could exhaust mandatory search capacity,
rejecting otherwise readable PR content under AC04. The earlier limit/hostile-link
evidence did not cover this case. The supplied report and dispositions are retained
in [the pack review record](../issues/_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json).

[BPR-1399](../issues/60-done/BPR-1399-correct-merge-gates-critical-review.md)
corrects AC04 at `eae174cb95e7b66c97739b0a8dd1e6c5ac71af2b`: mandatory search
finishes before optional result collections spend the remaining unchanged budget.
`github_paged_read` passes valid 12-page status continuations and a valid 128 KiB
aggregate/page byte-padding probe. Incomplete status history remains unavailable;
known checks, PR files and search survive. This regression failed before repair.
The reviewer independently also reproduced the production 64 MiB limit.

The composed Overview/native-CLI Home pass is 3/3; final provider, read-contract,
CLI compiler, paged and anonymous public replay pass 5/5. Strict replay now follows
the production order without changing recorded response bytes. Old observer
assertions were updated for that order while retaining identity/conflict/refusal
checks. The initial byte probe's invalid page/aggregate combination was corrected;
production bounds were unchanged. Relevant unchanged scale/layout evidence is
reused. The narrow independent correction confirmation remains with the coordinator;
this correction does not claim whole-pack acceptance or new native qualification.
