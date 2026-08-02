# Model Routing And Updates

`config/routing.json` is the single checked-in source for role-to-model assignment. The canonical `SKILL.md` and shared role definitions name semantic roles and capability classes, never vendor models.

## Capability Classes

- `mechanical`: exact, bounded, mechanically checkable work such as locating files and symbols, inventory, prescribed command execution, schema or generated-file drift checks, and compact log condensation.
- `balanced`: semantic repository analysis, ownership tracing, public-path and build-selection analysis, documentation-impact mapping, and verification-surface discovery.
- `deep`: production implementation and ordinary independent closeout review where difficult multi-step reasoning and code judgment are expected.
- `deep-max`: exceptional high-assurance review where maximum depth matters more than latency or token use.

## Current Checked-In Assignments

- `rfc-utility` -> `mechanical`;
- `rfc-scout` -> `balanced`;
- `rfc-worker` -> `deep`;
- `rfc-reviewer` -> `deep`;
- `rfc-critical-reviewer` -> `deep-max`.

The primary BetterPullReview RFC integrator is not a generated child role; when the runtime exposes model choice, run it at the `deep` tier or stronger. The implementation worker is intentionally routed at the same deep tier as the ordinary reviewer. BetterPullReview issue-card slices are bounded, but they commonly involve C++ ownership and lifetime, functional-core/host/Qt boundaries, GitHub/GHES authority, Git and SQLite recovery, diff identity, UI behavior, build selection, or public runtime paths; bounded does not imply routine.

Use `rfc-critical-reviewer` instead of `rfc-reviewer` for a critical closeout. Do not run both by default. A second review is warranted only when the first review exposes a material unresolved disagreement or the user explicitly requests redundant review.

## Current Checked-In Routes

The routes were verified on 2026-08-01:

| Class | Codex | Claude |
|---|---|---|
| `mechanical` | GPT-5.6 Luna, max | Claude Haiku 4.5; no effort field because this model does not support effort |
| `balanced` | GPT-5.6 Terra, high | Claude Sonnet 5, high |
| `deep` | GPT-5.6 Sol, xhigh | Claude Opus 5, xhigh |
| `deep-max` | GPT-5.6 Sol, max | Claude Opus 5, max |

These are configuration defaults, not permanent architectural decisions.

## Session Route Reporting

The runtime adapter renderer places the selected provider route into every
generated role prompt. Each newly spawned or restarted delegated session must
state its semantic role, model, and effort in its first outward status message
before any tool call or task work. When a runtime explicitly overrides a
configured route, report the effective value instead of silently repeating the
checked-in default. Use `runtime-not-exposed` when the effective value is not
available and `not-supported` when the model has no effort setting.

## Why Luna-Max Exists

Luna is reserved for work whose correctness is directly observable and whose output contract is narrow. Max effort is affordable enough under the current Codex allowance structure to improve diligence on mechanical work without spending Sol-class budget. The role must remain bounded: Luna must not decide architecture, implementation strategy, root cause, verification sufficiency, or RFC closeout.

Examples:

- locate exact files, symbols, CMake presets, issue cards, or instruction documents;
- inventory changed files, board state, `docs/issues/NEXT_ID`, or generated adapter drift;
- run an exact command list and report exit status and first failure;
- validate schemas, manifests, fixture inventories, or generated files;
- reduce a large log to failing command, test name, diagnostic location, and artifact path.

When a utility task becomes semantic—for example, deciding why a failure occurred or whether a public BetterPullReview path proves an RFC claim—promote it to `rfc-scout` rather than stretching the Luna role.

## Critical Review Selection

Use `rfc-critical-reviewer` when the final diff or acceptance claims materially affect one or more of:

- credentials, authentication, authorization, host/account/repository authority, or remote mutation safety;
- native HTTPS, Git smart transport, installed-process containment, or provider request contracts;
- Git object retention, SQLite persistence, durable recovery, file formats, schemas, migrations, or backward compatibility;
- concurrency, serialized decision lanes, request identity, stale-result rejection, ownership, cancellation, shutdown, or lifetime;
- widely consumed public C++ API or ABI compatibility with meaningful backward-compatibility obligations;
- destructive migration, hard cutover, cache deletion, irreversible data transformation, or credential cleanup;
- default provider/backend selection, package composition, signing/release behavior, or tests-disabled production seams;
- diff correspondence, line identity, comment anchoring, or other cross-subsystem paths whose failure can evade local helper tests;
- an RFC previously reopened because its earlier closeout overclaimed or relied on indirect evidence;
- a material disagreement between the primary agent and an independent reviewer or verifier.

For other substantial RFCs, use `rfc-reviewer` at the deep tier. Small, local, directly verified changes may use a primary-agent skeptical pass when an independent review would add little value; record that disposition in the closeout task.

## Claude Through Bedrock

The generated Claude agents always name stable Anthropic model IDs. Keep those same definitions at home and work. On Amazon Bedrock, map the IDs to approved inference-profile ARNs with Claude Code's native `modelOverrides` setting.

1. Open `config/claude-bedrock-model-overrides.example.json`.
2. Replace each example ARN with the approved organization inference profile.
3. Merge the `modelOverrides` object into the applicable Claude Code settings source:
   - organization-managed settings when the mapping is centrally governed;
   - `.claude/settings.local.json` for a developer-local mapping when policy permits it.
4. Keep account IDs and private deployment ARNs out of the portable skill and generated agent files.

`modelOverrides` lets Claude Code retain the canonical model family and capability identity while sending the provider-specific ARN to Bedrock. When managed `availableModels` policy is active, the organization may need to own the overrides because lower-precedence mappings can be ignored.

Do not set `CLAUDE_CODE_SUBAGENT_MODEL` when role-specific routing is expected. That environment variable overrides every subagent definition's model.

## Rendering And Candidate Evaluation

Render the checked-in assignments:

```sh
python .agents/skills/bpr-implement-rfc/scripts/render_runtime_adapters.py
```

For a candidate model mapping, create a small JSON overlay and render into a temporary repository root so the checked-in adapters remain untouched:

```sh
python .agents/skills/bpr-implement-rfc/scripts/render_runtime_adapters.py \
  --repo-root /tmp/bpr-agent-candidate \
  --routing-override /tmp/routing-candidate.json
```

## Updating Models

When a new model or effort level appears:

1. Add it to a candidate overlay or a branch of `config/routing.json`.
2. Render the native adapters into a temporary root.
3. Run the deterministic adapter tests and drift check.
4. Evaluate the candidate on representative historical BetterPullReview RFC work:
   - exact locating, command execution, and log condensation for the mechanical lane;
   - ownership and public execution-path discovery;
   - CMake/provider/composition selection analysis;
   - bounded implementation with direct tests;
   - independent review of known overclaim, authority, or missing-verification cases.
5. Compare accepted findings, false positives, rework, elapsed time, and token cost against the incumbent.
6. Promote by changing only `config/routing.json`, update `verified_as_of`, render, validate, and commit the source and generated outputs together.
7. Retain the previous routing revision in version control for immediate rollback.

Do not promote a newly released model first into `rfc-worker`, `rfc-reviewer`, or `rfc-critical-reviewer`. Establish it on utility, scout, or shadow-review work before giving it implementation or closeout-critical responsibility.
