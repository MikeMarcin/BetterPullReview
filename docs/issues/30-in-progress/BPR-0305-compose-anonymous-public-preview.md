---
id: BPR-0305
title: Compose the production anonymous public preview
status: In Progress
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
