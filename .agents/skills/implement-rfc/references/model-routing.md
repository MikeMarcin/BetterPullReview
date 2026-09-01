# Model Routing And Updates

`config/routing.json` is the single checked-in source for role-to-model assignment. The canonical `SKILL.md` and shared role definitions name semantic roles and capability classes, never vendor models.

## Capability Classes

- `mechanical`: exact, bounded, mechanically checkable work such as locating files and symbols, inventory, prescribed read-only command execution, schema or generated-file drift checks, and compact log condensation.
- `balanced`: semantic repository analysis, ownership tracing, build-selection analysis, and verification-surface discovery.
- `deep`: production implementation and ordinary independent closeout review where difficult multi-step reasoning and code judgment are expected.
- `deep-max`: optional exceptional review where maximum depth matters more than latency or token use.

## Current Checked-In Assignments

- `rfc-utility` -> `mechanical`;
- `rfc-scout` -> `balanced`;
- `rfc-worker` -> `deep`;
- `rfc-verifier` -> `deep`;
- `rfc-reviewer` -> `deep`;
- `rfc-critical-reviewer` -> `deep-max`.

The primary RFC integrator is not a generated child role; when the runtime exposes model choice, run it at the `deep` tier or stronger. The implementation worker and verifier are intentionally routed at the same deep tier as the ordinary reviewer. Issue-card slices and verification commands are bounded, but they commonly involve public contracts, ownership and lifetime, build or package selection, cross-module seams, or runtime behavior; bounded does not imply routine.

When independent review is selected, use `rfc-critical-reviewer` instead of `rfc-reviewer` when the changed risk warrants maximum-depth counterexample search. Do not run the ordinary and critical roles together by default; a second *role* is warranted only when the first review exposes a material unresolved disagreement or the user explicitly requests redundant review. A dual-family critical review — the same critical role on both provider routes, per Lane Runtime Selection below — is one review, not a redundant second one.

## Current Checked-In Routes

The routes were verified on 2026-08-02:

| Class | Codex | Claude |
|---|---|---|
| `mechanical` | GPT-5.6 Luna, max | Claude Haiku 4.5; no effort field because this model does not support effort |
| `balanced` | GPT-5.6 Terra, high | Claude Sonnet 5, high |
| `deep` | GPT-5.6 Sol, xhigh | Claude Opus 5, xhigh |
| `deep-max` | GPT-5.6 Sol, max | Claude Fable 5, max |

These are configuration defaults, not permanent architectural decisions.

## Lane Runtime Selection

The routes table gives every capability class both a Codex and a Claude
route. In a Claude Code primary session, native Claude agents are the
default lane mechanism; `scripts/codex_lane.py` launches a lane on the
Codex route instead (read [codex-lanes.md](codex-lanes.md) first). Choose
per lane, not per run:

- Prefer the Codex route for an *ordinary* closeout reviewer when the
  implementation was produced by Claude: cross-family review is more
  independent than any same-family fresh context.
- For a *critical* review from a Claude driver, run
  `rfc-critical-reviewer` on both deep-max routes — Claude Fable 5 at max
  natively, and GPT-5.6 Sol via the launcher (downgraded to xhigh by the
  companion cap) — then reconcile as the integrator: union the findings,
  reproduce every blocking finding directly, and resolve disagreements by
  evidence, not by vote. A Codex driver cannot launch Claude lanes; it runs
  its native critical reviewer alone and records the omitted cross-family
  lane per the standard fallback rule.
- Prefer the Codex route for mechanical `rfc-utility` bulk when allowance
  economics favor it; bulk token volume off the Claude budget preserves the
  integrator's context and quota.
- The primary integrator never runs through the launcher; it launches lanes
  only.

Record every independent review's runtime on the closeout card per the
Recording The Review Runtime section of
[codex-lanes.md](codex-lanes.md).

The Claude `deep-max` route was promoted to Claude Fable 5 on 2026-08-02 by owner decision, ahead of the establish-first evaluation below: the primary integrator runs on Opus 5, so a Fable critical reviewer is both a stronger and a different model than the implementation it audits. Two Fable-specific caveats: requests its safety classifiers flag (chiefly cybersecurity and biology domains) trigger automatic model fallback, which the reviewer's startup banner must surface as an effective-model change; and Fable availability on Bedrock is unverified, so confirm an approved inference profile exists there before trusting this route in that environment.

