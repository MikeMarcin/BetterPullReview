---
schema: "implement-rfc.card.v1"
id: "BPR-0314"
title: "Verify anonymous preview remains unavailable on Linux"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0306"]
---

# Verify anonymous preview remains unavailable on Linux

## Required final state

- A clean supported Linux target builds the tests-disabled Release application
  and selects typed public-preview unavailability for GitHub.com and GHES.
- Exact counters prove zero vault/keyring, Authorization header, REST/GraphQL,
  Git smart HTTP, browser, mutation, child-process, and private-content effects.
- Runtime/dependency/ELF, Qt presentation, fixture-seam absence, documentation,
  and hygiene gates pass target-native. No enablement claim is made.
- This card's first target-native course is provenance for the unavailable
  selection; BPR-0309 reruns the same idempotent course at immutable final head
  after cross-platform simplification and documentation reconciliation.

## Partial target-native evidence — 2026-08-22

Three of this card's rows are now produced on target-native Linux. They are
recorded as advance evidence; the card stays open, and none of this is an
enablement claim.

None of it was obtainable before today. Every row needs the tests-disabled
Release binary, which did not build on Linux at all until [[BPR-0421]] and
[[BPR-0440]] landed — [[BPR-0156]] recorded that preset as "does not compile".

Host: Ubuntu 26.04 x86_64, GCC 15.2.0, `main` after the Windows-lane merge.
Binary: `build/app-release/src/app/better_pull_review`, 10,535,800 bytes.

| Row | Command | Result |
|---|---|---|
| clean Linux target builds the tests-disabled Release application | `cmake --build --preset app-release -- -k 0` | exit 0, links |
| fixture-seam absence | `scripts/verify_rfc0017_test_inventory.py --build-root build/dev --release-binary …` | `RFC0017_TEST_INVENTORY_OK` |
| dependency / ELF | `scripts/verify_binary_dependencies.py …` | `BINARY_DEPENDENCIES_OK` |
| runtime | `scripts/verify_runtime_surface.py …` | `RUNTIME_SURFACE_OK` |

The fixture-seam row is the one worth noting: that script exists precisely to
prove the RFC-BPR-0017 evidence selectors are registered once in a tests-enabled
build and **absent** from a tests-disabled Release binary. It has presumably
never run against a Linux Release binary before, because there was none.

### Still outstanding, and why this card cannot close

- Typed public-preview unavailability selected for GitHub.com and GHES, and the
  exact zero-effect counters for vault/keyring, Authorization header,
  REST/GraphQL, Git smart HTTP, browser, mutation, child-process and
  private-content. Both need the RFC-BPR-0017 code that does not exist yet.
- Qt presentation and the hygiene matrix.
- [[BPR-0306]], this card's declared dependency, is in `10-backlog/` behind a
  chain rooting at [[BPR-0298]] (`awaiting-live-github`) and at [[BPR-0035]] →
  [[BPR-0036]] (target-native Windows).
- `docs/rfcs/RFC-BPR-0017-unauthenticated-public-review-read.md` gates
  implementation on "RFC-BPR-0002 Auto-Verified" and carries no early-
  implementation carve-out, unlike RFC-BPR-0016. Starting the missing work here
  would breach that gate.

## Target-native Linux course and close — 2026-09-03

[[BPR-0306]] is terminal, so this card's one remaining item — the Linux
unavailable-path course, target-native — has now run. The shipped selection is
**unchanged**: Linux answers `public_preview_unavailable` and this card makes
no enablement claim. Enabling any platform is [[BPR-0316]]'s, and only macOS.

Host: Ubuntu 26.04 LTS, `Linux 6.18.33.2-microsoft-standard-WSL2 x86_64`.
Toolchain: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), CMake 4.4.2, Ninja 1.13.2,
Qt 6.10.3, Python 3.13.15. Merged head `91873924`, which carries [[BPR-0305]]
and [[BPR-0306]], this card's dependency chain.

Precondition, read first and before anything else: the Secret Service `login`
collection's `Locked` property over D-Bus read `(<false>,)` at
2026-09-03T05:44:35Z. It is recorded because a locked collection makes the
vault-touching selectors skip rather than fail, and a course that skipped them
would report the same green as one that ran them. It was unlocked, so they ran.

**The course.** From a build root deleted whole rather than trusted to
`--fresh`:

```
rm -rf build/rfc0017-linux-enablement
python3 scripts/verify_rfc0017_platform_course.py --platform linux \
    --dev-preset dev --release-preset app-release \
    --build-root build/rfc0017-linux-enablement \
    --expected-preview-selection unavailable --fresh
```

Exit 0. Every marker it printed:

