---
id: BPR-0305
title: Compose the production anonymous public preview
status: Done
rfc: RFC-BPR-0017
depends_on: [BPR-0303, BPR-0304, BPR-0317]
---

# Compose the production anonymous public preview

## Required final state

- The tests-disabled application composes the existing provider/normalizer and
  Git/cache/epoch/diff paths behind explicit platform selection, while
  production macOS, Linux, and Windows remain typed public-preview unavailable
  with zero requests in this candidate slice.
- BPR-0316 owns same-slice macOS enablement and selected-artifact rerun after
  BPR-0306 passes. Linux and Windows remain unavailable with zero
  requests; enabling either requires a separately allocated target-native card
  and an update to BPR-0309 before closeout.
- GHES, mutation, GraphQL, viewer, vault, credential, and private-intent paths
  refuse before request construction; counters prove zero effects.
- The composed course opens a recorded public fixture, renders flat/grouped
  comments without resolution, selects a file and renders its exact diff
  content, then switches to authenticated mode with exact invalidation and no
  stale identity-sensitive projection.
- The designated public-observer subprocess and all fixture helpers remain
  outside production selection and testing symbols are absent from Release.

## Lane — 2026-09-02

[[BPR-0303]], [[BPR-0304]], and [[BPR-0317]] are terminal; claimed straight
into `30-in-progress/` by the integrator, its worker started in a worktree
on the head that carries all three, under the transition exception. What
they leave at this card's door, all of it composed to `nullptr` today: the
host's `aAnonymousPullRequests` (a provider constructed with
`github_provider_read_authority::anonymous_public`), `aAnonymousSmartGit`
(a factory over `make_native_https_anonymous_smart_transport`), and the
`public_preview_projection_port` the window implements — plus
`set_public_preview_now`, which nothing binds yet, so without composition
the ring's reset text measures from zero. The ledger's selection table says
the preview is unavailable on every platform through [[BPR-0306]] and
enabled on macOS only by [[BPR-0316]], so this card composes every port and
keeps the shipped selection unavailable, proving the typed refusal with zero
requests and, under the tests-enabled fixture, the enabled course.

## Part one landed — 2026-09-02

Integrated at `913919e8` with the card open; the enabled fixture course and
the `public_path` selector follow as part two.

**The selection is a constant, and it is unavailable everywhere.**
`shipped_public_preview_capability()` sits beside RFC-BPR-0016's device
constant and answers `Available = false` on every platform, as the ledger's
selection table requires through [[BPR-0306]]; [[BPR-0316]] flips macOS and
[[BPR-0314]]/[[BPR-0315]] prove Linux and Windows stay unavailable. The
host receives the anonymous ports only when the selection or a
tests-enabled course allows, so with the shipped selection
`activate_public_preview` is refused `public_preview_unavailable` with zero
requests of any kind. The worker's negative mutation flipped the `__APPLE__`
branch to `true`: both shipped-selection selectors failed on
`PUBLIC_PREVIEW_COMPOSED:true`.

**Every port is composed, under the device host's split.** A second
provider, `make_github_provider(…, anonymous_public)`, whose factory's
`make_anonymous` builds `make_native_https_anonymous_read_transport` in
production and the `_for_test` twin under the fixture; an
`anonymous_git_smart_transport_factory` over
`make_native_https_anonymous_smart_transport`; and the public-preview
projection fanned out through `composition_public_preview_projection` to a
recording self-test sink and the window, with `set_public_preview_now` bound
to the composition's clock and the window's sign-in gesture dispatched to
the host. The anonymous provider is wrapped in
`counting_anonymous_pull_requests`, constructed in both profiles and never
handed to the host under the shipped selection — deliberately, so the
shipped `PUBLIC_PREVIEW_REQUESTS:0` means the gate was not bypassed rather
than that nothing exists. The composed-host audit now anchors the two
shipped credential-free factories in both profiles (required 30).
`make_designated_review_public_observer` keeps its one attended call site;
nothing here reuses it.

