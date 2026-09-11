---
schema: "implement-rfc.card.v1"
id: "BPR-0715"
title: "Opening a pull request signed out no longer reaches a comparison"
status: "Parked"
rfcs: ["RFC-BPR-0024","RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"first_bad_commit":"826fb871","last_good_commit":"9a175e81","attended_decision":"bdd9244a-754a-4a7a-94b4-79c7110687d9"}
parked_on: "human-review"
acceptance: [{"criterion":"application_anonymous_public_preview_public_path passes at the fixed head, and its mutation (dropping the fixture smart-Git binding) fails it","outcome":"pass","revision":"63d4e37e","evidence":{"kind":"observation","detail":"ctest -R '^application_anonymous_public_preview_public_path$' in build/dev passes in about 10 s with PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:1, PUBLIC_PREVIEW_EPOCH_STATUS:6 (ready), PUBLIC_PREVIEW_SMART_ACQUISITIONS:1 and PUBLIC_PREVIEW_ENABLED_OK; with the fixture binding replaced by nullptr the same selector fails with EPOCH_EFFECT_FAILED:revision-prepare:0 and PUBLIC_PREVIEW_EPOCH_STATUS:8 (scratch logs d4-anon-fix4.log, d4-anon-mutation.log)."}},{"criterion":"The tests-disabled bundle at the fixed head, started signed out with the owner's data root layout, opens https://github.com/microsoft/terminal/pull/20607 to files and an epoch line; the diagnostics log shows the acquire stages or an observation-only prepare followed by a resolved comparison","outcome":"pending","revision":"63d4e37e","evidence":{"kind":"observation","detail":"Not yet observed in the signed bundle. The owner's data root (window-restoration record: window 2, public preview, microsoft/terminal#20607) driven by the fixed tests-enabled dev build traced the whole path: restoration read accepted, workbench authority valid with account=0 viewer=0, observation finished with 4 actionable files (exact), observation-only prepare, comparison resolved with both anchors verified in the cache and a diffable pair, then the explicit left/right pair (scratch log d4-wt-stderr-real2.log, 18:36Z). The tests-disabled bundle (executable sha256 9334b13a...) launched four times on the same root (open -n and the binary directly, 40-72 s each) restored nothing and made no request: its diagnostics log shows only 'application started', the same signature as the owner's own v002 run at 16:37Z on 484b2072, whereas d2's bundle runs at 17:07-17:20Z reached the epoch stages. Anonymous read refusals are not logged; GitHub's anonymous limit was exhausted (60/60) during this session at 18:19Z and had reset by 18:33Z. The screen is locked, so no window capture was possible. The owner's v003 run, which opens the pull request by hand, is the observation this criterion waits on."}},{"criterion":"qt_anonymous_public_preview_scale_100 passes or is carded separately with its own cause named","outcome":"pass","revision":"63d4e37e","evidence":{"kind":"observation","detail":"Stale reflow assertion after the BPR-0646 Settings adoption, carded and repaired as BPR-0717 (done, 63d4e37e): qt_anonymous_public_preview_scale_{100,125,200} pass and the reverted resize fails the compact assertion again."}},{"criterion":"The composed selectors (qt_composed_github_scale_*, application_composition_contract, application_review_round_trip_composition_contract, application_profile_multi_window_public_path), check_boundaries, strict tidy on touched translation units, clang-format and the documentation gate pass","outcome":"pass","revision":"63d4e37e","evidence":{"kind":"observation","detail":"Integration pass in build/dev: qt_composed_github_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, qt_settings_window, qt_anonymous_public_preview_scale_{100,125,200}, application_anonymous_diff_authority, application_epoch_host, application_workbench_host (with the new signed-out validity test), application_workbench_snapshot_observation, application_github_window_read_host, application_profile_multi_window_public_path (79 s), application_github_public_path_github_com, application_setup_destination_public_path, application_anonymous_public_preview_public_path, application_review_round_trip_composition_contract and application_composition_contract (148 s, both after repinning the two github_application.cpp digests) all pass; check_boundaries, tidy.py --warnings-as-errors on the five touched translation units, clang-format --dry-run --Werror on every touched file, git diff --check and verify_documentation.py pass. application_workbench_composition_contract is red at fd7388e8 before and after the fix with the same signature and is carded as BPR-0718."}}]
---

# Opening a pull request signed out no longer reaches a comparison

The owner, 2026-09-10, on the pack-head bundle (executable sha256
`484b2072…`, attended session v002, decision `bdd9244a`): "Can't open any
files. Saw no epoch line." That verdict rejected
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md),
but the chrome is not what broke.

## What is established

