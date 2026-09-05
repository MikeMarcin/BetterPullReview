# BetterPullReview Project Rules

External consumer context. `project.json` holds the
machine-read facts (paths, lanes, gate commands); this file holds the rules a
session needs in words.

## Repository identity

BetterPullReview: a native C++/Qt desktop application for reviewing GitHub
and GitHub Enterprise pull requests. Origin `MikeMarcin/BetterPullReview`.
Layering rule: the core decides, the host performs, Qt presents.

## Read first

Root `AGENTS.md` and its read-first list (`README.md`, `CONTRIBUTING.md`,
`_coding-standard/STANDARD.md`, `docs/architecture/product-contract.md`,
`docs/architecture/system-architecture.md`), the current RFC, the live board
and card, and the nearest nested `AGENTS.md`. `docs/issues/README.md` before
editing anything under `src/`.

## Side-effect boundary

Never expose or commit GitHub credentials, GHE tokens, cookies, private
repository content, or secret-bearing logs. Use fixtures, recordings, or a
designated test repository for GitHub mutations: no reviews, comments,
approvals, labels, pushes, or branch updates on a real pull request unless the
RFC requires live validation and the user has already named the safe target.
GitHub Actions is not an evidence surface.

## Direct surfaces

The public path is the application itself, the provider facade, the diff
pipeline, the persisted state path, or the installed package. A preset named
`ghe` is not proof that the GHE implementation was selected; inspect the
selected configuration. Diff claims use representative fixtures and verify
displayed line identity against expected data. Visual or interaction claims
need a rendered application state plus deterministic automation or a
documented manual check, not a mockup or a view-model unit test. Selector
traps recorded on the board: pick test selectors by composition, not
subsystem, and changes to `LOOPBACK_PORT_COUNT` must run
`test_network_isolation_manifest`.

## Workflow and closeout

Use canonical cards, shared allocation and claim/finish. The product documentation gate retains per-criterion closeout checks, RFC baseline and canonical index order. Ordinary slices use focused checks. At pack or milestone completion normally run one integration pass; full relevant suites and critical review are appropriate when warranted. No default exhaustive matrix, hermetic replay or manual approval.

## Attendance and platform eligibility

Use shared attended-review when an explicit human criterion remains; park with human-review and continue independent work. Prepare ready items incrementally, preserve partial verdicts and relevant protections, and use wizard for manual steps. Prefer local HTML and agent-captured objective evidence; retain offline copy/paste. Platform requirements live in platforms metadata; query the applicable platform, and leave unmet dependencies in Backlog. Existing RFC sequencing exceptions do not waive required real-platform evidence.