**Probe and markers.** `--public-preview-probe` is unconditional like the
device probe; `--public-preview-enabled-course` and
`--public-preview-ghes-course` exist only under `BPR_BUILD_TESTING`. The
course prints `PUBLIC_PREVIEW_COMPOSED`, `_STAGE`, `_REQUESTS`,
`_HEADER_EFFECTS`, `_VAULT_EFFECTS`, `_SIGN_IN_REQUESTS`,
`_REVIEW_COMMENT_GROUPS`, and one of `PUBLIC_PREVIEW_UNAVAILABLE_OK`,
`_REFUSED_OK`, `_ENABLED_OK` (or their `_FAILED` twins). The shipped probe on
the integrator host prints `COMPOSED:false`, `STAGE:unavailable`,
`REQUESTS:0`, `HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`,
`PUBLIC_PREVIEW_UNAVAILABLE_OK`; the GHES course prints `COMPOSED:true`,
`STAGE:refused`, `REQUESTS:0`, `PUBLIC_PREVIEW_REFUSED_OK` — the authority
composed and the core policy still refusing before a request exists. The
header and vault effect counts are fixture-derived, so on the shipped rows
they are a weaker claim than `COMPOSED:false` with `REQUESTS:0`.

**Three selectors**, mirroring the device triple:
`application_anonymous_public_preview_unavailable`, `_release_contract`, and
`_ghes_refusal`, all `WINDOWS_UNCONTAINED` and bounded, pinned in the
network-isolation manifest and allocated in the inventory with their probe
flags as required arguments. Both digest pins moved in two passes after
formatting: `bpr_expected_full_application_digest`
`590dc82d…bd20e8` → `dc448811…3d56fc`,
`bpr_expected_production_app_manifest_digest` `63667581…67e8ee` →
`9638be22…a5593`; no file was added, so `bpr_expected_app_sources` is
unchanged.