## Why Luna-Max Exists

Luna is reserved for work whose correctness is directly observable and whose output contract is narrow. Max effort is affordable enough under the current Codex allowance structure to improve diligence on mechanical work without spending Sol-class budget. The role must remain bounded: Luna must not decide architecture, implementation strategy, root cause, verification sufficiency, or RFC closeout.

Examples:

- locate exact files, symbols, presets, aliases, or instruction documents;
- inventory changed files, issue state, or generated adapter drift;
- run an exact read-only command list and report exit status and first failure;
- validate schemas or manifests;
- reduce a large log to failing command, test name, diagnostic location, and artifact path.

When a utility task becomes semantic—for example, deciding why a failure occurred or whether a public path proves an RFC claim—promote it to `rfc-scout` rather than stretching the Luna role.

## Critical Review Selection

Consider `rfc-critical-reviewer` when the user requests independent critical
review or the final diff materially affects one or more of:

- wire or protocol contracts;
- persistence or migration behavior;
- authority or security boundaries;
- concurrency primitives;
- destructive migrations.

Selection remains discretionary: use review only when it is likely to add
discriminating evidence. For other work, a primary-agent skeptical pass is
sufficient. A reviewer may block only on an evidence-backed failure of an
existing acceptance criterion or a critical regression in the changed surface.

## Claude Through Bedrock

The generated Claude agents always name stable Anthropic model IDs. Keep those same definitions at home and work. On Amazon Bedrock, map the IDs to approved inference-profile ARNs with Claude Code's native `modelOverrides` setting.

1. Open `config/claude-bedrock-model-overrides.example.json`.
2. Replace each example ARN with the approved organization inference profile.
3. Merge the `modelOverrides` object into the applicable Claude Code settings source:
   - organization-managed settings when the mapping is centrally governed;
   - `.claude/settings.local.json` for a developer-local mapping when policy permits it.
4. Keep account IDs and private deployment ARNs out of the portable skill and generated agent files.

`modelOverrides` lets Claude Code retain the canonical model family and capability identity while sending the provider-specific ARN to Bedrock. When managed `availableModels` policy is active, the organization may need to own the overrides because lower-precedence mappings can be ignored.

Unverified as of 2026-08-02: the `modelOverrides` setting could not be confirmed in official Claude Code documentation. Before relying on role-to-model routing through Bedrock, verify on the Bedrock-configured machine that each generated agent's startup banner reports the expected model; if the setting is ignored, route through organization-managed model policy instead.

Do not set `CLAUDE_CODE_SUBAGENT_MODEL` when role-specific routing is expected. That environment variable overrides every subagent definition's model.

## Rendering And Candidate Evaluation

Rendering injects each selected role, model, and effort into that native agent's mandatory first-line startup banner. A route with no supported effort renders as `none (unsupported)`. Regenerate the adapters whenever a route changes so the banner cannot drift from the native model configuration.

Render the checked-in assignments:

```sh
python .agents/skills/implement-rfc/scripts/render_runtime_adapters.py
```

For a candidate model mapping, create a small JSON overlay and render into a temporary repository root so the checked-in adapters remain untouched:

```sh
python .agents/skills/implement-rfc/scripts/render_runtime_adapters.py \
  --repo-root /tmp/implement-rfc-candidate \
  --routing-override /tmp/routing-candidate.json
```

## Updating Models

When a new model or effort level appears:

1. Add it to a candidate overlay or a branch of `config/routing.json`.
2. Render the native adapters into a temporary root.
3. Run the deterministic adapter tests and drift check.
4. Evaluate the candidate on representative historical RFC work:
   - exact locating, command execution, and log condensation for the mechanical lane;
   - ownership and execution-path discovery;
   - build/provider selection analysis;
   - bounded implementation with direct tests;
   - independent review of known overclaim or missing-verification cases.
5. Compare accepted findings, false positives, rework, elapsed time, and token cost against the incumbent.
6. Promote by changing only `config/routing.json`, update `verified_as_of`, render, validate, and commit the source and generated outputs together.
7. Retain the previous routing revision in version control for immediate rollback.

Do not promote a newly released model first into `rfc-worker`, `rfc-reviewer`, or `rfc-critical-reviewer`. Establish it on utility, scout, or shadow-review work before giving it implementation or closeout-critical responsibility.