```
RFC0017_PLATFORM:linux
RFC0017_SELECTION:unavailable
RFC0017_COURSE_SELECTORS:17
RFC0017_PREVIEW_STAGE:unavailable
RFC0017_PREVIEW_REQUESTS:0
RFC0017_RELEASE_ARTIFACT:build/rfc0017-linux-enablement/release/src/app/better_pull_review
RFC0017_ELAPSED_SECONDS:222
RFC0017_PLATFORM_COURSE_OK
```

The artifact it built and then interrogated:

```
build/rfc0017-linux-enablement/release/src/app/better_pull_review
size    11,533,488 bytes
sha256  bd9481396fb215b6c75dd0bb34a40cd9c23017ca6127ea917c5b40b9ecab257f
ELF     64-bit LSB pie executable, x86-64, dynamically linked, for GNU/Linux 3.2.0
```

**The seventeen course selectors.** The count matches the macOS course exactly,
which is the answer to the question a per-platform count invites: this graph
owes no fewer selectors than the one BPR-0306 ran, so nothing was audited on
macOS and silently skipped here. Derived from this tree's own `ctest -N`
listing by the inventory's rule, never hand-listed:
`application_anonymous_diff_authority`,
`application_anonymous_public_preview_ghes_refusal`,
`application_anonymous_public_preview_public_path`,
`application_anonymous_public_preview_release_contract`,
`application_anonymous_public_preview_unavailable`,
`application_anonymous_read_host`, `composed_application_hosts`,
`core_anonymous_public_read`, `git_anonymous_public_read`,
`github_anonymous_read_contract`, `native_https_anonymous_read_contract`,
`native_https_anonymous_read_fixture`,
`qt_anonymous_public_preview_scale_(100|125|200)`,
`rfc0017_credential_free_native_https`, `sqlite_anonymous_epoch_authority`.

### The shipped artifact, interrogated directly

The course runs these itself; they are re-run here against the staged artifact
so the card quotes their markers rather than only the course's verdict.

```
python3 scripts/verify_rfc0017_test_inventory.py \
    --build-dir build/rfc0017-linux-enablement/debug \
    --release-binary build/rfc0017-linux-enablement/release/src/app/better_pull_review
RFC0017_TEST_INVENTORY_OK

python3 scripts/verify_composed_hosts.py --profile production \
    build/rfc0017-linux-enablement/release/src/app/better_pull_review
COMPOSED_HOSTS_OK: profile=production required=29 forbidden=7 inspector=nm | c++filt

python3 scripts/verify_binary_dependencies.py <artifact>   BINARY_DEPENDENCIES_OK
python3 scripts/verify_runtime_surface.py <artifact>       RUNTIME_SURFACE_OK
```

The course's dev tree is `<build-root>/debug`, not `<build-root>/dev`; the
directory names are the script's (`debug/` and `release/`), independent of the
`dev` preset that configures the first one.

**The shipped probe**, under `QT_QPA_PLATFORM=offscreen`. On this host the
CTest-declared launch environment adds nothing to the ambient environment — all
45 entries are identical — so the two runs the course distinguishes are the
same run here; that is a property of the Linux inventory, not an omission.

```
build/rfc0017-linux-enablement/release/src/app/better_pull_review \
    --self-test --public-preview-probe --automated-run-budget-ms 20000
```

```
PUBLIC_PREVIEW_COMPOSED:false
PUBLIC_PREVIEW_STAGE:unavailable
PUBLIC_PREVIEW_REQUESTS:0
PUBLIC_PREVIEW_HEADER_EFFECTS:0
PUBLIC_PREVIEW_VAULT_EFFECTS:0
PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0
PUBLIC_PREVIEW_UNAVAILABLE_OK
```

The same command with `--ghes-host ghe.example.invalid` printed the identical
seven lines, exit 0. That is the expected shape under the shipped selection
rather than a probe that ignored the flag: the enterprise hostname is refused
by core policy *after* a preview authority is composed, and this build composes
none, so it never reaches the `refused` branch and answers `unavailable`
first. The course accepts either, and rejects only `enabled`. A build with
Linux enabled would answer `refused` here; that difference is [[BPR-0316]]'s
shape, not this card's.

**`NATIVE_PLATFORM_VAULT_SELECTED:true` beside `VAULT_EFFECTS:0` is not a
contradiction**, and is called out because it reads like one. The first says
the Secret Service vault is the platform vault this artifact composed for
signed-in use; the second says the public-preview path performed zero
operations against it. Selecting a vault and touching one are different facts,
and the zero is the one this RFC claims.

### The nine zero-effect claims, and where each is actually proven

The card asks for exact counters on nine effects. Three are printed counters,
one is a counter folded into the terminal verdict, and five are proven by gates
rather than counters — recorded plainly, because a reader who expects nine
printed zeroes and finds five would otherwise be right to doubt the row.

