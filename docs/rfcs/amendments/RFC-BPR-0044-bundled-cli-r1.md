# Proposed amendment to RFC-BPR-0044: bundle `bpr` with the desktop app

Status: proposed, 2026-10-26. Source revision: `182e54a1b79b456149517ad946bb22f6c904ce3b`.

## Owner decision and reason

The owner wants the macOS desktop installation to include the CLI used by
`pr-tour`. Requiring a separate CLI acquisition makes the installed skill
unusable even when BetterPullReview is installed. A separate CLI distribution
can be added later if demand warrants it.

## Change to adopted scope

Amend [RFC-BPR-0044](../RFC-BPR-0044-headless-tour-runtime-and-terminal-cli.md),
"Decision and additive scope" and AC44-12: a macOS desktop release includes
`bpr` inside `BetterPullReview.app`. The CLI remains a terminal executable and
retains its headless runtime, core/application service reuse, profile and
credential boundaries, and command contract. The app does not shell out to it.
The release's standard installation must give a `pr-tour` agent a documented,
stable way to invoke the bundled executable without a source checkout or a
second package. Put it at `BetterPullReview.app/Contents/MacOS/bpr`. The first
release uses instructions rather than a desktop button or automatic shell
configuration. Explain that a terminal can use the absolute path directly or
run `export PATH="/Applications/BetterPullReview.app/Contents/MacOS:$PATH"`
after installing the app in `/Applications`. Document how to make that PATH
change persistent in the user's shell configuration, and how to substitute the
actual app location if installed elsewhere. Do not edit shell configuration or
create a symlink without an explicit user action.

The `pr-tour` skill must look for `bpr` on `PATH`, then the standard macOS app
location, and give a specific setup instruction if neither works. It must
invoke the located executable by absolute path for the rest of that session;
this works even when a GUI-launched agent did not inherit the user's shell PATH.

Replace AC44-12's separately installable package obligation with a bundled
macOS release qualification: inspect the installed app, invoke `bpr tour
capabilities --json` from its final bundle path, verify its runtime dependencies,
signing/notarization and installed notices, SPDX and corresponding-source
coverage. Test first-run profile and authentication from that path. Preserve
the existing platform safety obligations; do not infer Windows or Linux
qualification from the macOS result.

The separate CLI archive producer may remain as development machinery, but it
is no longer a prerequisite for the macOS `pr-tour` workflow or RFC closeout.
No standalone CLI release is promised by this amendment. A later standalone
distribution would need its own decision and qualification.

## Reconciliation

Affected live sources: RFC-BPR-0044 and AC44-12;
[the milestone plan](../plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json),
[BPR-1068](../../issues/20-ready/BPR-1068-qualify-standalone-packages-and-native-headless-safety.md),
[BPR-1070](../../issues/10-backlog/BPR-1070-document-the-workflow-and-simplify-the-integration.md),
the macOS release producer and verifier, `pr-tour` skill package, and CLI/user
documentation. Reconcile those through `implement-rfc`; retain existing passing
CLI semantics evidence where the executable and code path are unchanged.

Package the CLI in the app's executable area, include its runtime closure in
the same app, and extend release source/legal/security inventory and signature
verification before claiming a releasable artifact. Update the skill's setup
section and user-facing CLI guide to give the exact PATH and absolute-path
commands; its source-build instructions become developer fallback. Confirm
that the normal app install and relocated
bundle can run the CLI without Qt GUI/QPA initialization. Account for app
variants whose local automation is disabled: they cannot promise a working
`bpr` unless their build contract changes.

Existing publication approval, GitHub authority, headless behavior, and
cross-interface draft obligations are unchanged. No separate product choice is
needed to implement the owner's macOS direction.