- `application_anonymous_public_preview_public_path` and
  `qt_anonymous_public_preview_scale_100` are red at `97c8f9f1`. Neither is
  in the integration pass the pack head was accepted on, which is why a green
  pass predicted nothing about this run.
- The application course was green at `9a175e81` (2026-09-07). `git bisect
  run` over the 171 commits since, building `app_better_pull_review` in a
  scratch worktree and running that one selector, names `826fb871` "Finish
  BPR-0705" (the live profile window composition,
  [BPR-0705](../60-done/BPR-0705-live-profile-window-composition.md); 3,585
  insertions across `github_application.cpp`, `profile_desktop.cpp`, the
  GitHub provider and the native HTTPS transport) as the first bad commit.
  Every intermediate step is in the bisect log; the good endpoint was run
  before the bisect started.
- In the course the failure is `EPOCH_EFFECT_FAILED:revision-prepare:0`
  (`epoch_failure::unavailable`) raised from the `protocol_acquire` stage of
  `epoch_application_host.cpp` (the `if (!acquisition)` branch after
  `acquire_git_protocol_v2_pack`), and the transport error is
  `git_smart_transport_failure::network`. Under the network-isolated course
  that means the signed-out smart-Git lease dialled a real transport instead
  of the fixture's; the course then sits at "Pull request ready." with
  `PUBLIC_PATH_CONVERSATIONS ... file=0` until its budget expires.
- The signed bundle reproduces the owner's run without any interaction: it
  restores the previous public-preview review of microsoft/terminal#20607,
  the diagnostics log shows `catalog_open`, `catalog_recovery` and
  `catalog_prepare` succeed and then no `cache_open`, `smart_acquire` or
  `protocol_acquire` ever starts, and the window reads "This pull request is
  open. Its comparison is not ready; Refresh to try again. The workbench
  could not be updated. Review history is ready." with no files and no
  epoch line. A run of the same bundle on 2026-09-07 20:56 went
  `catalog_prepare` → `cache_open` → `pack_sink` → `smart_acquire` →
  `protocol_acquire` → `cache_inventory` → `revision_prepare` and showed the
  comparison.
- In the bundle the objects for that head are already cached, so the
  revision-prepare worker takes the observation-only branch
  (`epoch_preparation_effect::none`); the stage that then fails to produce a
  comparison was not captured. The failing event in the bundle is therefore
  not yet named; the course's transport failure is the only named one.

## Not established

- Whether the bundle's failure is the same missing smart-Git binding (now on
  the comparison-resolve path) or a second break in the same commit. The
  worktree dev build could not be driven against the owner's data root
  (it shows no window after an earlier instance was killed with SIGKILL),
  so the instrumented run that would name the event did not happen.
- Whether `qt_anonymous_public_preview_scale_100` ("compact connection
  buttons use a separate row below the profile", a 420-pixel reflow claim on
  the session panel now mounted on Settings) is the same regression or a
  test left stale by [BPR-0670](../60-done/BPR-0670-public-preview-is-buried-and-does-not-say-what-it-is.md).
  It is recorded here so it is not lost; it may deserve its own card.

## Scope

- Restore the signed-out open: Home → Read without signing in → paste a
  public pull request → files, epoch line and an exact comparison, over the
  live profile window composition, without weakening the anonymous
  transport's refusals (an authenticated authority on the credential-free
  lease stays refused; a public-preview authority on the account lease stays
  refused).
- Owned paths: `src/app/src/github_application.cpp`,
  `src/app/src/profile_desktop.cpp`, the anonymous smart-Git factory and its
  fixture counterpart in `src/app/src/github_public_path_fixture.cpp`, and
  whatever in `src/application/src/epoch_application_host.cpp` the diagnosis
  reaches. Add the two red selectors to the pack's integration pass.
- Candidate protection over `src/adapters/qt` and `src/app` is still active
  for attended session `cbd48b91…`; run `resume-development` for the
  affected candidates before the first edit, and expect every candidate to
  need preparing again at the fixed head.

## Acceptance

- `application_anonymous_public_preview_public_path` passes at the fixed
  head, and its mutation (dropping the fixture smart-Git binding) fails it.
- The tests-disabled bundle at the fixed head, started signed out with the
  owner's data root layout, opens
  `https://github.com/microsoft/terminal/pull/20607` to files and an epoch
  line; the diagnostics log shows the acquire stages or an observation-only
  prepare followed by a resolved comparison.
- `qt_anonymous_public_preview_scale_100` passes or is carded separately
  with its own cause named.
