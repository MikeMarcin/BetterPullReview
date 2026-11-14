---
schema: "implement-rfc.card.v1"
id: "BPR-2177"
title: "A fixture agent for every tests-on attended build: looks never spend the owner's agent usage"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2177--BPR-2177--3d53b996aaf5.json", "origin": "owner, 2026-11-14, after the ask-v001 look ran the installed Claude Code on every Ask"}
---

# BPR-2177: A fixture agent for every tests-on attended build

The owner, 2026-11-14:

> Jesus dude we need a fixture agent for testing why am I wasting a ton of money just seeing how buttons and UI flow works.

The ask-v001 look (session `6102c30f…`) was a tests-on build whose attended
launch searched the person's own agents (`--attended-launch` set
`AttendedAgentSearch`), so every Ask ran the installed Claude Code.

## Scope

- A scripted fixture agent, built with the tests: it speaks Claude's
  stream-JSON and Codex's JSONL, answers every built-in prompt and quick ask
  with canned, fixture-shaped answers (Was this addressed? claims, Explain this
  file, What changed, a free question, a commit message, findings), streams
  with small delays so running, Stop and the queue are visible, and can be
  scripted to fail or to hit a usage limit. No network, no cost.
- Whenever a fixture or any attended launch flag is in effect, agent launches
  go to the fixture agent only and can never reach a real agent program; the
  picker and footer name it "Fixture agent". A guard test proves a fixture or
  attended launch cannot start a real `claude` or `codex`. Only `freeze --live`
  builds may use real agents.
- `freeze_attended_build.py`'s `--check` verifies it for tests-on images.
- `AGENTS.md`, Parking work on the owner: looks never spend the owner's agent
  usage; tests-on looks use the fixture agent.

## Acceptance

- **AC01:** The fixture agent answers each of the listed prompts, streams
  visibly, and fails or hits a usage limit when scripted to, with no network.
- **AC02:** A fixture or attended launch's agent runs all reach the fixture
  agent; a guard test shows a real `claude` or `codex` on the search path and
  in a saved agent entry is never started; the picker says "Fixture agent".
- **AC03:** A tests-on freeze's `--check` fails for an image that could start
  a real agent program.

## Progress (2026-11-14)

- **AC01** passes: `fixture_agent_answers` runs the fixture agent
  (`tests/support/fixture_agent_main.cpp`, built with every tests-on build and
  copied as `claude` and `codex` into the build's `fixture-agent` folder) on
  What changed, Was this addressed?, Explain this file, a tour draft, an
  Enhance rewrite, a commit message, a review with findings and a free
  question, in Claude's stream-JSON and Codex's JSONL: each answers in kind, in
  steps with pauses, and a scripted crash and usage limit fail.
- **AC02** passes: `application_fixture_agent_guard_public_path` starts a
  person's session (`FixtureAgentSession`, which any `--attended…` launch sets;
  `--attended-launch` no longer searches the person's agents): a `claude` and
  a `codex` first on PATH that would leave a mark, `/opt/homebrew/bin/claude`
  and a wrapper are refused by the launch guard on the Agent tab's and the
  tour's launchers; a bare `claude` starts the fixture agent; the picker says
  "Fixture agent"; a question is answered by it; nothing marked.
- **AC03** passes: `freeze_attended_build_contract`: a launcher's `--check`
  fails an image without the fixture agent (`FIXTURE_AGENT_MISSING`), one
  whose executable does not name its folder (`FIXTURE_AGENT_UNNAMED`), and one
  that starts a `claude` from PATH (`REAL_AGENT_STARTED`).
- `AGENTS.md`, Parking work on the owner: looks never spend the owner's agent
  usage.

Not covered: the Review Cycles Send launcher and the local draft's launcher
have no guard of their own (their programs come from the same search, which a
person's session points at the fixture agent).
