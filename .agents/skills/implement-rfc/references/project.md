# BetterPullReview Project Rules

The only project-specific prose in this skill. `config/project.json` holds the
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

## Closeout-card contract

`scripts/verify_documentation.py --root .` (the `structure_gate`) checks the
closeout card `BPR-NNNN-close-rfc-NNNN.md` titled `Verify and close
RFC-BPR-NNNN`: its `rfc:` field agrees with the filename, an `RFC base:` line
carries the 40-hex `RFC_BASE_SHA`, and a `## Closeout checklist` table has one
row per canonical acceptance criterion with the check run, its result, and
the commit. Nothing else is validated. The full `dev`, `core-only`, and
`coding-tidy` runs take most of an hour; run them only on explicit request or
when a change plausibly reaches beyond its slice.

## Attended checkpoints

Subjective visual, audio, or interaction acceptance, credential or release
authority, and remote-mutation decisions use the `bpr-attended-review` skill:
a markdown operator packet for one bounded decision, or its offline review
page for many. Park the card in `40-parked` with a `parked_on` token and
continue; present pending packets together at the run boundary.

## Platform lanes

`21-ready-win`, `22-ready-mac`, and `23-ready-lin` hold cards that must run
on that platform (native Windows, native macOS, WSL2 or native Linux). A
station reads only its own platform lane plus cards dispatched to it; a card
ready only elsewhere is recorded as deferred, never skipped silently.