| Claimed zero effect | Where it is proven | Value |
|---|---|---|
| Credential vault / keyring | `PUBLIC_PREVIEW_VAULT_EFFECTS` | `0` |
| `Authorization` header | `PUBLIC_PREVIEW_HEADER_EFFECTS` | `0` |
| REST and GraphQL requests | `PUBLIC_PREVIEW_REQUESTS`, `PUBLIC_PREVIEW_SIGN_IN_REQUESTS` | `0`, `0` |
| Git smart HTTP | `PublicPreviewSmartAcquisitions == 0`, a conjunct of `PUBLIC_PREVIEW_UNAVAILABLE_OK` | verdict printed |
| Browser / embedded engine | course's `check_platform_surfaces`: RFC-BPR-0016's prohibited symbol and string sets absent from the image | no match |
| Mutation | production-forbidden anchors absent: `make_review_mutation_testing_grant`, `make_review_mutation_one_shot_testing_grant` | `forbidden=7` |
| Child process | production-forbidden anchors absent: `gh_cli_transport`, `compile_gh_cli_process_request`, `make_github_cli_process_runner`, `validate_gh_cli_configuration` | `forbidden=7` |
| Private content | `PUBLIC_PREVIEW_COMPOSED:false` — no preview authority exists to read anything, public or private; and `github_public_path_fixture_components` is absent from the artifact | `false` |
| Fixture / test seam in the shipped image | `verify_rfc0017_test_inventory.py --release-binary` | `RFC0017_TEST_INVENTORY_OK` |

The last four rows are absence-of-code claims about the artifact, not runtime
counts, and that is the stronger form: a counter reads zero for one run, an
absent symbol cannot be reached by any run.

### The ledger's remaining Linux rows

```
ctest --preset dev -R '^(core_anonymous_public_read|github_anonymous_read_contract|
native_https_anonymous_read_(contract|fixture)|git_anonymous_public_read|
sqlite_anonymous_epoch_authority|application_anonymous_read_host|
application_anonymous_diff_authority|qt_anonymous_public_preview_scale_(100|125|200)|
application_anonymous_public_preview_(public_path|unavailable|release_contract|
ghes_refusal)|rfc0017_test_inventory|rfc0017_platform_course_launch_contract|
rfc0017_sanitizer_course_contract|test_network_isolation_manifest)$' --output-on-failure
```

exit 0, 19/19 in 4.45 s, every test labelled `bpr_network_isolated`.

```
rm -rf build/rfc0017-sanitize
python3 scripts/verify_rfc0017_sanitizer_course.py --build-root build/rfc0017-sanitize --fresh
```

exit 0: `RFC0017_SANITIZER_SELECTORS:4`
(`application_anonymous_diff_authority`, `application_anonymous_read_host`,
`core_anonymous_public_read`, `sqlite_anonymous_epoch_authority`),
`RFC0017_SANITIZER_ELAPSED_SECONDS:66`, `RFC0017_SANITIZER_COURSE_OK` — no
AddressSanitizer or UBSan diagnostic.

### What this card does not claim

- No enablement. The Linux production selection is untouched; this course is
  the proof of the *unavailable* path, which is what the RFC's capability table
  says Linux answers.
- Not the immutable-head replay. [[BPR-0309]] reruns this same idempotent
  command at final head under `build/rfc0017-linux-closeout`.
- No network step. The public-preview course sends nothing, and nothing here
  contacted GitHub; `ghe.example.invalid` is refused before a request exists.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The Linux platform course, build root deleted whole, selection unavailable | the course command above | exit 0 in 222 s; the eight markers above | this commit |
| The keyring precondition was unlocked, so no vault selector skipped | `gdbus call … /org/freedesktop/secrets/collection/login … Locked` | `(<false>,)` | this commit |
| The shipped artifact carries no test seam | `verify_rfc0017_test_inventory.py --release-binary` | `RFC0017_TEST_INVENTORY_OK` | this commit |
| The shipped artifact composes every production host and no forbidden one | `verify_composed_hosts.py --profile production` | `required=29 forbidden=7` | this commit |
| ELF dependencies and idle runtime surface | `verify_binary_dependencies.py`, `verify_runtime_surface.py` | `BINARY_DEPENDENCIES_OK`, `RUNTIME_SURFACE_OK` | this commit |
| The shipped probe selects unavailable and sends nothing | `--self-test --public-preview-probe` | `COMPOSED:false`, `STAGE:unavailable`, three zeroes, `PUBLIC_PREVIEW_UNAVAILABLE_OK` | this commit |
| An enterprise host is refused with zero requests | the same probe with `--ghes-host ghe.example.invalid` | `STAGE:unavailable`, zero requests, no `_FAILED` verdict | this commit |
| The ledger's focused Linux matrix row | the `ctest --preset dev -R …` above | 19/19 in 4.45 s | this commit |
| The sanitizer course on a fresh isolated ASan/UBSan tree | `verify_rfc0017_sanitizer_course.py --build-root build/rfc0017-sanitize --fresh` | 4 selectors, `RFC0017_SANITIZER_COURSE_OK` | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
