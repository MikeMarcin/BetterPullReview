---
id: "RFC-BPR-0044"
title: "Headless Tour Runtime and Terminal CLI"
status: "Proposed"
area: "BPR"
milestone: "Tour CLI and pr-tour skill"
depends_on: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042"]
pack_version: "pr-tour-cli-extension-v1"
metadata: {"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# RFC-BPR-0044: Headless Tour Runtime and Terminal CLI

## Decision and additive scope

Ship `bpr tour` as an AI-free terminal frontend to the existing review-tour core and application services. Under the owner's [bundled-cli-r1 amendment](amendments/RFC-BPR-0044-bundled-cli-r1.md), macOS desktop releases with local automation include it at `BetterPullReview.app/Contents/MacOS/bpr`, with its runtime closure in the same app. It runs without starting the desktop or requiring a display server. Direct absolute-path invocation and optional user-managed PATH setup require neither a checkout nor another package; the first release adds no desktop button, shell edits or automatic symlink. The automation-off app variant omits it. Standalone archive machinery is development support, not a promised separate CLI release or macOS closeout prerequisite. The desktop calls shared C++ services directly; it does not shell out to its CLI. `pr-tour` is a later thin caller of the same CLI, not another tour implementation.

This RFC extends imported RFC-BPR-0039–0042. It does not replace their authoring model, ordinary GitHub publication format, exact source anchors, staged publication protocol, maintenance rules, or authority journal. Import identifiers may have been remapped: resolve the predecessor pack's actual mapping before editing code. The associated RFC-BPR-0045 provides shared work-in-progress (WIP) handoff. RFC-BPR-0046 provides the optional skill; neither an agent nor AI settings are prerequisites for manual terminal use.

The outcome is one tour engine with different presentations. Do not implement a Python/shell publisher beside the C++ publisher, an agent-only draft format, or a generic command server. A text editor and a line-oriented walkthrough are enough for the first terminal release. A full-screen TUI, terminal code editor, MCP mutation service, background daemon, hosted service, and cloud draft sync are out of scope.

## Existing source and runtime separation

The supplied root source has separate `BetterPullReview::Core` and `BetterPullReview::Application` libraries. However, `src/adapters/github/CMakeLists.txt` gates concrete transport code on `BPR_BUILD_QT`, despite linking only Qt Core/Network for those services. `src/app/CMakeLists.txt` composes the desktop and Qt adapter together. These are observed source facts, not evidence that a headless tour executable exists. Exact paths, dump offsets, and hashes are in [SOURCE-BASELINE](assets/bpr-pr-tour-cli-extension-v1/references/SOURCE-BASELINE.json).

Separate service availability from desktop presentation. Reuse Qt Core/Network and `QCoreApplication` where they already implement the event loop, HTTPS, or process work; do not replace them simply to claim zero Qt dependencies. The CLI must neither link/load Qt Widgets, Qt GUI/QPA plugins, WebEngine, syntax-presentation resources, nor create a hidden/offscreen `QApplication`. Native vault dependencies and admitted process helpers remain when required. Qt's non-GUI event-loop role is documented in external reference QT-CORE; using it here is a design decision.

Add a CLI build/install option and dependency closure, preserving the current desktop and core-only presets. Candidate targets are `BetterPullReview::TourRuntime` and `bpr`, but live naming conventions decide concrete names. Do not collect unrelated Home, release-update, tray, mascot, or window startup services in the headless target. Keep the functional core free of Qt and effect types. Any new dependency must follow the existing package/provider and corresponding-source/notices policy.

## Human workflow

The supported everyday flow is deliberately small:

```sh
bpr tour open 123
bpr tour edit problem
bpr tour edit solution
bpr tour add src/data_store.cpp:148-169
bpr tour walk
bpr tour preview
bpr tour publish
```

A number is accepted only with an unambiguous repository/host context. A full PR URL works outside a checkout; use the existing read/epoch acquisition path rather than require a clone. `open` loads published material and resumes the matching local draft; it never changes the PR's draft status or overwrites unsent work. A cold open without a published tour initializes a local draft without creating GitHub comments.

Problem precedes Solution. `edit` opens the user's explicitly configured Markdown editor, using a private temporary projection of one field; `--file` or stdin provides a noninteractive alternative. `add` captures a validated source destination and lets the author edit the title/comment. Original commit/side/path/line semantics remain explicit; a current worktree path is not proof of the PR's source revision. Left-side and historical destinations require explicit selection. Dirty worktrees never silently substitute for pinned source.

`walk` shows one stop's title, exact source context/diff, current comment, and unresolved limitations, then offers Edit, Ready and next, Previous, Move, Remove from tour, and Quit. It may use a configured pager; it does not start AI. Keyboard and line-input interaction work at narrow widths and without color. Readiness follows the same current-content decision as the desktop. Source retrieval or printing JSON alone does not assert a human visit.

Use task verbs for move/remove/undo/redo and reuse the parent's post-publication semantics: removing a stop from the route does not delete its thread; moving its order does not retarget its GitHub subject. `publish` also handles explicit maintenance of an existing tour; it computes and previews only the required operations.

CLI-only first run must work without a pre-existing desktop profile. Reuse the existing local profile initializer and expose a narrow explicit `bpr tour connect --host HOST --transport native|gh` path for current admitted authentication services. Opening a PR can initialize local defaults but must not sign in, switch accounts, or open a browser automatically. An unsupported headless credential environment produces a precise setup/refusal action, not a hidden GUI dependency. This is a frontend for existing authentication, not a replacement credential flow.

## Command contract

[CLI-CONTRACT](assets/bpr-pr-tour-cli-extension-v1/specs/CLI-CONTRACT.md) and [command-catalog.json](assets/bpr-pr-tour-cli-extension-v1/specs/command-catalog.json) define the new interface. Human output is the default. JSON is emitted only for explicit `--json`; progress and warnings belong on stderr, never interleaved into machine stdout. One invocation produces one bounded result envelope with an operation identity and typed outcome. The initial machine schema is versioned and discoverable. Unsupported versions/options fail without effects.

The skill always passes the concrete profile/draft or published root returned by `open`; it never depends on a mutable global last-opened PR. Human convenience selection is scoped to an unambiguous terminal/worktree context. Where two drafts or roots are possible, ask the human or return `target_ambiguous` in noninteractive mode. Never choose a different account or PR because a desktop tab gained focus.

Machine edits require expected document or structure revisions and carry text on stdin or in explicitly named files, not command-line prose. GitHub numeric identifiers are losslessly encoded as decimal strings in the CLI/IPC contract; do not round-trip them through JSON doubles. Core strong IDs remain unchanged. Raw text ranges are half-open UTF-8 byte intervals, checked at Unicode scalar boundaries; source line ranges remain one-based inclusive and are a different type.

A process without an interactive terminal must not open an editor/pager, read an unexpected prompt, launch a browser, or infer acceptance from EOF. Return `interaction_required` with the next useful action. External editor launch is a narrow new user-initiated process capability: approved executable plus argument vector, no shell evaluation or repository-selected executable. See the editor/temp and terminal escape rules in [SECURITY-AND-AUTHORITY](assets/bpr-pr-tour-cli-extension-v1/supporting/SECURITY-AND-AUTHORITY.md).

## Publication and authority

`preview` and default `recover` perform reads/local planning only. No opening, editing, walking, readiness decision, local save, import, hook inspection, or assistance completion may create a pending review, modify the PR body, or submit comments.

`publish` uses the same bounded immutable parent plan and one-use child grants as RFC-BPR-0041. It shows host, repository, PR, acting account, expected revision, comment destinations/body changes, optional PR-body update, and native versus compatibility visibility boundaries. One explicit confirmation can authorize the finite plan; it must not become a general session-wide write lease. The implementation performs link staging, verification, submission, and maintenance through the existing host/provider/journal, never by bypassing it with an arbitrary `gh` command.

Interactive terminal confirmation and a supported agent-host approval bridge are presentations of the same exact-plan authorization. JSON, `ready`, a prose claim of approval, or an arbitrary file containing a plan ID is not a live grant. A noninteractive caller without a supported approval bridge gets a preview and `interaction_required`; the author can execute the displayed ordinary `bpr tour publish --draft …` command directly. Do not add an unscoped `--yes` workaround. An approval bridge is optional in this first release; its absence must not block preparation, human-terminal publication, or skill usefulness.

CLI transport uses the already admitted fixed `gh api` operations where supported; native HTTPS remains available through the same policy. `gh pr review` is not a substitute for structured pending batches. Do not remove platform/account/credential/process constraints to obtain CLI parity. Unavailable vaults, approved hosts, or qualified transports produce actionable typed diagnostics, not token extraction or silent personal-provider fallback.

Cancellation after a possible send reports an uncertain or partial outcome. Read-first recovery reuses confirmed comment IDs and operations. No retry after an ambiguous timeout without reconciliation and fresh authorization for remaining writes. Changing interfaces or machines does not recreate a live grant. The residual GitHub concurrent-writer races and non-atomic maintenance boundaries remain exactly as stated in the parent pack.

## Acceptance criteria

- **AC44-01:** Tests-disabled `bpr` installs and runs without desktop/GUI/display dependencies; desktop and core-only builds remain valid.
- **AC44-02:** Desktop and CLI exercise the same tour services, parser, publisher, journal, and normalized result semantics; no duplicate publisher or GUI child process exists.
- **AC44-03:** A human can open, write Problem/Solution, add/reorder/walk/complete stops, preview, and publish without an AI provider or GUI.
- **AC44-04:** Human/JSON outputs, typed errors, paging, exact identifiers, cancellation, and noninteractive refusal obey CLI-CONTRACT.
- **AC44-05:** Every command binds an explicit or unambiguous profile/account/PR/draft/revision; concurrent agent sessions cannot redirect each other through current selection.
- **AC44-06:** Source destinations are pinned and distinguish prose byte ranges from source line ranges, including historical/left/unchanged fallback targets.
- **AC44-07:** Editor/stdin/file operations preserve text, use exact revision checks, release ownership while awaiting an editor, and do not execute repository instructions.
- **AC44-08:** Preview/recover/readiness/local editing make zero GitHub writes; publication uses the exact finite existing plan with explicit confirmation and no unscoped noninteractive bypass.
- **AC44-09:** Native and admitted CLI transports retain host/platform qualification and credential boundaries; unsupported paths refuse honestly.
- **AC44-10:** Killed/interrupted terminal publication and maintenance reconcile known identities without duplicate sends or automatic resume.
- **AC44-11:** Terminal rendering neutralizes untrusted control/escape sequences, reports truncation, and remains usable with no color, narrow width, and Unicode paths.
- **AC44-12:** A macOS desktop release with local automation includes `Contents/MacOS/bpr` and its runtime closure. Inspect the installed app and run `bpr tour capabilities --json` from that final path; qualify dependencies, signing/notarization, installed notices, SPDX, corresponding source and first-run profile/authentication there. Document stable absolute-path and optional persistent PATH setup, and verify normal and relocated bundles remain headless. The `pr-tour` skill locates PATH first, then the standard macOS app, and retains one absolute executable path. Preserve separately recorded Windows/Linux safety and qualification obligations; macOS evidence does not qualify them.

## Delivery

Deliver read/local authoring before remote terminal operations and before skill assistance. Existing tour dependencies are implementation prerequisites, not permission to re-import their cards or replay every predecessor's closeout. Tests use the parent's isolated fixtures and existing production seams. Actual native/link/package and designated GitHub/GHES evidence are required for corresponding shipping claims. A fixture or this pack's validator is not that evidence.