- The composed selectors (`qt_composed_github_scale_*`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract`,
  `application_profile_multi_window_public_path`), `check_boundaries`, strict
  tidy on touched translation units, clang-format and the documentation gate
  pass.

## Diagnosis, 2026-09-10, dispatch `ux-redesign-0028-pack.d4`

Two defects, both reached by the signed-out open, neither the fixture's
smart-Git binding as such:

- **The bundle's failing event is `workbench_application_host::observe_now`
  refusing the signed-out authority.** A HEAD dev build driven against the
  owner's data root (restoring microsoft/terminal#20607 signed out, no
  interaction) traced: observation-only revision prepare, comparison
  resolution with both anchors verified in the cache
  (`c7572cde…`, `472eb71d…`), `comparison_resolved` with an exact,
  diffable pair — and, before any of it, `WorkbenchHost->observe` refused
  with `host_failure` because `valid(workbench_pull_request_authority)`
  required a non-zero account and viewer, which a public-preview handoff
  never carries (`account=0 viewer=0`, every other field non-zero). No file
  rows are published, so the window keeps "Its comparison is not ready …
  The workbench could not be updated" and nothing to select. The epoch host
  itself was healthy; its resolution path logs no stage, which is why the
  diagnostics log stopped at `catalog_prepare`.
- **The course's red is the host projection, not the epoch.** At the last
  good commit the course passed in 0.7 s: the review-comment observation
  came from the host's own read window and the run shut down before the
  epoch ran (`EPOCH_CATALOG_FAILURE:17`, cancelled). Since `826fb871` the
  primary window is an explicitly attached `github_window_read_host`, and
  `github_application_host` never adopted its accepted snapshot, so the
  public-preview presentation never reported the review comments
  (`PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:0`) and the run waited out its
  budget — long enough for the epoch to reach `protocol_acquire` over the
  production anonymous factory and fail with `network`, the symptom the
  card was opened on. The fixture never served a signed-out smart-Git lease;
  no green run of this course had ever reached a comparison.

## Fix

- `application::valid(workbench_pull_request_authority)` admits the
  credential-free authority: account and viewer both zero
  (`application::anonymous`), never one without the other.
- `github_application_host::observe_attached_read` adopts an attached
  window's accepted snapshot and rate limits and republishes the
  public-preview presentation; the profile runtime calls it after each
  primary-window read update.
- The fixture's anonymous transport now serves the signed-out smart-Git
  lease (public-preview authority, no account), the composition binds it
  under a fixture, and the course requires an exact comparison
  (`PUBLIC_PREVIEW_EPOCH_STATUS:6`, one smart acquisition) before
  `PUBLIC_PREVIEW_ENABLED_OK`.

## Verification record

2026-09-10, dispatch `ux-redesign-0028-pack.d2`: reproduction and bisect
only, no product source changed on main. Selector runs at `97c8f9f1` in
`build/dev`: `application_anonymous_public_preview_public_path` failed
(40.37 s, budget expired at stage 1), `qt_anonymous_public_preview_scale_100`
failed; `application_setup_destination_public_path`,
`application_profile_multi_window_public_path` and
`application_github_public_path_github_com` passed. Bisect: good `9a175e81`,
`f00ecb7d`, `91a75ec6`, `4ae98c1c`, `061dfcdf`; bad `58fb1d41`, `02b35b74`,
`88c42087`, `826fb871`.

2026-09-10, dispatch `ux-redesign-0028-pack.d4`, see the diagnosis above.
`application_anonymous_public_preview_public_path` passes in about 10 s at
the fixed head; with the fixture smart-Git binding replaced by `nullptr` it
fails with `EPOCH_EFFECT_FAILED:revision-prepare:0` and
`PUBLIC_PREVIEW_EPOCH_STATUS:8`. `qt_anonymous_public_preview_scale_100` was
a stale reflow assertion, carded and repaired as
[BPR-0717](../60-done/BPR-0717-the-public-preview-reflow-assertion-is-stale-after-the-settings-adoption.md).
`application_workbench_composition_contract` is red at `fd7388e8` with the
same signature before and after this fix (`WORKBENCH_INTENT_WIRE_FAILED`
onward) and belongs to no card here.

## Observation from the v003 session, 2026-09-10

The owner's screenshots on the
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
rejection (decision `65f3f881`, bundle `9334b13a…`) show the signed-out
bundle with microsoft/terminal#20607 open: four files in the drawer, three
of them rendering ("Diff ready — 6 semantic changes (cached)"), the strip
and the epoch line in place. That is the files and the epoch line this
card's pending criterion names. The log half of the criterion is not met
as written: the review was restored at launch and the diagnostics log
(05:19:48Z to 05:34:45Z) shows the catalog stages and cached comparisons
only, no acquire stage and no observation-only prepare; the restoration
path does not go through the acquire. The criterion stays pending until a
fresh open through Home on the owner's root shows the acquire stages, or the
criterion is narrowed to what restoration can show. The refused fourth file
is [BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
not this card.
