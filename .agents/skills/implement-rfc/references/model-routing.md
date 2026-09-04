# Model Routing And Updates

`config/routing.json` is the single checked-in source for role-to-model assignment. The canonical `SKILL.md` and shared role definitions name semantic roles and capability classes, never vendor models.

## Capability Classes And Assignments

| Role | Class | Codex | Claude |
|---|---|---|---|
| `rfc-utility` | `mechanical` | GPT-5.6 Luna, xhigh | Claude Haiku 4.5, effort unsupported |
| `rfc-test-runner` | `mechanical` | GPT-5.6 Luna, xhigh | Claude Haiku 4.5, effort unsupported |
| `rfc-scout` | `balanced` | GPT-5.6 Sol, low | Claude Opus 5, low |
| `rfc-worker-routine` | `standard` | GPT-5.6 Sol, high | Claude Opus 5, high |
| `rfc-worker` | `deep` | GPT-6 Astra, high | Claude Fable 5.1, high |
| `rfc-reviewer` | `deep` | GPT-6 Astra, high | Claude Fable 5.1, high |
| `rfc-critical-reviewer` | `critical` | GPT-6 Astra, xhigh | Claude Fable 5.1, xhigh |

- `mechanical`: exact locating, inventories, prescribed command execution,
  schema/drift checks, and log condensation with directly checkable outputs.
- `balanced`: bounded semantic repository analysis, ownership and execution-path
  tracing, verification-surface discovery, and failure investigation.
- `standard`: implementation using an established approach, an existing pattern,
  exclusive ownership, and direct verification. Sensitive protocol, persistence,
  authority, concurrency, lifetime, and destructive-migration changes use `deep`.
- `deep`: implementation and independent review requiring substantial code and
  contract judgment.
- `critical`: deeper counterexample analysis focused on a named material risk.

The primary integrator is not a generated child role. Recommend the `deep`
route (Astra high or Fable 5.1 high) when choosing a primary model, but preserve
user-selected active session settings. These are defaults, not permanent
architectural decisions. No default uses `max`. Haiku's omitted effort field
means effort is unsupported; it does not establish that thinking is disabled.

## Routing Decision And Evidence

Research and owner approval: **2026-09-04**. `verified_as_of` records this routing
review date, not a successful local model-quality evaluation. This update is an
owner-approved promotion informed by public measurements and experience; no
project task bakeoff was performed.

- **Flagships and effort:** use Astra/Fable high for difficult implementation and
  review, and xhigh for critical review. Public measurements support examining
  cost per completed task rather than always selecting maximum effort; they do
  not establish the best effort on every project slice. See
  [Astra evaluation](https://artificialanalysis.ai/articles/benchmarking-gpt-6-astra),
  [Fable 5.1 evaluation](https://artificialanalysis.ai/articles/claude-fable-5-1), and
  [Anthropic's cost/intelligence guidance](https://platform.claude.com/docs/en/about-claude/models/optimizing-for-cost-and-intelligence).
- **Scouts:** the researched Artificial Analysis Codex-agent results gave Sol low
  and Terra high the same coding index (55), with Sol low taking 3.5 versus 6.0
  minutes at $1.29 versus $1.14 per task. CursorBench 3.2 gave Opus 5 low 62.8%
  versus Sonnet 5 high 56.9%, at $2.55 versus $2.13 per task and 37 versus 57
  steps. These support our speed/capability tradeoff; steps are not elapsed time
  and the two harnesses must not be combined into one ranking. Sources:
  [Artificial Analysis coding agents](https://artificialanalysis.ai/agents/coding-agents/comparisons/claude-code-vs-codex),
  [CursorBench](https://cursor.com/evals).
- **Routine workers:** Sol high and Opus high retain capable implementation
  routes for known approaches. The owner reports Fable often completes difficult
  work in one attempt where Opus needs repeated turns; this is user experience,
  not a controlled benchmark or a claim that Opus loses on every task.
- **Mechanical roles:** Luna xhigh reflects the owner's preference to spend its
  inexpensive reasoning budget on diligence. Haiku remains appropriate only for
  prescribed, checkable work. Its $1/$5 input/output price per million tokens is
  half Sonnet 5's $2/$10; that is API pricing, not subscription allowance or
  guaranteed cost per completed task. See
  [Anthropic pricing](https://platform.claude.com/docs/en/about-claude/pricing) and
  [Haiku capabilities](https://platform.claude.com/docs/en/models/haiku-4-5/overview).

Public results are dated, harness-dependent evidence. The role boundaries and
selected defaults are engineering judgments. Watch actual rework, accepted
findings, false positives, elapsed time, and usage when revisiting them.

## Lane Runtime Selection

Each class has both a Codex and a Claude route. In a Claude Code primary
session, native Claude agents are the default lane mechanism;
`scripts/codex_lane.py` selects the same canonical role's Codex route instead
(read [codex-lanes.md](codex-lanes.md) first). Choose per lane:

- Use one independent reviewer when useful, preferring the opposite model family
  from the implementation when the driver can launch it. Different families can
  add diversity; that is not a guarantee of independent errors.
- Use the critical role instead of the ordinary reviewer for a named material
  risk. A second review, including another provider running the same role,
  requires an unresolved material concern or an explicit request. Reconcile
  findings using direct evidence, not a vote.
- Prefer Codex for mechanical bulk when allowance economics favor it.
- The primary integrator never runs through the launcher; it launches lanes only.

A driver unable to launch the other family uses its native reviewer and records
that limitation. Record each review's runtime per [codex-lanes.md](codex-lanes.md).
If the runtime exposes a fallback model or effort, report its effective value
in the startup banner. Bedrock availability remains environment-specific; confirm
approved inference profiles before relying on those routes there.

## Mechanical Work Boundaries

Utility handles read-only mechanical work. The test runner can also write
assigned disposable build/test outputs, but has no acceptance authority.
Neither chooses architecture, implementation strategy, root cause, or
verification sufficiency. Run trivial deterministic commands directly rather
than spawning an agent. Workers normally run their own focused checks; a
separate test runner is optional when it materially helps execution.

Send failure diagnosis to a scout. Send questions about whether tests can pass
while an RFC claim remains false to a reviewer or the primary. The primary
selects sufficient verification and adjudicates all acceptance decisions.

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
6. Promote through `config/routing.json`; update role contracts and schemas too when responsibilities or classes change. Update `verified_as_of`, render, validate, and commit the source and generated outputs together.
7. Retain the previous routing revision in version control for immediate rollback.

Absent an explicit owner-directed promotion, establish a newly released model on representative utility, scout, or shadow-review work before assigning implementation or closeout-critical responsibility. An owner-directed promotion can proceed on documented public evidence and judgment; state which local evaluations were not performed.
