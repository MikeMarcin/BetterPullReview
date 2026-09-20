# CLI read external fixture subset

The external driver is `tests/end_to_end/rfc0015_cli_read_fixture.py`.
It copies the existing sanitized github-com contract responses into a private
runtime directory, substitutes the exact commit identities in `objects.json`,
and supplies all nine existing github-com capability response parts. The pack
is the synthetic initial pack from `profile-window-restoration-v1`; it contains
two small source files at the base and head. No network or credentials are used
by the fake CLI. Unrecognized operations fail closed, including writes.

Run with Python 3.10 or newer. Prepare requires a new absolute directory:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/end_to_end/rfc0015_cli_read_fixture.py prepare --directory /Users/Shared/bpr-cli-read-new
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/end_to_end/rfc0015_cli_read_fixture.py launch --directory /Users/Shared/bpr-cli-read-new --app /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview --cmake-cache /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/CMakeCache.txt --seconds 180
```

The launcher supplies both HOME and CFFIXED_USER_HOME pointing to the
fresh absolute fixture home, and a PATH starting with the canonical fake
executable. No product auto-consent hook is passed. Use a path without a
symlink alias: the first `/private/tmp` course failed startup after macOS
returned a `/tmp` storage path. Before **every Connect**, external automation
must open Settings → Storage & privacy and verify that the visible local data
location lies inside that fixture home and that the directory exists. Do not
Connect when either check fails. HOME alone does not isolate macOS Qt storage.
The cache check verifies build config, not that the supplied binary was rebuilt
from the current source.

While it runs, external accessibility automation selects Accounts and hosts,
the GitHub CLI profile, Connect, and the visible read confirmation. After an
observed successful connection, enter `octo-org`, `review-repository`, and `17`
in the ordinary locator, then Open pull request. Record the actual full-window
render and structural `calls.jsonl` separately. The launcher waits and reaps
its child; the launcher enforces an external timeout even when the application ignores its run-budget argument. It never infers a rendered
PR from process success or a call log.

Prepare separate directories with `--course missing-process`,
`--course process-failure` and
`--course malformed-response` to test transport refusal. These inject failures
at the CLI boundary; they do **not** remove an adapter from the compiled graph,
and therefore do not satisfy the adapter-removal acceptance criterion.

The fake CLI accepts only the production `gh repo clone` command for
`https://github.com/octo-org/review-repository.git`, the known base or head OID,
and the exact closed flag list including empty templates and disabled hooks
and fsmonitor. Destinations must be new `clone-0.git` or `clone-1.git` directories
inside an owner-private canonical `/private/tmp/bpr-cli-git-*` root on macOS.
It uses local Git with only the file protocol enabled to fetch the exact commit
from prepared `repository.git`, then sets HEAD to that OID. No network, shell,
credential helper, or ambient Git configuration is used by this fixture.
The fixture is ready for a post-bridge tests-disabled render attempt; its unit
test and earlier metadata UI evidence do not establish full-diff acceptance.

Focused check:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/end_to_end/rfc0015_cli_read_fixture_tests.py
```

The `modified-missing-left` course uses the same 515-file sparse acquisition as
`large-positive`, but removes `src/file_0000.cpp` from only the exact selected
base tree while retaining provider status M. It reproduces comparison-basis
mismatch, not an empty blob. `src/file_0001.cpp` is the positive two-sided
recovery control. All paths and bytes are generated safe fixture content.

When the production sandbox denies trace-file writes, the fake CLI silently
skips its optional trace and continues. Successful version/API requests require
empty stderr, so a missing call log is not a zero-launch verdict. Keep the visible
UI observations primary; do not relax the sandbox for fixture logging.
