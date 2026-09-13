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

The Git operation is deliberately unavailable until the production bridge's
exact argv and disk-repository-to-Smart-v2 interface are specified. The local
`repository.git` has `refs/heads/main` and `refs/pull/17/head` ready for that
interface. No rendered-diff acceptance is claimed by this subset.

Focused check:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/end_to_end/rfc0015_cli_read_fixture_tests.py
```

When the production sandbox denies trace-file writes, the fake CLI emits
structural operation names on bounded stderr and continues. The app may discard
that stderr, so a missing call log is not a zero-launch verdict. Keep the visible
UI observations primary; do not relax the sandbox for fixture logging.
