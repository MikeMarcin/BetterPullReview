# Source, update history, and Review entry evidence

Run `rfc0037-run-20260918`, dispatch `rfc0037-dispatch-001`, producer
`/root/rfc0037_m1`, mandate revision `1`.

The accepted source candidate is
`5b95485c9432cd365e8cedffc3aee0ea7d52b164`.

## Usable behavior

- Overview emits a typed Start/Continue entry. Start selects the projected
  file and enters Review. Continue selects the projected file and follows the
  retained Continue preference. File rows and thread paths still enter Read.
- Neither Start nor Continue dispatches a review mutation. The composed seed
  course exercises natural Start before any receipt exists and natural
  Continue after the preference and receipt have been accepted; both preserve
  the recorded mutation counters.
- A selected file with no exact comparison context reports an actionable
  comparison-unavailable failure. A completion whose authority, revision, or
  selection was replaced still reports the distinct stale failure.
- The composed source course launches the application with a discovered,
  explicitly confirmed `github_cli` profile on the recorded GHES 3.18
  contract. One course refuses the exact smart-Git acquisition and observes no
  source, no native credential/bootstrap path, and no false stale wording. The
  supported course uses the same durable application root across single and
  reuse launches, retains two actual epochs, and renders the intended base and
  updated first lines for `review.cpp` and `secondary.cpp` from the recorded
  packs.

## Investigation boundary

The refusal course establishes that a missing exact comparison and failed
history preparation can share the smart-Git acquisition boundary in the
current composed graph. It does not establish that the owner encountered that
boundary: the exact 0.1.3 build, pull request, GitHub CLI version, Git version,
and provider response were not captured. The retained-history publication and
selected-diff replay paths already exist in the registration baseline, so no
live-cause claim is made.

The CLI evidence is profile-typed recorded composition, not a literal `gh`
subprocess or a live GHES request. Recovery is demonstrated by healthy
relaunches over the same durable store; the refusal course verifies that the
Refresh action remains available but does not claim successful in-process
Refresh recovery. File rendering checks exact file identity and each pane's
first line, not byte-for-byte equality of the whole file.

## Verification

All commands ran from the repository root without a live GitHub/GHES target:

- `application_setup_destination_public_path`: passed in 30.67 seconds.
- `application_review_behaviour_contract`: passed in 87.10 seconds.
- `application_workbench_host`: passed.
- `qt_pull_request_overview_page`: passed.
- `qt_workbench_window_scale_100`: passed.
- `application_epoch_host`: passed.
- `gh_cli_git_smart_transport`: passed.
- `application_workbench_composition_contract`: passed.
- `application_overview_since_review_contract`: passed.
- `application_github_public_path_ghes_3_18`: passed.
- `application_profile_multi_window_public_path`: passed.
- `python3 tests/end_to_end/rfc0015_cli_read_fixture_tests.py`: 3 tests passed.
- `cmake --build --preset dev --target coding-format-check -j 4`: passed.
- `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`.
- `python3 tests/scripts/verify_documentation_tests.py`: 64 tests passed.
- RFC-BPR-0037 board validation: valid, 919 cards, no errors.
- `git diff --check`: passed.

The correction pass from
`/root/rfc0037_m1/rfc_reviewer_critical_source_entry` accepted the candidate
with no blocking or non-blocking findings. The reviewer independently traced
the typed CLI/GHES authority, unavailable and stale paths, natural Start and
Continue gestures, exact two-file display assertions, and quiescent read-only
review state.

The accepted cards closed in authored milestone order: BPR-0922 at
`ae881081ddb8b7d9d8db6a1986863dded3aaf85a`, BPR-0921 at
`e34649d676eddb5bbeb34799e7dd91dc07f31099`, and BPR-0919 at
`34760e1381573f4c0db2f1a70090d17143b704ae`.