**Not yet: the enabled course.** Under `--public-preview-enabled-course
--github-fixture github-com` the authority composes and three anonymous
reads reach the fixture (`COMPOSED:true`, `REQUESTS:3`, `HEADER_EFFECTS:0`,
`VAULT_EFFECTS:0`), then the pull request fails to load and the run ends
`PUBLIC_PREVIEW_ENABLED_FAILED`: the fixture's recorded interactions are
shaped for the authenticated sequence, and a signed-out session issues a
shorter one with the REST review-comment operation the authenticated
manifest never records. Part two adds that recording and registers
`application_anonymous_public_preview_public_path`; until then the
`anonymous_public` → `authenticated` mutation is not observable by any
registered selector.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | `913919e8` + this commit |
| The three new selectors, the device triple, both anonymous host suites, one Qt scale, the composed-host audit with the new anchors, the manifest and inventory gates, the public-path fixtures, and both composition contracts | `ctest --preset dev -R '^(application_anonymous_public_preview_(unavailable\|release_contract\|ghes_refusal)\|application_device_authorization_(public_path\|unavailable\|release_contract)\|application_anonymous_read_host\|application_anonymous_diff_authority\|qt_anonymous_public_preview_scale_100\|composed_application_hosts\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_release_absence_negative_control\|application_github_public_path_(github_com\|uiforetw_74\|ghes_3_18)\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 19/19 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The shipped image carries both anonymous factories and no test twin, and answers the probe unavailable with zero requests | `cmake --build --preset app-release`; `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all --release-binary <artifact>`; `nm -C`; `<artifact> --self-test --ghes-host ghe.example.invalid --public-preview-probe --automated-run-budget-ms 20000` | `RFC0017_TEST_INVENTORY_OK`; read and smart factories present, `_for_test` absent; `PUBLIC_PREVIEW_COMPOSED:false`, `STAGE:unavailable`, `REQUESTS:0`, `PUBLIC_PREVIEW_UNAVAILABLE_OK` | this commit |
| The composed-host audit's, the manifest gate's, and the inventory's own contracts | `python3 tests/scripts/verify_composed_hosts_tests.py`; `…/verify_test_network_isolation_tests.py`; `…/verify_rfc0017_test_inventory_tests.py` | OK; OK; OK | this commit |

## Part two landed — 2026-09-02

Integrated at `f888e868`. With it the card's required final state is met and
it closes.

**The anonymous script is derived, not recorded twice.** The public-path
fixture builds a nine-exchange signed-out script from the authenticated
ten in C++ — no fixture file added, the authenticated `manifest.json` byte
for byte unchanged (its security suite 16/16). The order is
`anonymous_read_operations`: repository, pull request, files, commits,
reviews, review comments, issue comments, checks, search; eight bodies are
the authenticated recording's REST responses and the ninth,
`ListReviewComments`, is the promoted live body from
`tests/fixtures/github/rfc0017-anonymous-v001/captures/pull-request-comments.json`
byte-exact, its request derived from the recorded reviews exchange the way
`compile_read_request` emits it. Viewer identity and review threads are
absent. The anonymous role refuses any `Authorization` or
`Proxy-Authorization` header, and an exact-count check mirrors the
authenticated one.

**The real blocker was the adapter's guards, not the recording.**
`valid_read_correlation` demanded structurally valid capability evidence
and `provider_request_valid` demanded a valid observation with a nonzero
viewer account, so the host's synthesised, never-probed entry could not
pass and `make_anonymous` was never reached. `signed_out_read_identity` in
`github_anonymous_read_admission.hpp` now admits exactly one shape at both
call sites: mode `public_preview`, an anonymous viewer, zero account
everywhere, `anonymous_capabilities()`, a shape reading nothing, and both
freshness epochs zero — a probe always sets them, so observed evidence
cannot enter through this branch, which is the load-bearing term the
integrator reviewed. Probe-derived evidence still passes unchanged. A
second fix in `main.cpp`: the stage is reported from the observation the
run made while the session was live, because `access_mode()` read after
shutdown had already been cleared.

**The enabled course.** Under `--public-preview-enabled-course
--github-fixture github-com` the pull request opens through the anonymous
provider: `COMPOSED:true`, `STAGE:enabled`, `REQUESTS:1` (anonymous
pull-request reads, not wire exchanges — part one's semantics),
`HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`, `SIGN_IN_REQUESTS:0`,
`REVIEW_COMMENT_GROUPS:1`, and two write attempts offered through the
host's legality query and refused before any request —
`WRITE_REFUSALS:2` — then `PUBLIC_PREVIEW_ENABLED_OK`.
`application_anonymous_public_preview_public_path` pins that whole marker
sequence, registered like its siblings, pinned in the manifest, and
allocated in the inventory with its course flag; the platform course now
takes it from the inventory rather than as a course-only name. Pins moved
in two passes: `dc448811…3d56fc` → `ed8f960a…10578c`,
`9638be22…a5593` → `085e3611…f8b87`.

**One test caught up with the rule.** `github_paged_read`'s signed-out
request still started from the authenticated one — a real viewer and
probe-derived evidence with its capability facts flipped — which the pager
now rightly refuses to treat as signed out, so the integrator's first
reproduction failed it on "a signed-out read normalizes". The test now
builds the identity `signed_out_read_identity` admits, term by term, and
asserts it before reading; the suite and every adapter sibling pass.

**Recorded for [[BPR-0316]].** The probe's `PUBLIC_PREVIEW_ENABLED_OK`
verdict requires at least one read, so a flipped shipped selection — which
activates with zero requests — would take the `UNAVAILABLE_FAILED` branch
under the plain probe; the macOS flip owes a probe-verdict or course change.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The four composition selectors, the three public-path fixtures, the device public path, both anonymous host suites, the adapter's anonymous contract and pager and provider suites, the read contract, the transport tables, the composed-host audit, the manifest, inventory, and course-contract gates, and both composition contracts | `ctest --preset dev -R '^(application_anonymous_public_preview_(public_path\|unavailable\|release_contract\|ghes_refusal)\|application_github_public_path_(github_com\|uiforetw_74\|ghes_3_18)\|application_device_authorization_public_path\|application_anonymous_read_host\|application_anonymous_diff_authority\|github_anonymous_read_contract\|github_paged_read\|github_provider\|github_read_contract\|native_https_anonymous_read_(contract\|fixture)\|composed_application_hosts\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_platform_course_launch_contract\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 23/23 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The shipped image still answers the probe unavailable with zero requests and carries no test seam | `cmake --build --preset app-release`; the inventory's `--all --release-binary` scan; the shipped probe with `--ghes-host` | `RFC0017_TEST_INVENTORY_OK`; `STAGE:unavailable`, `REQUESTS:0`, `PUBLIC_PREVIEW_UNAVAILABLE_OK` | this commit |
| Composing the authenticated authority instead is caught | The worker composed the anonymous provider with `github_provider_read_authority::authenticated`, rebuilt, ran the public path; restored, repinned | `REQUESTS:3`, `GROUPS:0`, `WRITE_REFUSALS:0`, `PUBLIC_PREVIEW_ENABLED_FAILED`; restored, passes with no pin drift | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
